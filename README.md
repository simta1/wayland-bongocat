# Bongo Cat Wayland Overlay <img src="assets/bongocat.gif" alt="Bongo Cat Demo" width="60" style="vertical-align: middle;">

A Wayland overlay that displays an animated bongo cat reacting to keyboard input. Built with modern C11 standards and featuring a modular architecture, comprehensive error handling, and optimizations. Perfect for streamers, developers, or anyone who wants an adorable desktop companion!

![Demo](assets/demo.gif)

*Watch Bongo Cat react to your keystrokes in real-time!*


## ✨ Features

### Core Functionality
- **Wayland Layer Shell Integration** - Professional overlay implementation
- **Real-time Input Monitoring** - Responsive keyboard input detection
- **Smooth Animation System** - 60 FPS animation with configurable timing
- **Automatic Screen Detection** - Dynamic screen width detection (Hyprland support)
- **Configurable Positioning** - Precise pixel-level control

### Professional Architecture
- **Modular Codebase** - Clean separation of concerns across 7 modules
- **Comprehensive Error Handling** - Detailed logging with timestamps
- **Memory Management** - Built-in leak detection and cleanup
- **Signal Handling** - Graceful shutdown and process management
- **Configuration Validation** - Runtime validation with range checking
- **Command-Line Interface** - Professional CLI with help and version info

## 📋 Requirements

### System Dependencies
- **Wayland Compositor** - Any Wayland-based desktop environment
- **C11 Compiler** - GCC or Clang with C11 support
- **Make** - GNU Make for building
- **Wayland Development Libraries** - Core Wayland client libraries
- **wayland-scanner** - Protocol code generation

### Runtime Dependencies
- **libwayland-client** - Wayland client library
- **libm** - Math library (usually included)
- **libpthread** - POSIX threads (usually included)
- **hyprctl** - For automatic screen detection (Hyprland users)

### Installation Commands

#### Ubuntu/Debian
```bash
sudo apt install libwayland-dev wayland-protocols build-essential
```

#### Arch Linux
```bash
sudo pacman -S wayland wayland-protocols base-devel
```

#### Fedora
```bash
sudo dnf install wayland-devel wayland-protocols-devel gcc make
```

## 🔨 Building

### Quick Build
```bash
make
```

### Build Options
```bash
# Production build (default)
make

# Debug build with symbols
make debug

# Clean build artifacts
make clean

# Force rebuild
make clean && make
```

### Build Process
The Makefile automatically:
1. **Generates Wayland protocol files** from XML specifications
2. **Compiles source modules** with fast optimizations
3. **Links the final executable** with required libraries
4. **Applies security hardening** flags and optimizations

### Compiler Flags Used
- **Standards**: `-std=c11` (Modern C11 standard)
- **Warnings**: `-Wall -Wextra -Wpedantic` (Comprehensive warnings)
- **Security**: `-fstack-protector-strong -D_FORTIFY_SOURCE=2`
- **Optimization**: `-O3 -flto -march=native` (Maximum performance)

## 🚀 Usage

### Basic Usage
```bash
# Run with default configuration
./bongocat

# Show help information
./bongocat --help

# Display version information
./bongocat --version

# Use custom configuration file
./bongocat --config my_config.conf
```

### Command-Line Options
- `-h, --help` - Display help message and usage information
- `-v, --version` - Show version and build information
- `-c, --config FILE` - Specify custom configuration file path

### Expected Behavior
- The overlay appears as a transparent layer on your desktop
- Bongo cat animates in response to keyboard input
- Runs continuously until terminated (Ctrl+C)
- Logs activity to console (configurable verbosity)

## ⚙️ Configuration

The application uses a comprehensive configuration system with runtime validation. Customize behavior using the `bongocat.conf` file in the project directory.

### Configuration File Format

The configuration file uses a simple key-value format with comments:

```ini
# Position settings (in pixels)
cat_x_offset=150        # Horizontal offset from center (+ = right, - = left)
cat_y_offset=0          # Vertical offset from center (+ = down, - = up)

# Size settings
cat_height=32           # Height of the bongo cat in pixels
overlay_height=40       # Height of the entire overlay bar

# Animation settings
idle_frame=0            # Which frame to use when idle (0, 1, or 2)

# Animation timing (in milliseconds)
keypress_duration=100   # How long to show animation after keypress
test_animation_duration=200  # How long to show test animation
test_animation_interval=3    # How often to trigger test animation (seconds, 0 = disabled)

# Performance settings
fps=60                  # Animation frame rate (1-120 fps)

# Visual settings
overlay_opacity=150     # Background opacity (0-255, 0 = transparent, 255 = opaque)

# Debug settings
enable_debug=1          # Show debug messages (0 = off, 1 = on)

# Input device
keyboard_device=/dev/input/event4  # Path to your keyboard device
```

### Configuration Options Reference

| Setting | Type | Range | Default | Description |
|---------|------|-------|---------|-------------|
| `cat_x_offset` | Integer | -9999 to 9999 | 150 | Horizontal position offset from center |
| `cat_y_offset` | Integer | -9999 to 9999 | 0 | Vertical position offset from center |
| `cat_height` | Integer | 16 to 128 | 32 | Height of bongo cat in pixels |
| `overlay_height` | Integer | 20 to 200 | 40 | Total overlay bar height |
| `idle_frame` | Integer | 0 to 2 | 0 | Animation frame when idle |
| `keypress_duration` | Integer | 50 to 5000 | 100 | Animation duration after keypress (ms) |
| `test_animation_duration` | Integer | 100 to 2000 | 200 | Test animation duration (ms) |
| `test_animation_interval` | Integer | 0 to 60 | 3 | Test animation interval (seconds, 0=disabled) |
| `fps` | Integer | 1 to 120 | 60 | Animation frame rate |
| `overlay_opacity` | Integer | 0 to 255 | 150 | Background opacity (0=transparent, 255=opaque) |
| `enable_debug` | Boolean | 0 or 1 | 1 | Enable debug logging |
| `keyboard_device` | String | Valid path | `/dev/input/event4` | Input device path |

### Animation Frame Reference

- **Frame 0**: Both paws up (default idle state)
- **Frame 1**: Left paw down (left key animation)
- **Frame 2**: Right paw down (right key animation)

### Automatic Features

- **Screen Detection**: Automatically detects screen width using `hyprctl` (Hyprland)
- **Configuration Validation**: All values are validated at runtime with helpful error messages
- **Hot Reload**: Configuration changes require restart (planned feature for future versions)

## 📁 Project Structure

The project follows a modular architecture with clear separation of concerns:

```text
bongocat/
├── src/                    # Source code modules
│   ├── main.c             # Application entry point and coordination
│   ├── wayland.c          # Wayland protocol handling and window management
│   ├── animation.c        # Animation system with frame management
│   ├── input.c            # Keyboard/mouse input monitoring
│   ├── config.c           # Configuration file parsing and validation
│   ├── error.c            # Comprehensive error handling and logging
│   └── memory.c           # Memory management with leak detection
├── include/               # Header files and interfaces
│   ├── bongocat.h         # Common definitions and shared structures
│   ├── wayland.h          # Wayland interface declarations
│   ├── animation.h        # Animation system interface
│   ├── input.h            # Input monitoring interface
│   ├── config.h           # Configuration system interface
│   ├── error.h            # Error handling interface
│   └── memory.h           # Memory management interface
├── lib/                   # Third-party libraries
│   ├── stb_image.h        # STB image loading library (embedded)
│   └── stb_image_resize.h # STB image resize library (embedded)
├── protocols/             # Wayland protocol files
│   ├── wlr-layer-shell-unstable-v1.xml    # Layer shell protocol spec
│   ├── xdg-shell-client-protocol.h        # Generated XDG shell header
│   ├── xdg-shell-protocol.c               # Generated XDG shell code
│   ├── zwlr-layer-shell-v1-client-protocol.h  # Generated layer shell header
│   └── zwlr-layer-shell-v1-protocol.c     # Generated layer shell code
├── assets/                # Animation frame images
│   ├── bongo-cat-both-up.png     # Idle state (both paws up)
│   ├── bongo-cat-left-down.png   # Left paw animation
│   └── bongo-cat-right-down.png  # Right paw animation
├── obj/                   # Compiled object files (created during build)
├── bongocat              # Final executable (created during build)
├── bongocat.conf         # User configuration file
├── Makefile              # Build system
├── build.sh              # Convenience build script
└── README.md             # This documentation
```

### Module Responsibilities

| Module | Purpose | Key Functions |
|--------|---------|---------------|
| **main.c** | Application lifecycle | Initialization, main loop, cleanup |
| **wayland.c** | Wayland integration | Layer shell setup, surface management |
| **animation.c** | Animation engine | Frame management, timing, rendering |
| **input.c** | Input monitoring | Device polling, event processing |
| **config.c** | Configuration | File parsing, validation, defaults |
| **error.c** | Error handling | Logging, error codes, diagnostics |
| **memory.c** | Memory management | Allocation tracking, leak detection |

## 🎨 Assets

The project includes high-quality animation frames:

- **`bongo-cat-both-up.png`** - Default idle state (both paws raised)
- **`bongo-cat-left-down.png`** - Left paw down animation frame
- **`bongo-cat-right-down.png`** - Right paw down animation frame

### Asset Specifications
- **Format**: PNG with transparency
- **Recommended Size**: 32x32 pixels (configurable)
- **Color Depth**: 32-bit RGBA
- **Optimization**: Optimized for fast loading and rendering

## 🔧 Troubleshooting

### Common Issues and Solutions

#### Permission Errors
**Problem**: `Permission denied` when accessing `/dev/input/event4`

**Solutions**:
1. **Add user to input group** (recommended):
   ```bash
   sudo usermod -a -G input $USER
   # Log out and back in for changes to take effect
   ```

2. **Run with elevated permissions** (temporary):
   ```bash
   sudo ./bongocat
   ```

3. **Use udev rules** (system-wide):
   ```bash
   # Create udev rule for input access
   echo 'KERNEL=="event*", GROUP="input", MODE="0664"' | sudo tee /etc/udev/rules.d/99-input.rules
   sudo udevadm control --reload-rules
   ```

#### Input Device Detection
**Problem**: Keyboard input not detected or wrong device

**Diagnosis**:
```bash
# List all input devices
ls -la /dev/input/

# Show detailed device information
cat /proc/bus/input/devices

# Monitor input events (requires root)
sudo evtest
```

**Solution**: Update `keyboard_device` in `bongocat.conf` with the correct device path.

#### Build Errors
**Problem**: Compilation fails with missing dependencies

**Solutions**:
- **Missing Wayland headers**: Install development packages (see Requirements section)
- **Compiler too old**: Ensure GCC 4.9+ or Clang 3.4+ for C11 support
- **Protocol generation fails**: Install `wayland-scanner` package

#### Runtime Issues
**Problem**: Application crashes or doesn't display

**Debugging steps**:
1. **Enable debug logging**:
   ```bash
   ./bongocat --config bongocat.conf  # Ensure enable_debug=1
   ```

2. **Check Wayland compositor**:
   ```bash
   echo $WAYLAND_DISPLAY  # Should show wayland-0 or similar
   ```

3. **Verify layer shell support**:
   - Ensure your compositor supports `wlr-layer-shell-unstable-v1`
   - Tested with: Hyprland, Sway, Wayfire

#### Performance Issues
**Problem**: High CPU usage or stuttering animation

**Solutions**:
- Lower `fps` setting in configuration (try 30 FPS)
- Increase `keypress_duration` to reduce animation frequency
- Disable `test_animation_interval` (set to 0)
- Use release build instead of debug build

### Getting Help

If you encounter issues not covered here:

1. **Check the logs** - Enable debug mode for detailed output
2. **Verify your setup** - Ensure all dependencies are installed
3. **Test minimal config** - Try with default configuration
4. **Check compositor compatibility** - Verify layer shell support

## 📊 Performance & Specifications

### System Requirements
- **CPU**: Any modern x86_64 or ARM64 processor
- **RAM**: ~2MB runtime memory usage
- **Storage**: ~1.1MB executable size
- **Compositor**: Wayland with layer shell protocol support

### Performance Characteristics
- **Startup Time**: <100ms typical
- **CPU Usage**: <1% on modern systems
- **Memory Usage**: ~2MB RSS (with leak detection)
- **Animation Smoothness**: 60 FPS with vsync support

### Tested Environments
- **Hyprland** ✅ Full support with screen detection
- **Sway** ✅ Full support
- **Wayfire** ✅ Compatible
- **KDE Wayland** ⚠️ Limited layer shell support
- **GNOME Wayland** ❌ No layer shell support

## 🚀 Development

### Code Quality Standards
- **C11 Standard** - Modern C with strict compliance
- **Memory Safety** - Comprehensive leak detection and bounds checking
- **Error Handling** - All functions return proper error codes
- **Documentation** - Extensive inline comments and documentation
- **Testing** - Build with optimization flags

### Build System Features
- **Dependency Tracking** - Automatic rebuilds on header changes
- **Protocol Generation** - Automatic Wayland protocol code generation
- **Optimization Levels** - Debug and release build configurations
- **Static Analysis** - Comprehensive compiler warnings enabled

### Contributing
This is a codebase following industry best practices. The modular architecture makes it easy to extend with new features or adapt for different use cases.

## 📄 License

This project is open source and available under a permissive license. Feel free to modify, distribute, and use in your own projects.

---

**Built with ❤️ for the Wayland community**

*Bongo Cat Overlay v1.0*
