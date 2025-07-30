# NixOS Installation Guide

This guide covers various ways to install and use wayland-bongocat on NixOS.

## Quick Start

### Method 1: Direct Installation with Flakes

```bash
# Run once without installing
nix run github:saatvik333/wayland-bongocat

# Install to user profile
nix profile install github:saatvik333/wayland-bongocat

# Find your input devices
bongocat-find-devices
```

### Method 2: Traditional Nix

```bash
# Clone the repository
git clone https://github.com/saatvik333/wayland-bongocat.git
cd wayland-bongocat

# Build and install
nix-env -f nix/default.nix -i

# Or just build
nix-build nix/default.nix
```

## System-Wide Installation

### Using the NixOS Module (Recommended)

Add to your `/etc/nixos/configuration.nix`:

```nix
{ config, pkgs, ... }:

{
  # Import the module
  imports = [
    # ... your other imports
    /path/to/wayland-bongocat/nix/nixos-module.nix
  ];

  # Enable and configure bongocat
  programs.wayland-bongocat = {
    enable = true
    autoStart = true;        # Start on login
    
    # Position settings
    catXOffset = 120;        # Horizontal position
    catYOffset = 0;          # Vertical position
    
    # Size settings
    catHeight = 40;          # Cat size in pixels
    overlayHeight = 60;      # Overlay bar height
    
    # Animation settings
    idleFrame = 0;           # Default idle frame
    keypressDuration = 150;  # Animation duration (ms)
    fps = 60;                # Frame rate
    
    # Visual settings
    overlayOpacity = 0;      # Transparent background
    
    # Input devices (find yours with bongocat-find-devices)
    inputDevices = [
      "/dev/input/event4"    # Built-in keyboard
      "/dev/input/event20"   # External keyboard (example)
    ];
    
    # Debug settings
    enableDebug = false;     # Set to true for troubleshooting
  };
}
```

Then rebuild your system:

```bash
sudo nixos-rebuild switch
```

### Using Flakes in NixOS

If you use flakes for your NixOS configuration:

```nix
# In your flake.nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    bongocat.url = "github:saatvik333/wayland-bongocat";
  };

  outputs = { self, nixpkgs, bongocat }: {
    nixosConfigurations.your-hostname = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ./configuration.nix
        bongocat.nixosModules.default
        {
          programs.wayland-bongocat = {
            enable = true;
            autoStart = true;
          };
        }
      ];
    };
  };
}
```

## Development Environment

### Using Flakes

```bash
# Enter development shell
nix develop

# Or with direnv (recommended)
echo "use flake" > .envrc
direnv allow
```

### Traditional Nix

```bash
# Enter development shell
nix-shell nix/shell.nix

# Build the project
make

# Run the binary
build/bongocat
```

## Configuration

### Finding Input Devices

Use the included helper script:

```bash
bongocat-find-devices
```

This will show you:
- Available input devices
- Device details with human-readable names
- Suggested configuration entries
- Testing instructions

### Manual Configuration

Create a `bongocat.conf` file:

```ini
# Multiple input devices
keyboard_device=/dev/input/event4   # Built-in keyboard
keyboard_device=/dev/input/event20  # External keyboard

# Position and size
cat_x_offset=120
cat_y_offset=0
cat_height=40
overlay_height=60

# Animation settings
idle_frame=0
keypress_duration=150
fps=60

# Visual settings
overlay_opacity=0  # Transparent background
enable_debug=0
```

## Systemd Service Management

When using the NixOS module with a user specified, a systemd user service is created:

```bash
# Check service status
systemctl --user status wayland-bongocat

# Start/stop manually
systemctl --user start wayland-bongocat
systemctl --user stop wayland-bongocat

# Enable/disable auto-start
systemctl --user enable wayland-bongocat
systemctl --user disable wayland-bongocat

# View logs
journalctl --user -u wayland-bongocat -f
```

## Troubleshooting

### Permission Issues

If you get permission errors accessing input devices:

1. **Check if you're in the input group:**
   ```bash
   groups $USER | grep input
   ```

2. **The NixOS module automatically adds you to the input group, but you may need to log out and back in.**

3. **Manual group addition (if not using the module):**
   ```bash
   # Add to your configuration.nix
   users.users.saatvik333.extraGroups = [ "input" ];
   ```

### Service Issues

If the systemd service fails to start:

1. **Check logs:**
   ```bash
   journalctl --user -u wayland-bongocat -n 50
   ```

2. **Test manually:**
   ```bash
   bongocat --config /nix/store/.../bongocat.conf
   ```

3. **Enable debug mode:**
   ```nix
   programs.wayland-bongocat.enableDebug = true;
   ```

### Input Device Detection

If keyboard input isn't detected:

1. **Find your devices:**
   ```bash
   bongocat-find-devices
   ```

2. **Test device events:**
   ```bash
   sudo evtest  # Select your device and type
   ```

3. **Update configuration:**
   ```nix
   programs.wayland-bongocat.inputDevices = [
     "/dev/input/eventX"  # Replace X with your device number
   ];
   ```

### Wayland Compositor Compatibility

Ensure your compositor supports the layer shell protocol:

- ✅ **Hyprland** - Full support
- ✅ **Sway** - Full support  
- ✅ **Wayfire** - Compatible
- ⚠️ **KDE Wayland** - Limited support
- ❌ **GNOME Wayland** - Not supported

## Advanced Usage

### Custom Package

You can override the package in the module:

```nix
programs.wayland-bongocat = {
  enable = true;
  package = pkgs.wayland-bongocat.overrideAttrs (old: {
    # Custom build options
    buildInputs = old.buildInputs ++ [ pkgs.someExtraPackage ];
  });
};
```

### Multiple Configurations

Run different instances with different configs:

```bash
# Instance 1
bongocat --config ~/.config/bongocat/work.conf &

# Instance 2  
bongocat --config ~/.config/bongocat/gaming.conf &
```

### Integration with Window Managers

For Hyprland users, you can add to your config:

```
# In hyprland.conf
exec-once = bongocat
```

For Sway users:

```
# In sway config
exec bongocat
```

## Building from Source

### Prerequisites

The flake includes all necessary dependencies. For manual builds:

```nix
# In shell.nix or development environment
buildInputs = with pkgs; [
  gcc gnumake pkg-config
  wayland wayland-protocols wayland-scanner
  # Development tools
  gdb valgrind clang-tools evtest
];
```

### Build Process

```bash
# Generate protocol files
make protocols

# Generate embedded assets
make embed-assets

# Build release version
make release

# Build debug version
make debug

# Clean build artifacts
make clean
```

The binary will be created in `build/bongocat`.

## Contributing

See the main README.md for contribution guidelines. The NixOS integration welcomes:

- Additional compositor testing
- Module option improvements
- Documentation enhancements
- Bug reports and fixes

## Support

For NixOS-specific issues:
1. Check this guide first
2. Review the module source in `nixos-module.nix`
3. Test with the development shell
4. Open an issue with your NixOS version and configuration