// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_WALLETUTIL_H
#define BITCOIN_WALLET_WALLETUTIL_H

#include <chainparamsbase.h>
#include <util.h>

//! Get the path of the wallet directory.
fs::path GetWalletDir();

//! The WalletLocation class provides wallet information.
//! Wallets found in the wallet directory, whether loaded or not. A wallet is either
//! a Berkeley DB file or a directory containing one named wallet.dat.
std::vector<fs::path> ListWalletDir();

class WalletLocation final
{
    std::string m_name;
    fs::path m_path;

public:
    explicit WalletLocation() {}
    explicit WalletLocation(const std::string& name);

    //! Get wallet name.
    const std::string& GetName() const { return m_name; }

    //! Get wallet absolute path.
    const fs::path& GetPath() const { return m_path; }

    //! Return whether the wallet exists.
    bool Exists() const;

    //! Whether a wallet actually lives here. Exists() only says the path is there,
    //! and for the unnamed default wallet that path IS the wallet directory, which
    //! always exists - so Exists() answers "yes" for a datadir holding no wallet.
    bool HasWalletData() const;
};

#endif // BITCOIN_WALLET_WALLETUTIL_H
