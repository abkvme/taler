### Verify Binaries

#### Preparation:

Make sure you obtain the proper release signing key and verify the fingerprint with several independent sources.

#### Usage:

This script attempts to download the signature file `SHA256SUMS.asc` from https://taler.tech.

It first checks if the signature passes, and then downloads the files specified in the file, and checks if the hashes of these files match those that are specified in the signature file.

The script returns 0 if everything passes the checks. It returns 1 if either the signature check or the hash check doesn't pass. If an error occurs the return value is 2.

If you do not want to keep the downloaded binaries, specify anything as the second parameter.
