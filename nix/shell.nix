{pkgs ? import <nixpkgs> {}}:
pkgs.mkShell {
  buildInputs = with pkgs; [
    # Build dependencies
    gcc
    gnumake
    pkg-config
    wayland-scanner
    wayland-protocols
    wayland

    # Development tools
    gdb
    valgrind
    clang-tools

    # Optional tools for input device debugging
    evtest
    udev

    # Documentation tools
    man-pages
    man-pages-posix
  ];

  shellHook = ''
    echo "🐱 Bongo Cat Development Environment (Traditional Nix)"
    echo "Available commands:"
    echo "  make          - Build the project"
    echo "  make debug    - Build with debug symbols"
    echo "  make clean    - Clean build artifacts"
    echo "  make memcheck - Run with valgrind (requires debug build)"
    echo ""
    echo "Helper scripts:"
    echo "  ./scripts/find_input_devices.sh - Find your input devices"
    echo ""
    echo "Build output will be in: build/bongocat"
    echo ""
    echo "To install system-wide:"
    echo "  nix-env -f . -i"
    echo ""
    echo "To build without installing:"
    echo "  nix-build"
  '';
}
