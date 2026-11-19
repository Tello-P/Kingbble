#!/usr/bin/env bash
# Builds the engine for the browser: web/static/demo/engine.js and engine.wasm.
# Needs the Emscripten SDK (em++) on PATH.
set -euo pipefail
cd "$(dirname "$0")/../.."
out=web/static/demo
mkdir -p "$out"
em++ -O3 -std=c++20 -fwasm-exceptions -DNDEBUG \
  -Iengine/include -Iengine/apps \
  web/wasm/web_bridge.cpp engine/src/*.cpp \
  -sMODULARIZE=1 -sEXPORT_NAME=KingbbleEngine -sENVIRONMENT=worker \
  -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=4194304 \
  -sEXPORTED_RUNTIME_METHODS=ccall,FS \
  -o "$out/engine.js"
ls -l "$out"
