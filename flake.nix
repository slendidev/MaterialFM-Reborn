{
  nixConfig = {
    extra-substituters = [ "https://pspdev.cachix.org" ];
    extra-trusted-public-keys = [ "pspdev.cachix.org-1:lFw1M0EYJeN3Y2xHR7spiuPmThrNDXo8Z9I0Jgzig/0=" ];
  };

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-parts.url = "github:hercules-ci/flake-parts";
    pspdev = {
      url = "github:pspdev/pspdev-nix";
      #url = "git+file:///Users/lain/Documents/Code/pspdev-nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = inputs@{ nixpkgs, flake-parts, ... }:
    flake-parts.lib.mkFlake { inherit inputs; } {
      systems = nixpkgs.lib.systems.flakeExposed;

      perSystem = { system, ... }:
        let
          pkgs = import nixpkgs {
            inherit system;
            overlays = [ inputs.pspdev.overlays.default ];
          };
          nativeBuildInputs = with pkgs; [
            imagemagick
            oxipng
          ];
        in
        {
          packages.default = pkgs.pspMkDerivation {
            pname = "MaterialFM";
            version = "0.1.0";
            src = ./.;
            buildSystem = "cmake";

            inherit nativeBuildInputs;

            preBuild = ''
              ./scripts/generate_icon_atlas.sh
            '';

            postInstall = ''
              rm -rfv assets/Textures/Icons
              cp -rv assets $out/assets
            '';
          };

          devShells.default = pkgs.mkShell {
            inputsFrom = [ inputs.pspdev.devShells.${system}.default ];
            packages = nativeBuildInputs;
          };
        };
    };
}
