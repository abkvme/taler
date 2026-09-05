// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <useragent.h>

#include <clientversion.h>
#include <net.h>
#include <sidecar.h>
#include <sync.h>
#include <utilstrencodings.h>

namespace useragent {
namespace {

CCriticalSection cs_useragent;
RunMode g_run_mode GUARDED_BY(cs_useragent) = RunMode::Server;
std::vector<std::string> g_operator_comments GUARDED_BY(cs_useragent);

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

    SetSubVersion(composed);
    return true;
}

std::string Get()
{
    return GetSubVersion();
}

} // namespace useragent
