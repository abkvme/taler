// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <useragent.h>

#include <clientversion.h>
#include <net.h>
#include <scheduler.h>
#include <sidecar.h>
#include <sync.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utiltime.h>
#include <validation.h>

namespace useragent {
namespace {

CCriticalSection cs_useragent;
RunMode g_run_mode GUARDED_BY(cs_useragent) = RunMode::Server;
std::vector<std::string> g_operator_comments GUARDED_BY(cs_useragent);

// --- connection refresh -------------------------------------------------
// See the contract on StartConnectionRefreshTask() in useragent.h; the numbers
// are the mitigations, so they are worth reading together with it.

//! The agent must hold still this long before anything is dropped, so a sidecar
//! that crash-loops cannot turn into a disconnect loop.
const int64_t DEBOUNCE_SECONDS = 15;
//! Ceiling on how often a pass may start. Each mass reconnect is a fresh draw of
//! peers, and letting anyone who can restart the sidecar force those on demand
//! would hand them repeated attempts at surrounding the node.
const int64_t MIN_SECONDS_BETWEEN_CYCLES = 600;
//! One peer at a time, this far apart.
const int64_t STAGGER_SECONDS = 3;
//! Never drop below this many peers, whatever else is true.
const size_t MIN_CONNECTIONS_KEPT = 3;

CCriticalSection cs_refresh;
//! When the agent last actually changed, and 0 once that change has been served.
int64_t g_changed_at GUARDED_BY(cs_refresh) = 0;
//! Connections older than this are stale. Non-zero only while a pass is running.
int64_t g_cycle_cutoff GUARDED_BY(cs_refresh) = 0;
int64_t g_last_cycle_finished GUARDED_BY(cs_refresh) = 0;
//! Whether the pass in progress has actually dropped anything. A pass that finds
//! nothing stale cost the node nothing, so it must not spend the cooldown - the
//! first change after start-up would otherwise be locked out for ten minutes by
//! the pass that served the start-up composition and had no work to do.
bool g_cycle_dropped_any GUARDED_BY(cs_refresh) = false;

void NoteAgentChanged()
{
    LOCK(cs_refresh);
    g_changed_at = GetTime();
}

void RefreshTick()
{
    if (!g_connman) return;

    int64_t cutoff;
    {
        LOCK(cs_refresh);
        const int64_t now = GetTime();

        if (g_cycle_cutoff == 0) {
            // Idle. Should a pass start?
            if (g_changed_at == 0) return;
            if (now - g_changed_at < DEBOUNCE_SECONDS) return;
            // Losing peers mid-sync costs real progress, and the agent matters
            // least to a node that is not yet useful to anyone.
            if (IsInitialBlockDownload()) return;
            if (g_last_cycle_finished != 0 &&
                now - g_last_cycle_finished < MIN_SECONDS_BETWEEN_CYCLES) {
                return;
            }
            g_cycle_cutoff = g_changed_at;
            g_changed_at = 0;
            g_cycle_dropped_any = false;
            LogPrint(BCLog::NET, "useragent: refreshing connections so peers learn %s\n",
                     GetSubVersion());
        } else if (g_changed_at != 0 && g_changed_at > g_cycle_cutoff) {
            // Changed again mid-pass: adopt the newer cutoff rather than finish
            // handing out a string that is already out of date.
            g_cycle_cutoff = g_changed_at;
            g_changed_at = 0;
        }
        cutoff = g_cycle_cutoff;
    }

    // One stale peer per tick. "Stale" is judged against the moment the agent
    // changed, so a peer that has already reconnected is never dropped again and
    // the pass is guaranteed to end.
    std::vector<CNodeStats> stats;
    g_connman->GetNodeStats(stats);
    if (stats.size() <= MIN_CONNECTIONS_KEPT) {
        // Too few to spend one. Leave the pass open; it will run when there are
        // more, and the agent still reaches every new connection meanwhile.
        return;
    }

    for (const CNodeStats& peer : stats) {
        if (peer.nTimeConnected >= cutoff) continue;
        if (g_connman->DisconnectNode(peer.nodeid)) {
            LOCK(cs_refresh);
            g_cycle_dropped_any = true;
            LogPrint(BCLog::NET, "useragent: dropped %s (%s, connected %is before the change) to refresh its copy of our version\n",
                     peer.addrName, peer.fInbound ? "inbound" : "outbound",
                     cutoff - peer.nTimeConnected);
        }
        return;
    }

    // Nothing stale left.
    LOCK(cs_refresh);
    g_cycle_cutoff = 0;
    if (g_cycle_dropped_any) {
        // Only a pass that actually cost the node peers starts the cooldown.
        g_last_cycle_finished = GetTime();
        LogPrint(BCLog::NET, "useragent: all peers now have %s\n", GetSubVersion());
    }
}

} // namespace

void SetRunMode(RunMode mode)
{
    LOCK(cs_useragent);
    g_run_mode = mode;
}

RunMode GetRunMode()
{
    LOCK(cs_useragent);
    return g_run_mode;
}

std::string RunModeTag()
{
    return GetRunMode() == RunMode::Gui ? "GUI" : "SERV";
}

void SetOperatorComments(std::vector<std::string> comments)
{
    LOCK(cs_useragent);
    g_operator_comments = std::move(comments);
}

bool Rebuild(std::string* error)
{
    std::vector<std::string> comments;
    {
        LOCK(cs_useragent);
        comments.push_back(g_run_mode == RunMode::Gui ? "GUI" : "SERV");
    }

    // Only while a sidecar is registered. Its version was sanitised when it
    // registered, so nothing unsafe can reach the wire from here.
    const sidecar::Registration registration = sidecar::Current();
    if (registration.registered) {
        comments.push_back(registration.name + ":" + registration.version);
    }

    {
        LOCK(cs_useragent);
        comments.insert(comments.end(), g_operator_comments.begin(), g_operator_comments.end());
    }

    const std::string composed = FormatSubVersion(CLIENT_NAME, CLIENT_VERSION, comments);
    if (composed.size() > MAX_SUBVERSION_LENGTH) {
        // Refuse rather than truncate. A cut-off agent loses its closing "/" and
        // every parser downstream reads the remains as part of the version.
        if (error) {
            *error = strprintf("the resulting network version string would be %i bytes, over the %i-byte limit",
                               composed.size(), MAX_SUBVERSION_LENGTH);
        }
        return false;
    }

    // Only a real change is worth dropping peers over. The very first
    // composition, during start-up, is not one: nothing is connected yet, so
    // there is no peer holding an earlier string.
    const std::string previous = GetSubVersion();
    if (composed != previous) {
        SetSubVersion(composed);
        if (!previous.empty()) NoteAgentChanged();
    }
    return true;
}

void StartConnectionRefreshTask(CScheduler& scheduler)
{
    scheduler.scheduleEvery(&RefreshTick, STAGGER_SECONDS * 1000);
}

std::string Get()
{
    return GetSubVersion();
}

} // namespace useragent
