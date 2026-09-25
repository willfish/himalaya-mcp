{
  description = "Small Himalaya MCP server";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { nixpkgs, ... }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forEach = f: nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
    in
    {
      packages = forEach (pkgs: {
        default = pkgs.stdenv.mkDerivation {
          pname = "himalaya-mcp";
          version = "0.2.0";
          src = ./.;
          nativeBuildInputs = [ pkgs.pkg-config ];
          buildInputs = [ pkgs.cjson ];
          TZDIR = "${pkgs.tzdata}/share/zoneinfo";
          buildPhase = "make -j$NIX_BUILD_CORES";
          doCheck = true;
          checkPhase = "make test";
          installPhase = ''
            make install PREFIX="$out"
          '';
        };
      });
    };
}
