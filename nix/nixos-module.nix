# NixOS module for wayland-bongocat
{
  config,
  lib,
  pkgs,
  ...
}:
with lib; let
  cfg = config.programs.wayland-bongocat;
  wayland-bongocat = pkgs.callPackage ./default.nix {};
  configFile = pkgs.writeTextFile {
    name = "bongocat.conf";
    text = ''
      # Auto-generated config for `wayland-bongocat`

      # Position
      cat_x_offset=${toString cfg.catXOffset}
      cat_y_offset=${toString cfg.catYOffset}

      # Size
      cat_height=${toString cfg.catHeight}
      overlay_height=${toString cfg.overlayHeight}

      # Animations
      idle_frame=${toString cfg.idleFrame}
      keypress_duration=${toString cfg.keypressDuration}
      test_animation_duration=${toString cfg.testAnimationDuration}
      test_animation_interval=${toString cfg.testAnimationInterval}

      # Performance
      fps=${toString cfg.fps}
      overlay_opacity=${toString cfg.overlayOpacity}

      # Debug mode
      enable_debug=${
        if cfg.enableDebug
        then "1"
        else "0"
      }

      # Input devices
      ${concatMapStringsSep "\n" (device: "keyboard_device=${device}") cfg.inputDevices}
    '';
  };
in {
  meta.maintainers = with lib.maintainers; [];
  options.programs.wayland-bongocat = {
    enable = mkEnableOption "wayland-bongocat overlay";
    autostart = mkOption {
      type = types.bool;
      default = false;
      description = "Enable and automatically start `bongocat-wayland` as a service on login";
    };

    package = mkOption {
      type = types.package;
      default = wayland-bongocat;
      description = "The wayland-bongocat package to use.";
    };

    # Debug mode
    enableDebug = mkOption {
      type = types.bool;
      default = false;
      description = "Enable debug logging";
    };

    # Position
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

    # Size
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

    # Animations
    idleFrame = mkOption {
      type = types.int;
      default = 0;
      description = "Frame to use when idle (0, 1, or 2)";
    };
    keypressDuration = mkOption {
      type = types.int;
      default = 150;
      description = "Animation duration after keypress (ms)";
    };
    testAnimationDuration = mkOption {
      type = types.int;
      default = 200;
      description = "Test animation duration (ms)";
    };
    testAnimationInterval = mkOption {
      type = types.int;
      default = 0;
      description = "How often to trigger test animation (seconds, 0 = disabled)";
    };

    # Performance
    fps = mkOption {
      type = types.int;
      default = 60;
      description = "Animation framerate (FPS)";
    };

    overlayOpacity = mkOption {
      type = types.int;
      default = 0;
      description = "Overlay background opacity (0-255, 0 = transparent)";
    };

    # Input devices
    inputDevices = mkOption {
      type = types.listOf types.str;
      default = ["/dev/input/event4"];
      description = "List of input devices to monitor, run `bongocat-find-devices` to see all devices to add to this list";
      example = ["/dev/input/event4" "/dev/input/event20"];
    };
  };

  config = mkIf cfg.enable {
    # Install the package system-wide
    environment.systemPackages = [
      cfg.package

      # Helper script for finding input devices
      (pkgs.writeScriptBin "bongocat-find-devices" ''
        #!${pkgs.bash}/bin/bash
        exec ${cfg.package}/bin/bongocat-find-devices "$@"
      '')
    ];

    # Create systemd user service if user is specified
    systemd.user.services.wayland-bongocat = mkIf cfg.autostart {
      enable = true;
      description = "Wayland Bongo Cat Overlay";
      wantedBy = ["graphical-session.target"];
      partOf = ["graphical-session.target"];
      after = ["graphical-session.target"];
      environment = {
        WAYLAND_DISPLAY = "wayland-0";
        XDG_RUNTIME_DIR = "/run/user/%i";
      };
      serviceConfig = {
        Type = "exec";
        ExecStart = "${cfg.package}/bin/bongocat --config ${configFile}";
        Restart = "on-failure";
        RestartSec = "5s";

        # Security settings
        NoNewPrivileges = true;
        PrivateTmp = true;
        ProtectSystem = "strict";
        ProtectHome = "read-only";
      };
    };
  };
}
