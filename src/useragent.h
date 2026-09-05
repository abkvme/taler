// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_USERAGENT_H
#define BITCOIN_USERAGENT_H

#include <string>
#include <vector>

class CScheduler;

/**
 * The BIP-14 user agent this node reports to its peers.
 *
 * Composed from four things, in this order inside the comment list:
 *
 *   1. the run mode, GUI or SERV
 *   2. the registered API sidecar, as api:<version>, while one is registered
 *   3. whatever the operator passed in -uacomment
 *
 * giving, for example, /Taler:0.20.0(SERV; api:1.2.0; eu-1)/.
 *
 * Unlike upstream this is not fixed at start-up: a sidecar can register and
 * deregister while the node runs, so the string is rebuilt and read under a
 * lock. Note what that does and does not achieve - the string is sent once per
 * connection, during the version handshake, so a change reaches new peers only.
 * Peers already connected keep whatever they were told until they reconnect.
 * There is no message in the protocol for revising it.
 */
namespace useragent {

enum class RunMode {
    Server, //!< talerd, and anything else linking the node
    Gui,    //!< taler-qt
};

/** Set by each binary's main() before AppInitMain composes the agent. */
void SetRunMode(RunMode mode);
RunMode GetRunMode();
//! "GUI" or "SERV", as it appears in the user agent.
std::string RunModeTag();

/** The -uacomment values, already sanitised and validated by init. */
void SetOperatorComments(std::vector<std::string> comments);

/**
 * Recompose the agent from the current mode, sidecar state and operator
 * comments, and publish it.
 *
 * Returns false and leaves the published string untouched when the result would
 * exceed MAX_SUBVERSION_LENGTH - a node must never broadcast a truncated agent,
 * because the trailing "/" is what tells a parser it reached the end.
 */
bool Rebuild(std::string* error = nullptr);

/** The current agent. Safe to call from any thread. */
std::string Get();

/**
 * Refresh peer connections so they learn a changed user agent.
 *
 * The agent is sent once, in the version handshake, and the protocol has no
 * message for revising it - so a sidecar registering after the node connected
 * leaves every existing peer holding the old string until it happens to
 * reconnect. This closes that gap by dropping stale connections so they are
 * remade, which is the only mechanism the protocol offers.
 *
 * Dropping peers is not free, so it is hedged:
 *
 *  - **Only on a real change.** A rebuild that produces the same string does
 *    nothing at all.
 *  - **Debounced.** The agent must hold still for DEBOUNCE_SECONDS first, so a
 *    sidecar crash-looping cannot turn into a disconnect loop.
 *  - **Rate limited.** At most one pass per MIN_SECONDS_BETWEEN_CYCLES, so
 *    anyone able to restart the sidecar cannot make the node churn on demand -
 *    which also matters because each mass reconnect is a fresh draw of peers,
 *    and repeated draws are how eclipse attempts get their chances.
 *  - **Staggered.** One peer every STAGGER_SECONDS, never all at once, so the
 *    node is never briefly friendless and never re-picks its whole peer set in
 *    one instant.
 *  - **Floored.** Never drops below MIN_CONNECTIONS_KEPT peers.
 *  - **Not while syncing.** Skipped during initial block download, where losing
 *    peers costs real progress and the agent matters least.
 *  - **Only peers that are actually stale**, judged by connection time against
 *    the moment the agent changed, so a peer is never dropped twice for the
 *    same change and the pass always terminates.
 */
void StartConnectionRefreshTask(CScheduler& scheduler);

} // namespace useragent

#endif // BITCOIN_USERAGENT_H
