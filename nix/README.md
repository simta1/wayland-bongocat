# Nix Support for Wayland Bongo Cat

This directory contains all Nix-related files for the wayland-bongocat project.

## Files

- **`default.nix`** - Main package derivation
- **`shell.nix`** - Development environment
- **`nixos-module.nix`** - NixOS system module
- **`NIXOS.md`** - Comprehensive NixOS installation guide
- **`test-nix-build.sh`** - Test script for validating builds

## Quick Usage

```bash
# Build the package
nix-build default.nix

# Enter development shell
nix-shell shell.nix

# Test all builds
./test-nix-build.sh
```

## With Flakes (from project root)

```bash
# Build
nix build

# Develop
nix develop

# Run
nix run .
```

For detailed instructions, see [NIXOS.md](NIXOS.md).