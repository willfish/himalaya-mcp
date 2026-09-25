# Installation

[Back to README](../README.md)

## Prebuilt releases

The curl installer supports Linux x86_64/ARM64 (glibc or musl) and macOS 13+ on
Intel or Apple Silicon. It rejects unsupported platforms rather than guessing.

```sh
curl -fsSL https://github.com/willfish/himalaya-mcp/releases/latest/download/install | sh
```

Requires curl, CA certificates, a POSIX shell, tar, `sha256sum` or `shasum`, and
standard Unix utilities. No compiler, Homebrew, cJSON package or additional timezone
package is required. Linux binaries are static; macOS binaries embed cJSON and
link only to Apple's system library.

The installer includes a private, checksum-pinned Himalaya 1.2.0 and timezone data.
It does not replace an existing `himalaya` command, configure accounts, change
credentials, invoke sudo or edit shell startup files. Mail tools require a
[configured Himalaya account](https://github.com/pimalaya/himalaya#configuration).
Compatibility with other CLI versions is not established.

## Installation paths

The default prefix is `$HOME/.local`, or `/usr/local` when run as root. This keeps
normal macOS installations out of Homebrew-owned directories and avoids requiring
administrator access.

Set an absolute `PREFIX` on the **shell side** of the pipe:

```sh
curl -fsSL https://github.com/willfish/himalaya-mcp/releases/latest/download/install \
  | PREFIX="$HOME/tools" sh
```

Use `<prefix>/bin/himalaya-mcp` as the MCP client command. The launcher locates its
private CLI and timezone data. It reads your normal Himalaya configuration.
`HIMALAYA_BINARY` and `HIMALAYA_ZONEINFO_DIR` can override those bundled paths;
most installations should leave them unset.

After configuring an account, run `<prefix>/bin/himalaya-mcp doctor`. This checks
accounts and folders, not just whether the executable starts.

## Trust and version pinning

To pin a release, use `/releases/download/v0.3.0/install` instead of
`/releases/latest/download/install`. `HIMALAYA_MCP_VERSION` selects a specific
`vMAJOR.MINOR.PATCH` payload. You can download `install`, inspect it, then run
`sh install` rather than piping it directly to a shell.

Archive checksums detect corruption against the GitHub release manifest. They are
not independent signatures or a substitute for trusting the installer and release
publisher. The macOS MCP executable is ad-hoc signed, not Developer ID signed or
notarised. The installer does not disable Gatekeeper.

## Upgrade and uninstall

Rerun the installer to upgrade. It stages both executables and checks that they
run before atomically replacing the launcher. Older payloads remain under
`<prefix>/libexec/himalaya-mcp` so running servers are not disrupted.

To uninstall a curl installation, remove `<prefix>/bin/himalaya-mcp` and the
`<prefix>/libexec/himalaya-mcp` directory. Mail configuration and local
reminder/snooze records remain untouched. See [configuration](configuration.md)
for state locations.

For other installation methods, see [Nix](nix.md) or [building from source](building.md).
