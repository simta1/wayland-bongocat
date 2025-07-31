# Fullscreen Detection Feature Implementation

## Summary

I've implemented the fullscreen detection feature for some compositors. Here's what was added:

### Key Changes

1. **Layer Change**: Changed from `ZWLR_LAYER_SHELL_V1_LAYER_TOP` to `ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY`
   - This ensures bongocat appears above waybar/quickshell when they start after bongocat
   - Solves the layering battle issue you described

2. **Fullscreen Detection**: Added compositor-specific fullscreen detection
   - Supports Hyprland (primary) and Sway (fallback)
   - Detects both fullscreen modes in Hyprland (mode 1 and mode 2)
   - Checks every 500ms for fullscreen status changes

3. **Dynamic Opacity**: When fullscreen is detected and `hide_on_fullscreen=1`:
   - Sets overlay opacity to 0 (completely transparent)
   - Cat becomes invisible but overlay layer remains active
   - Automatically reappears when fullscreen is exited

4. **Configuration Option**: Added `hide_on_fullscreen` setting
   - `hide_on_fullscreen=1` (default): Hide cat when fullscreen detected
   - `hide_on_fullscreen=0`: Keep cat visible even in fullscreen

### Files Modified

- `include/config.h`: Added `hide_on_fullscreen` field
- `src/config.c`: Added config parsing and validation for new option
- `include/wayland.h`: Added XDG shell support and fullscreen detection globals
- `src/wayland.c`: Implemented fullscreen detection and dynamic opacity
- `bongocat.conf`: Added documentation for new option

### How It Works

1. **Startup**: Bongocat starts with OVERLAY layer (above waybar/quickshell)
2. **Detection**: Every 500ms, checks active window fullscreen status via:
   - `hyprctl activewindow` (for Hyprland) - detects both mode 1 and mode 2 fullscreen
   - `swaymsg -t get_tree` (for Sway)
3. **Response**: When fullscreen detected:
   - Skips drawing the cat entirely (no `blit_image_scaled` call)
   - Sets background opacity to 0 (completely transparent overlay)
   - Triggers immediate redraw
   - Logs state change (if debug enabled)
4. **Recovery**: When fullscreen exits:
   - Resumes normal cat drawing
   - Restores normal background opacity
   - Cat reappears automatically

### Testing

The feature has been tested and confirmed working:
- Fullscreen detection correctly identifies Hyprland fullscreen modes
- Layer change resolves waybar/quickshell ordering issues
- Dynamic opacity successfully hides/shows cat based on fullscreen state

**NOTE: Needs testing for sway** 

### Configuration

```
# By default 
layer=top
```


Change in your `bongocat.conf`:
```
# Hide cat when fullscreen applications are detected
layer=overlay
hide_on_fullscreen=1
```

### Debug Information

Enable debug logging to see fullscreen detection in action:
```
enable_debug=1
```

This will show:
- Fullscreen check intervals
- Detection results
- State changes
- Redraw triggers

## Usage

1. Build: `make clean && make`
2. Run: `./build/bongocat`
3. Test: Open any application and press F11 to go fullscreen
4. Result: Cat should disappear in fullscreen, reappear when exiting fullscreen