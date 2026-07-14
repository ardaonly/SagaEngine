{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  packages = [
    pkgs.python3
    pkgs.python3Packages.pyside6
  ];
}
