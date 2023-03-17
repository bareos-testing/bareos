#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2023 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

set -u
set -e
set -x
shopt -s nullglob

# make sure we're in bareos's toplevel dir
if [ ! -f core/src/include/bareos.h ]; then
  echo "$0: Invoke from Bareos' toplevel directory" >&2
  exit 2
fi

nproc="$(getconf _NPROCESSORS_ONLN 2>/dev/null || sysctl -n hw.ncpu)"

if [ -z ${CTEST_PARALLEL_LEVEL+x} ]; then
  export CTEST_PARALLEL_LEVEL="$nproc"
fi
if [ -z ${CMAKE_BUILD_PARALLEL_LEVEL+x} ]; then
  export CMAKE_BUILD_PARALLEL_LEVEL="$nproc"
fi

rm -rf cmake-build

# preconfigure ccache
CCACHE_DIR="/tmp/ccache/sanitize"
CCACHE_TEMPDIR="/tmp/ccache-tmp"
CCACHE_BASEDIR="$PWD"
CCACHE_SLOPPINESS="file_macro"

# central ccache cache
CCACHE_REMOTE_STORAGE="http://jenkins.bareos.com:9090|layout=bazel|connect-timeout=100"
CCACHE_REMOTE_ONLY=yes
export CCACHE_DIR CCACHE_TEMPDIR CCACHE_BASEDIR CCACHE_SLOPPINESS \
         CCACHE_REMOTE_STORAGE CCACHE_REMOTE_ONLY


cmake \
  -S . \
  -B cmake-build \
  -DENABLE_SANITIZERS=yes \
  -Dpostgresql=yes
cmake --build cmake-build

# avoid problems in containers with sanitizers
# see https://github.com/google/sanitizers/issues/1322
export ASAN_OPTIONS=intercept_tls_get_addr=0

cd cmake-build
export REGRESS_DEBUG=1
ctest \
  --script CTestScript.cmake \
  --verbose \
  --label-exclude "broken.*" \
  || echo "ctest failed"
