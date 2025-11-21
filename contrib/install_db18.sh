#!/bin/sh

# Install Berkeley DB 18.1.40 for Taler

export LC_ALL=C
set -e

if [ -z "${1}" ]; then
  echo "Usage: ./install_db18.sh <base-dir> [<extra-bdb-configure-flag> ...]"
  echo
  echo "Must specify a single argument: the directory in which db18 will be built."
  echo "This is probably \`pwd\` if you're at the root of the taler repository."
  exit 1
fi

expand_path() {
  echo "$(cd "${1}" && pwd -P)"
}

BDB_PREFIX="$(expand_path ${1})/db18"; shift;
BDB_VERSION='db-18.1.40'
BDB_HASH='0cecb2ef0c67b166de93732769abdeba0555086d51de1090df325e18ee8da9c8'
BDB_URL="https://download.oracle.com/berkeley-db/${BDB_VERSION}.tar.gz"

check_exists() {
  which "$1" >/dev/null 2>&1
}

sha256_check() {
  # Args: <sha256_hash> <filename>
  #
  if check_exists sha256sum; then
    echo "${1}  ${2}" | sha256sum -c
  elif check_exists sha256; then
    if [ "$(uname)" = "FreeBSD" ]; then
      sha256 -c "${1}" "${2}"
    else
      echo "${1}  ${2}" | sha256 -c
    fi
  else
    echo "${1}  ${2}" | shasum -a 256 -c
  fi
}

http_get() {
  # Args: <url> <filename> <sha256_hash>
  #
  # It's acceptable that we don't require SSL here because we manually verify
  # content hashes below.
  #
  if [ -f "${2}" ]; then
    echo "File ${2} already exists; not downloading again"
  elif check_exists curl; then
    curl --insecure "${1}" -o "${2}"
  else
    wget --no-check-certificate "${1}" -O "${2}"
  fi

  sha256_check "${3}" "${2}"
}

mkdir -p "${BDB_PREFIX}"
http_get "${BDB_URL}" "${BDB_VERSION}.tar.gz" "${BDB_HASH}"
tar -xzvf ${BDB_VERSION}.tar.gz -C "$BDB_PREFIX"
cd "${BDB_PREFIX}/${BDB_VERSION}/"

cd build_unix/

"${BDB_PREFIX}/${BDB_VERSION}/dist/configure" \
  --enable-cxx --disable-shared --disable-replication --with-pic --prefix="${BDB_PREFIX}" \
  "${@}"

make install

echo
echo "db18 build complete."
echo
echo 'When compiling talerd, run `./configure` in the following way:'
echo
echo "  export BDB_PREFIX='${BDB_PREFIX}'"
echo '  ./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-18.1" BDB_CFLAGS="-I${BDB_PREFIX}/include" ...'
