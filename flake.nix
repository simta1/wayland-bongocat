{
  description = "Bongo Cat Wayland Overlay - A fun animated overlay that reacts to keyboard input";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    flake-utils,
  }:
    flake-utils.lib.eachDefaultSystem (system: let
      pkgs = nixpkgs.legacyPackages.${system};
      bongocat = pkgs.callPackage ./nix/default.nix {};
    in {
      formatter = pkgs.alejandra;
      devShells.default = import ./nix/shell.nix {inherit pkgs;};
      packages = {
        default = bongocat;
        wayland-bongocat = bongocat;
      };
      apps = {
        default = flake-utils.lib.mkApp {
          drv = bongocat;
          name = "bongocat";
        };

        find-devices = flake-utils.lib.mkApp {
          drv = bongocat;
          name = "bongocat-find-devices";
        };
      };
    })
    // {
      nixosModules.default = import ./nix/nixos-module.nix;
    };
}
