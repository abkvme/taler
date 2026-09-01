// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/walletutil.h>

fs::path GetWalletDir()
{
    fs::path path;

    if (gArgs.IsArgSet("-walletdir")) {
        path = gArgs.GetArg("-walletdir", "");
        if (!fs::is_directory(path)) {
            // If the path specified doesn't exist, we return the deliberately
            // invalid empty string.
            path = "";
        }
    } else {
        path = GetDataDir();
        // If a wallets directory exists, use that, otherwise default to GetDataDir
        if (fs::is_directory(path / "wallets")) {
            path /= "wallets";
        }
    }

    return path;
}

WalletLocation::WalletLocation(const std::string& name)
    : m_name(name)
    , m_path(fs::absolute(name, GetWalletDir()))
{
}

bool WalletLocation::Exists() const
{
    return fs::symlink_status(m_path).type() != fs::file_not_found;
}
bool WalletLocation::HasWalletData() const
{
    if (fs::is_directory(m_path)) return fs::exists(m_path / "wallet.dat");
    return fs::symlink_status(m_path).type() != fs::file_not_found;
}


//! True when the file really is a Berkeley DB Btree, i.e. a wallet.
//!
//! This has to be a content check, not a name check. On an existing installation the
//! wallet directory IS the data directory, so peers.dat, debug.log, taler.conf and the
//! rest sit right next to the wallets - listing by name would offer to rename and
//! remove the node's own files.
static bool IsBerkeleyBtree(const fs::path& path)
{
    FILE* file = fsbridge::fopen(path, "rb");
    if (!file) return false;

    // The Btree magic number lives at offset 12, in either endianness.
    uint32_t magic = 0;
    bool ok = fseek(file, 12, SEEK_SET) == 0 && fread(&magic, sizeof(magic), 1, file) == 1;
    fclose(file);
    if (!ok) return false;
    return magic == 0x00053162 || magic == 0x62310500;
}

std::vector<fs::path> ListWalletDir()
{
    const fs::path wallet_dir = GetWalletDir();
    std::vector<fs::path> paths;
    if (!fs::is_directory(wallet_dir)) return paths;

    for (auto it = fs::directory_iterator(wallet_dir); it != fs::directory_iterator(); ++it) {
        const fs::path& path = it->path();
        const std::string name = path.filename().string();
        if (name == "database" || name == "removed" || name == "backups") continue;

        try {
            if (fs::is_directory(path)) {
                // A wallet may also be a directory holding wallet.dat.
                if (IsBerkeleyBtree(path / "wallet.dat")) paths.push_back(path);
            } else if (fs::is_regular_file(path) && IsBerkeleyBtree(path)) {
                paths.push_back(path);
            }
        } catch (const fs::filesystem_error&) {
            // unreadable entry: not a wallet we can offer
        }
    }
    return paths;
}
