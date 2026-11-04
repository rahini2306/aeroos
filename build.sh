#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
if command -v node >/dev/null 2>&1; then
  NODE="$(command -v node)"
elif [ -x "$HOME/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node" ]; then
  NODE="$HOME/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/bin/node"
else
  printf 'Install Node.js 20.19+ or 22.12+ and npm, then run ./build.sh\n' >&2
  exit 1
fi
cd frontend
if [ ! -d node_modules/typescript ]; then
  npm install
fi
"$NODE" node_modules/typescript/bin/tsc --noEmit
"$NODE" node_modules/vite/bin/vite.js build
