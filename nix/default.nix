{ lib
, stdenv
, fetchFromGitHub
, gcc
, gnumake
, pkg-config
, wayland
, wayland-protocols
, wayland-scanner
}:

stdenv.mkDerivation rec {
  pname = "wayland-bongocat";
  version = "1.2.0";

  src = ./.;

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
    
    # Generate protocol files
    make protocols
    
    # Generate embedded assets
    make embed-assets
    
    # Build the application
    make release
    
    runHook postBuild
  '';

  installPhase = ''
    runHook preInstall
    
    # Create directories
    mkdir -p $out/bin
    mkdir -p $out/share/bongocat
    mkdir -p $out/share/doc/bongocat
    
    # Install binary
    cp build/bongocat $out/bin/
    
    # Install configuration example
    cp bongocat.conf $out/share/bongocat/bongocat.conf.example
    
    # Install helper script
    cp scripts/find_input_devices.sh $out/bin/bongocat-find-devices
    
    # Install documentation
    cp README.md $out/share/doc/bongocat/
    cp -r assets $out/share/doc/bongocat/
    
    runHook postInstall
  '';

  meta = with lib; {
    description = "A Wayland overlay that displays an animated bongo cat reacting to keyboard input";
    longDescription = ''
      Bongo Cat Wayland Overlay is a fun desktop companion that shows an animated
      bongo cat reacting to your keyboard input in real-time. Features include:
      
      - Real-time keyboard input monitoring from multiple devices
      - Click-through overlay that doesn't interfere with your workflow
      - Configurable positioning, size, and animation settings
      - Support for multiple input devices (built-in + external keyboards)
      - Professional C11 codebase with comprehensive error handling
    '';
    homepage = "https://github.com/saatvik333/wayland-bongocat";
    license = licenses.mit;
    maintainers = [ ];
    platforms = platforms.linux;
    mainProgram = "bongocat";
  };
}