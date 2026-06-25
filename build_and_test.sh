#!/bin/bash
set -e

cmake --build build --clean-first 2>&1

echo ""
echo "Running test..."
./out
echo "Exit code: $?"
