// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_USERAGENT_H
#define BITCOIN_USERAGENT_H

#include <string>
#include <vector>

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

} // namespace useragent

#endif // BITCOIN_USERAGENT_H
