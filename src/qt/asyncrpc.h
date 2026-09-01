// Copyright (c) 2026 The Taler Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_ASYNCRPC_H
#define BITCOIN_QT_ASYNCRPC_H

#include <univalue.h>

#include <QString>

#include <string>

class QWidget;

namespace interfaces {
class Node;
}

/**
 * Run a node RPC without blocking the GUI thread, and wait for it.
 *
 * Some RPCs cannot be called from the GUI thread at all. unloadwallet is the clearest
 * case: UnloadWallet() waits on a condition variable until every shared_ptr to the
 * wallet is released, and the GUI's own WalletModel holds one that is only released
 * when the GUI thread processes a queued signal. Calling it directly from the GUI
 * thread deadlocks the application permanently.
 *
 * So the call runs on a worker thread while a local event loop keeps the GUI
 * responsive enough to release what the node is waiting for. An application-modal
 * progress dialog blocks user input meanwhile, so nothing else can be started on top
 * of a wallet operation already in flight.
 *
 * The same helper also keeps long operations - a restore rescan, for instance - from
 * freezing the window.
 */
bool RunNodeRpc(interfaces::Node& node, const QString& method, const UniValue& params,
                const std::string& uri, UniValue& result, QString& error,
                QWidget* parent, const QString& busy_message);

#endif // BITCOIN_QT_ASYNCRPC_H
