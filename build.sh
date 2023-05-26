#!/bin/bash

# set ldflag and cppflags
export LDFLAGS="-L/opt/homebrew/opt/openssl@3/lib"
export CPPFLAGS="-I/opt/homebrew/opt/openssl@3/include"
LDFLAGS+=" $(pkg-config --libs libwebsockets)"
LDFLAGS+=" $(pkg-config --libs json-c)"
CPPFLAGS+=" $(pkg-config --cflags libwebsockets)"
CPPFLAGS+=" $(pkg-config --cflags json-c)"

echo building with ldflags=$LDFLAGS and cppflags=$CPPFLAGS
# build
gcc $CPPFLAGS -o signal-server src/main.c $LDFLAGS 
