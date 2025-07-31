{
  lib,
  stdenv,
  gcc,
  gnumake,
  pkg-config,
  wayland,
  wayland-protocols,
  wayland-scanner,
}:
stdenv.mkDerivation {
  pname = "wayland-bongocat";
  version = "1.2.1";

  src = ../.;

  nativeBuildInputs = [
    gcc
    gnumake
    pkg-config
    wayland-scanner
    wayland-protocols
  ];

  buildInputs = [
    wayland
    wayland-protocols
  ];

  preBuild = ''
    # Ensure build directory exists
    mkdir -p build/obj

    # Make scripts executable
    chmod +x scripts/embed_assets.sh
    chmod +x scripts/find_input_devices.sh
  '';

  buildPhase = ''
    runHook preBuild

    # Ensure that the Makefile has the correct directory with the Wayland protocols
    export WAYLAND_PROTOCOLS_DIR="${wayland-protocols}/share/wayland-protocols"

    # Compile with all optimizations (Longer compile time, improved performance)
    make release

    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall

    # Create directories
    mkdir -p $out/bin
    mkdir -p $out/share/bongocat
    mkdir -p $out/share/doc/bongocat

    # Install binaries
    cp build/bongocat $out/bin/
    cp scripts/find_input_devices.sh $out/bin/bongocat-find-devices

    # Install configuration example
    cp bongocat.conf $out/share/bongocat/bongocat.conf.example

    # Install documentation
    cp README.md $out/share/doc/bongocat/
    cp -r assets $out/share/doc/bongocat/

    runHook postInstall
  '';
}
