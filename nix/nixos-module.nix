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

      # Cat
      cat_x_offset=${toString cfg.catXOffset}
      cat_y_offset=${toString cfg.catYOffset}
      cat_height=${toString cfg.catHeight}

      # Overlay
      overlay_position=${toString cfg.overlayPosition}
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
    enable = mkOption {
      type = types.bool;
      default = false;
      example = true;
      description = "Enable `wayland-bongocat` overlay";
    };
    autostart = mkOption {
      type = types.bool;
      default = false;
      example = true;
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
      example = true;
      description = "Enable debug logging";
    };

    # Overlay
    overlayPosition = mkOption {
      type = types.str;
      default = "top";
      example = "bottom";
      description = "Bongocat overlay position on screen - `top` or `bottom`";
    };
    overlayHeight = mkOption {
      type = types.int;
      default = 60;
      example = 70;
      description = "Height of the entire overlay bar";
    };
    overlayOpacity = mkOption {
      type = types.int;
      default = 0;
      example = 20;
      description = "Overlay background opacity (0-255, 0 = transparent)";
    };

    # Position
    catXOffset = mkOption {
      type = types.int;
      default = 120;
      example = 200;
      description = "Horizontal offset from center position (pixels)";
    };
    catYOffset = mkOption {
      type = types.int;
      default = 0;
      example = 10;
      description = "Vertical offset from center position (pixels)";
    };

    # Size
    catHeight = mkOption {
      type = types.int;
      default = 40;
      example = 50;
      description = "Height of the bongo cat in pixels";
    };

    # Animations
    idleFrame = mkOption {
      type = types.int;
      default = 0;
      example = 1;
      description = "Frame to use when idle (0, 1, or 2)";
    };
    keypressDuration = mkOption {
      type = types.int;
      default = 150;
      example = 100;
      description = "Animation duration after keypress (ms)";
    };
    testAnimationDuration = mkOption {
      type = types.int;
      default = 200;
      example = 100;
      description = "Test animation duration (ms)";
    };
    testAnimationInterval = mkOption {
      type = types.int;
      default = 0;
      example = 10;
      description = "How often to trigger test animation (seconds, 0 = disabled)";
    };

    # Performance
    fps = mkOption {
      type = types.int;
      default = 60;
      example = 120;
      description = "Animation framerate (FPS)";
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

      # Helper scripts
      # For finding input devices
      (pkgs.writeScriptBin "bongocat-find-devices" ''
        #!${pkgs.bash}/bin/bash
        exec ${cfg.package}/bin/bongocat-find-devices "$@"
      '')

      # For starting `wayland-bongocat` using the config file defined with Nix
      (pkgs.writeScriptBin "bongocat-exec" ''
        #!${pkgs.bash}/bin/bash
        exec ${cfg.package}/bin/bongocat --config ${configFile}
      '')
    ];

    # Create systemd user service if user is specified
    systemd.user.services.wayland-bongocat = mkIf cfg.autostart {
      enable = true;
      description = "Wayland Bongo Cat Overlay";
      wantedBy = ["graphical-session.target"];
      partOf = ["graphical-session.target"];
      after = ["graphical-session.target"];
      serviceConfig = {
        Type = "exec";
        ExecStart = "${cfg.package}/bin/bongocat --config ${configFile}";
        Restart = "on-failure";
        RestartSec = "5s";
      };
    };
  };
}
