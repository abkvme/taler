// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SIDECAR_H
#define BITCOIN_SIDECAR_H

#include <cstdint>
#include <string>

class CScheduler;

/**
 * Registry for an API sidecar running alongside this node.
 *
 * The sidecar is a separate process that speaks to the node over RPC. It tells
 * the node it is there, the node includes that in its BIP-14 user agent, and the
 * registration lapses on its own if the sidecar stops calling - so a sidecar
 * that crashes does not leave the node advertising an API nobody is serving.
 *
 * Deliberately not persisted. A node that restarts is, from the sidecar's point
 * of view, a different node: it has to register again, and until it does the
 * node correctly reports no API.
 *
 * At most one sidecar at a time. A token issued at registration guards heartbeat
 * and deregistration, so a stray process holding RPC credentials cannot silently
 * displace or retire a live registration.
 */
namespace sidecar {

//! Seconds without a heartbeat before the registration lapses.
static const int64_t DEFAULT_TIMEOUT = 90;
static const int64_t MIN_TIMEOUT = 10;
static const int64_t MAX_TIMEOUT = 3600;

//! How often the expiry sweep runs. Nothing here is urgent, but the lapse should
//! land near the moment it was promised.
static const int64_t EXPIRY_CHECK_SECONDS = 15;

//! Caps on what a sidecar may put in the user agent. Generous for a semver, and
//! small enough that no registration can crowd out the operator's -uacomment.
static const size_t MAX_NAME_LENGTH = 16;
static const size_t MAX_VERSION_LENGTH = 32;

struct Registration
{
    bool registered = false;
    std::string name;
    std::string version;
    int64_t registered_at = 0;
    int64_t last_heartbeat = 0;
    int64_t timeout = 0;
    //! Never leaves the node except in the reply to sidecarregister.
    std::string token;
};

/** A snapshot of the current registration. Safe from any thread. */
Registration Current();

/**
 * Register, or update an existing registration when the token matches.
 *
 * `name` and `version` must survive SanitizeString(SAFE_CHARS_UA_COMMENT)
 * unchanged: they are broadcast to peers, and a sidecar that could smuggle a
 * ';' or ')' through here would be forging the shape of this node's user agent.
 *
 * Fails, leaving any existing registration untouched, when the arguments are
 * unusable, when a different sidecar holds the slot, or when the resulting user
 * agent would not fit.
 */
bool Register(const std::string& name, const std::string& version, int64_t timeout,
              const std::string& supplied_token, Registration& out, std::string& error);

/** Push the expiry out. Fails if nothing is registered or the token is wrong. */
bool Heartbeat(const std::string& token, Registration& out, std::string& error);

/**
 * Clear the registration.
 *
 * Idempotent: deregistering when nothing is registered succeeds, so a sidecar's
 * shutdown path never has to know whether it had already lapsed.
 */
bool Deregister(const std::string& token, std::string& error);

/** Drop the registration if the heartbeat is overdue. Called by the scheduler. */
void ExpireIfStale();

/** Arm the periodic expiry sweep. */
void StartExpiryTask(CScheduler& scheduler);

} // namespace sidecar

#endif // BITCOIN_SIDECAR_H
