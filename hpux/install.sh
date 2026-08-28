#!/bin/sh
#  Minimal BSD install(1) stand-in: install.sh -m MODE SOURCE DEST
mode=755
while [ "$1" = "-m" ]; do mode=$2; shift 2; done
cp "$1" "$2" && chmod "$mode" "$2"
