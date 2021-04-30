#!/bin/bash

set -v -e -x

original_dir=$PWD
export MARIADB_VERSION="mariadb-10.4.18"
mkdir tmp
shopt -s extglob
mv !(tmp) tmp # Move everything but tmp
# Download mariadb using git, this has a habit of failing so let's do it first
git clone --recurse-submodules https://github.com/MariaDB/server.git -b ${MARIADB_VERSION} ${MARIADB_VERSION}

# Install TileDB using 2.0 release
if [[ -z ${SUPERBUILD+x} || "${SUPERBUILD}" == "OFF" ]]; then

  if [[ "$AGENT_OS" == "Linux" ]]; then
    curl -L -o tiledb.tar.gz https://github.com/TileDB-Inc/TileDB/releases/download/2.2.8/tiledb-linux-2.2.8-6e7a5a2-full.tar.gz \
    && sudo tar -C /usr/local -xvf tiledb.tar.gz
  elif [[ "$AGENT_OS" == "Darwin" ]]; then
    curl -L -o tiledb.tar.gz https://github.com/TileDB-Inc/TileDB/releases/download/2.2.8/tiledb-macos-2.2.8-6e7a5a2-full.tar.gz \
    && sudo tar -C /usr/local -xvf tiledb.tar.gz
  else
    mkdir build_deps && cd build_deps \
    && git clone https://github.com/TileDB-Inc/TileDB.git -b 2.2.8 && cd TileDB \
    && mkdir -p build && cd build

     # Configure and build TileDB
     cmake -DTILEDB_VERBOSE=ON -DTILEDB_S3=ON -DTILEDB_SERIALIZATION=ON -DCMAKE_BUILD_TYPE=Debug .. \
     && make -j$(nproc) \
     && sudo make -C tiledb install \
     && cd $original_dir
   fi
else # set superbuild flags
  export SUPERBUILD_FLAGS_NEEDED="-Wno-error=deprecated-declarations"
  export CXXFLAGS="${CXXFLAGS} ${SUPERBUILD_FLAGS_NEEDED}"
  export CFLAGS="${CFLAGS} ${SUPERBUILD_FLAGS_NEEDED}"
fi

mv tmp ${MARIADB_VERSION}/storage/mytile \
&& cd ${MARIADB_VERSION}
# Cherry-pick cmake 3.20 fix
git cherry-pick 4e825b0e8ae07e1e847cbbc3c5b7203ae5b96a89
mkdir builddir \
&& cd builddir \
&& cmake -DPLUGIN_TOKUDB=NO -DPLUGIN_ROCKSDB=NO -DPLUGIN_MROONGA=NO -DPLUGIN_SPIDER=NO -DPLUGIN_SPHINX=NO -DPLUGIN_FEDERATED=NO -DPLUGIN_FEDERATEDX=NO -DPLUGIN_CONNECT=NO -DCMAKE_BUILD_TYPE=Debug -SWITH_DEBUG=1 .. \
&& make -j$(nproc) \
&& if ! ./mysql-test/mysql-test-run.pl --suite=mytile --debug; then cat ./mysql-test/var/log/mysqld.1.err && false; fi;
