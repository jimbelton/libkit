#!/bin/sh

# This script should be run with sudo to add the dev packages required to build libkit

set -e

case $(uname -o) in
FreeBSD)
#   jemalloc is the default memory allocator in FreeBSD
#    PATH=/sbin:$PATH

#    for pkg in xxhash; do
#        if ! pkg info -q $pkg ; then
#            pkg install -y $pkg
#        fi
#    done
    ;;

GNU/Linux)
    export DEBIAN_FRONTEND=noninteractive
    apt-get -y install libjemalloc-dev
    ;;

*)
    echo "I don't know what I'm doing" >&2
    exit 1
    ;;
esac

exit 0
