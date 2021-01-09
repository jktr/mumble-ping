{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    musl gcc
  ];
  shellHook = ''
    export CC=musl-gcc
    export CFLAGS=-static
  '';
}
