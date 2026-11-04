#!/bin/sh
set -eu
cd "$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
make -C core test
.venv/bin/python -m unittest discover -s tests -v
./build.sh
if [ "${1:-}" = "--sanitize" ]; then
  make -C core sanitize
  AEROOS_TEST_ENGINE="$PWD/core/aeroos-sanitize" .venv/bin/python -m unittest discover -s tests -p test_engine.py -v
  AEROOS_TEST_ENGINE="$PWD/core/aeroos-sanitize" .venv/bin/python -m unittest discover -s tests -p test_verifier.py -v
fi
