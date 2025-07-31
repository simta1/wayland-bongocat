{
  description = "Bongo Cat Wayland Overlay - A fun animated overlay that reacts to keyboard input";

  #Dependencies
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  #What to do after fetching all dependencies
  outputs = inputs:
    inputs.flake-utils.lib.eachDefaultSystem (
      system: let
        pkgs = inputs.nixpkgs.legacyPackages.${system};
        bongocat = pkgs.callPackage ./nix/default.nix {};
      in {
        formatter = pkgs.alejandra;
        devShells.default = import ./nix/shell.nix {
          inherit pkgs;
        };
        packages.default = bongocat;
      }
    )
    // {
      nixosModules.default = import ./nix/nixos-module.nix;
    };
}
