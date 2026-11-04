#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
make -C core
if [ ! -x .venv/bin/python ]; then
  python3 -m venv .venv
  .venv/bin/python -m pip install -r backend/requirements.txt
fi
if [ ! -f frontend/dist/index.html ]; then
  ./build.sh
fi
printf '\nAeroOS — Airport Desktop: http://127.0.0.1:5050\nPress Ctrl+C to stop.\n\n'
exec .venv/bin/python backend/app.py
