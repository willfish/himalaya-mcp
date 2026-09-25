# Nix

[Back to README](../README.md)

## Install

```sh
nix profile install github:willfish/himalaya-mcp
```

Unlike the curl installer, the Nix package does not bundle Himalaya. Install
Himalaya separately and make it available on the MCP process's `PATH`, or set
`HIMALAYA_BINARY` to its absolute executable path. Command mappings target
Himalaya 1.2.0.

Configure a Himalaya account, then check it with:

```sh
himalaya-mcp doctor
```

Use the absolute installed server path in your client's MCP configuration if it
does not inherit your shell's `PATH`. An explicit CLI override looks like:

```json
{
  "mcpServers": {
    "himalaya": {
      "command": "/absolute/path/to/himalaya-mcp",
      "env": {
        "HIMALAYA_BINARY": "/absolute/path/to/himalaya"
      }
    }
  }
}
```

Do not put account secrets in the MCP configuration. Himalaya handles credentials.

## Build and develop

From the checkout:

```sh
nix build
./result/bin/himalaya-mcp doctor
nix develop
make test
```

`nix build` runs all four C suites and supplies pinned timezone data. The flake
defines Linux and macOS packages for x86_64 and aarch64; package definitions alone
are not evidence of runtime validation on every target. The native release builds
use a separate [packaging workflow](development.md#release-packaging).
