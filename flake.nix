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
          version = "0.1.0";
          src = ./.;
          nativeBuildInputs = [ pkgs.pkg-config pkgs.python3 ];
          buildInputs = [ pkgs.cjson ];
          buildPhase = ''
            $CC -std=c11 -O2 -Wall -Wextra -Werror \
              $(pkg-config --cflags libcjson) \
              -o himalaya-mcp src/main.c src/util.c src/tools.c \
              $(pkg-config --libs libcjson)
          '';
          doCheck = true;
          checkPhase = ''
            python3 tests/protocol_test.py ./himalaya-mcp
          '';
          installPhase = ''
            install -Dm755 himalaya-mcp $out/bin/himalaya-mcp
          '';
        };
      });
    };
}
