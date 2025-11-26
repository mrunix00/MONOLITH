{
  description = "Development environment for MONOLITH";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; };
    in
    {
      devShells.${system}.default = pkgs.mkShell {
        hardeningDisable = [ "format" ];
        buildInputs = with pkgs; [
          gcc
          gdb
          gnumake
          bison
          flex
          gmp
          libmpc
          mpfr
          texinfo
          isl
          qemu
          nasm
          grub2
          xorriso
          gnupg
        ];
      };
    };
}
