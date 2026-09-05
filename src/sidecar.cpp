// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sidecar.h>

#include <random.h>
#include <scheduler.h>
#include <sync.h>
#include <useragent.h>
#include <util.h>
#include <utilstrencodings.h>
#include <utiltime.h>

namespace sidecar {
namespace {

CCriticalSection cs_sidecar;
Registration g_registration GUARDED_BY(cs_sidecar);

std::string NewToken()
{
    // 128 bits: this only has to stop an accident or a stray script, not a
    // determined attacker - anyone with RPC credentials already owns the wallet.
    uint256 bytes = GetRandHash();
    return bytes.GetHex().substr(0, 32);
}

//! Is this safe to put in the user agent, as one comment among several?
//!
//! Stricter than SAFE_CHARS_UA_COMMENT on purpose. BIP-14's comment set permits
//! ';' and ' ', and ';' is the separator *between* comments - so a version of
//! "1.0;GUI" would pass a plain BIP-14 check and then split into two comments on
//! every peer that parsed it, letting a sidecar forge the shape of what this node
//! broadcasts. Restricting to letters, digits, '.', '-' and '_' leaves every
//! ordinary version string usable ("1.2.0", "1.2.0-rc.1", "v2_1") and nothing
//! that can be mistaken for structure. Semver build metadata ("1.0.0+abc") is
//! not accepted; '+' is outside BIP-14's set anyway.
bool IsSafeVersionToken(const std::string& text)
{
    return !text.empty() && text == SanitizeString(text, SAFE_CHARS_FILENAME);
}

} // namespace

Registration Current()
{
    LOCK(cs_sidecar);
    return g_registration;
}

bool Register(const std::string& name, const std::string& version, int64_t timeout,
              const std::string& supplied_token, Registration& out, std::string& error)
{
    if (name.size() > MAX_NAME_LENGTH) {
        error = strprintf("name is longer than %u characters", MAX_NAME_LENGTH);
        return false;
    }
    if (version.size() > MAX_VERSION_LENGTH) {
        error = strprintf("version is longer than %u characters", MAX_VERSION_LENGTH);
        return false;
    }
    // These two go straight into what the node broadcasts, so they are checked
    // rather than cleaned: silently rewriting a sidecar's version would leave it
    // reporting one thing and the node another.
    if (!IsSafeVersionToken(name)) {
        error = "name may contain only letters, digits, '.', '-' and '_' - it is not allowed to look like structure in the network version string";
        return false;
    }
    if (!IsSafeVersionToken(version)) {
        error = "version may contain only letters, digits, '.', '-' and '_' - it is not allowed to look like structure in the network version string";
        return false;
    }
    if (timeout < MIN_TIMEOUT || timeout > MAX_TIMEOUT) {
        error = strprintf("timeout must be between %i and %i seconds", MIN_TIMEOUT, MAX_TIMEOUT);
        return false;
    }

    Registration previous;
    {
        LOCK(cs_sidecar);
        previous = g_registration;

        // One slot. Re-registering with the right token is how a sidecar reports
        // a new version after upgrading itself, so that is an update, not a clash.
        if (previous.registered && supplied_token != previous.token) {
            error = strprintf("another sidecar is already registered (%s:%s); deregister it first",
                              previous.name, previous.version);
            return false;
        }

        const int64_t now = GetTime();
        g_registration.registered = true;
        g_registration.name = name;
        g_registration.version = version;
        g_registration.registered_at = previous.registered ? previous.registered_at : now;
        g_registration.last_heartbeat = now;
        g_registration.timeout = timeout;
        g_registration.token = previous.registered ? previous.token : NewToken();
        out = g_registration;
    }

    // Publish it. If the agent will not fit, put the registry back as it was -
    // reporting a sidecar the peers were never told about would be worse than
    // refusing outright.
    std::string agent_error;
    if (!useragent::Rebuild(&agent_error)) {
        {
            LOCK(cs_sidecar);
            g_registration = previous;
        }
        useragent::Rebuild();
        error = agent_error + " - reduce -uacomment, or use a shorter sidecar version";
        return false;
    }

    LogPrintf("sidecar: registered %s:%s, timeout %is\n", name, version, timeout);
    return true;
}

bool Heartbeat(const std::string& token, Registration& out, std::string& error)
{
    LOCK(cs_sidecar);
    if (!g_registration.registered) {
        error = "no sidecar is registered";
        return false;
    }
    if (token != g_registration.token) {
        error = "token does not match the registered sidecar";
        return false;
    }
    g_registration.last_heartbeat = GetTime();
    out = g_registration;
    return true;
}

bool Deregister(const std::string& token, std::string& error)
{
    std::string name, version;
    {
        LOCK(cs_sidecar);
        // Nothing registered is the state the caller wanted, so this succeeds.
        if (!g_registration.registered) return true;
        if (token != g_registration.token) {
            error = "token does not match the registered sidecar";
            return false;
        }
        name = g_registration.name;
        version = g_registration.version;
        g_registration = Registration();
    }
    useragent::Rebuild();
    LogPrintf("sidecar: deregistered %s:%s\n", name, version);
    return true;
}

void ExpireIfStale()
{
    std::string name, version;
    int64_t silent_for = 0;
    {
        LOCK(cs_sidecar);
        if (!g_registration.registered) return;
        const int64_t now = GetTime();
        if (now - g_registration.last_heartbeat < g_registration.timeout) return;
        name = g_registration.name;
        version = g_registration.version;
        silent_for = now - g_registration.last_heartbeat;
        g_registration = Registration();
    }
    useragent::Rebuild();
    LogPrintf("sidecar: %s:%s stopped answering %is ago, registration dropped\n",
              name, version, silent_for);
}

void StartExpiryTask(CScheduler& scheduler)
{
    scheduler.scheduleEvery(&ExpireIfStale, EXPIRY_CHECK_SECONDS * 1000);
}

} // namespace sidecar
