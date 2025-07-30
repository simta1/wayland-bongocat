#!/bin/bash

# Simple build script for bongocat

set -e

echo "Building bongocat..."

# Clean previous build
make clean

# Build the project
make

echo "Build complete! Run with: bongocat"