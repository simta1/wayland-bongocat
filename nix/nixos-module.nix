# NixOS module for wayland-bongocat
{ config, lib, pkgs, ... }:

with lib;

let
  cfg = config.programs.wayland-bongocat;
  
  wayland-bongocat = pkgs.callPackage ./default.nix {};
  
  configFile = pkgs.writeText "bongocat.conf" ''
    # Generated NixOS configuration for wayland-bongocat
    
    # Position settings (in pixels)
    cat_x_offset=${toString cfg.catXOffset}
    cat_y_offset=${toString cfg.catYOffset}
    
    # Size settings
    cat_height=${toString cfg.catHeight}
    overlay_height=${toString cfg.overlayHeight}
    
    # Animation settings
    idle_frame=${toString cfg.idleFrame}
    keypress_duration=${toString cfg.keypressDuration}
    test_animation_duration=${toString cfg.testAnimationDuration}
    test_animation_interval=${toString cfg.testAnimationInterval}
    
    # Performance settings
    fps=${toString cfg.fps}
    overlay_opacity=${toString cfg.overlayOpacity}
    
    # Debug settings
    enable_debug=${if cfg.enableDebug then "1" else "0"}
    
    # Input devices
    ${concatMapStringsSep "\n" (device: "keyboard_device=${device}") cfg.inputDevices}
  '';
  
in {
  options.programs.wayland-bongocat = {
    enable = mkEnableOption "wayland-bongocat overlay";
    
    package = mkOption {
      type = types.package;
      default = wayland-bongocat;
      description = "The wayland-bongocat package to use.";
    };
    
    # Position settings
    catXOffset = mkOption {
      type = types.int;
      default = 120;
      description = "Horizontal offset from center position (pixels)";
    };
    
    catYOffset = mkOption {
      type = types.int;
      default = 0;
      description = "Vertical offset from center position (pixels)";
    };
    
    # Size settings
    catHeight = mkOption {
      type = types.int;
      default = 40;
      description = "Height of the bongo cat in pixels";
    };
    
    overlayHeight = mkOption {
      type = types.int;
      default = 60;
      description = "Height of the entire overlay bar";
    };
    
    # Animation settings
    idleFrame = mkOption {
      type = types.int;
      default = 0;
      description = "Which frame to use when idle (0, 1, or 2)";
    };
    
    keypressDuration = mkOption {
      type = types.int;
      default = 150;
      description = "How long to show animation after keypress (ms)";
    };
    
    testAnimationDuration = mkOption {
      type = types.int;
      default = 200;
      description = "How long to show test animation (ms)";
    };
    
    testAnimationInterval = mkOption {
      type = types.int;
      default = 0;
      description = "How often to trigger test animation (seconds, 0 = disabled)";
    };
    
    # Performance settings
    fps = mkOption {
      type = types.int;
      default = 60;
      description = "Animation frame rate (frames per second)";
    };
    
    overlayOpacity = mkOption {
      type = types.int;
      default = 0;
      description = "Overlay background opacity (0-255, 0 = transparent)";
    };
    
    # Debug settings
    enableDebug = mkOption {
      type = types.bool;
      default = false;
      description = "Enable debug logging";
    };
    
    # Input devices
    inputDevices = mkOption {
      type = types.listOf types.str;
      default = [ "/dev/input/event4" ];
      description = "List of input device paths to monitor";
      example = [ "/dev/input/event4" "/dev/input/event20" ];
    };
    
    # User service options
    user = mkOption {
      type = types.nullOr types.str;
      default = "saatvik333";
      description = "User to run the service as (null = don't create service)";
    };
    
    autoStart = mkOption {
      type = types.bool;
      default = false;
      description = "Automatically start the service on login";
    };
  };
  
  config = mkIf cfg.enable {
    # Install the package system-wide
    environment.systemPackages = [ cfg.package ];
    
    # Create systemd user service if user is specified
    systemd.user.services.wayland-bongocat = mkIf (cfg.user != null) {
      description = "Wayland Bongo Cat Overlay";
      wantedBy = mkIf cfg.autoStart [ "graphical-session.target" ];
      partOf = [ "graphical-session.target" ];
      after = [ "graphical-session.target" ];
      
      serviceConfig = {
        Type = "simple";
        ExecStart = "${cfg.package}/bin/bongocat --config ${configFile}";
        Restart = "on-failure";
        RestartSec = "5s";
        
        # Security settings
        NoNewPrivileges = true;
        PrivateTmp = true;
        ProtectSystem = "strict";
        ProtectHome = "read-only";
        
        # Allow access to input devices
        SupplementaryGroups = [ "input" ];
      };
      
      environment = {
        WAYLAND_DISPLAY = "wayland-0";
        XDG_RUNTIME_DIR = "/run/user/%i";
      };
    };
    
    # Ensure input group exists and user is added to it
    users.groups.input = mkIf (cfg.user != null) {};
    users.users.${cfg.user} = mkIf (cfg.user != null) {
      extraGroups = [ "input" ];
    };
    
    # Create helper script for finding input devices
    environment.systemPackages = [
      (pkgs.writeScriptBin "bongocat-find-devices" ''
        #!${pkgs.bash}/bin/bash
        exec ${cfg.package}/bin/bongocat-find-devices "$@"
      '')
    ];
  };
  
  meta = {
    maintainers = with lib.maintainers; [ ];
    doc = ../README.md;
  };
}