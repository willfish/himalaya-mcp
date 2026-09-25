# Development

[Back to README](../README.md)

## Architecture

The MCP client talks to the C server over stdio. The server invokes the Himalaya
CLI, which handles account configuration, authentication and mail-provider access.
The server uses libc and cJSON; Himalaya remains a separate executable dependency.
Gmail uses the account configured in Himalaya, not a separate Gmail API integration.

Transport is newline-delimited JSON-RPC on stdin/stdout. There is no listening
network port. Running the server without a client waits for protocol input.
Subprocesses receive argument vectors without a shell; message templates are
passed through stdin.

`src/date.c` contains the shared date parser. It temporarily changes and restores
libc's timezone state and is intended for this single-threaded server, not
concurrent callers.

## Tests

Use the [source prerequisites](building.md) or enter `nix develop`, then run:

```sh
make test
make clean
```

Four C suites cover:

| Suite | Scope |
| --- | --- |
| Protocol | Tools, prompts, resources/templates, argument types, malformed requests and notifications |
| Mail | Template decoding, stdin-based sends, header validation, attachment paths, binary preservation and unsent drafts |
| Capabilities | Tool handlers, local persistence, calendar fields, failure paths and isolated clipboard helpers |
| Dates | Deterministic reference clocks, formats, UTC normalisation, offsets, leap years, rollovers, DST and invalid input |

Protocol tests launch the server with a fake Himalaya executable. The suites do
not use real mail accounts or the desktop clipboard. They do not establish
end-to-end delivery or desktop integration. No Python test dependency is required.
`nix build` also runs all four suites.

## Verification coverage

Integration checks complement the isolated tests. They use synthetic mail rather
than personal messages.

| Check | Evidence scope |
| --- | --- |
| Linux curl installation | v0.2.1 manually installed through the public URL in Ubuntu 24.04, Arch, Fedora 43 and Alpine 3.22 containers, followed by real Himalaya Maildir operations |
| Current Linux installer | v0.3.0 public curl installation and initialization checked directly on Linux; the four-distribution container checks were not repeated for that release |
| macOS curl installation | v0.3.0 manually checked on an Apple Silicon Mac, using system checksum tools, default and custom paths, synthetic Maildir operations and timezone overrides |
| Mail-provider integration | Synthetic Gmail send/read, draft persistence and binary attachment round trips checked through real Himalaya |
| Release targets | Native builds and all four C suites on Linux and macOS, each on x86_64 and ARM64 |

Maildir checks include listing and reading messages, unsent drafts, binary
attachment integrity and local organiser records. Installer checks include paths
with spaces and preserving an existing launcher on checksum failure.

These checks are not a claim that every release was manually tested on every OS,
or that every mail provider and desktop integration is covered. The macOS public
installer was manually exercised on Apple Silicon, not Intel hardware. Clipboard
integration and live external delivery were not exercised by the installation
checks. Full OS checks remain manual, not a recurring distro CI matrix.

Use the [manual installation recipes](manual-install-checks.md) to repeat checks
without real credentials.

## Release packaging

- Linux artifacts use static musl/cJSON builds from `packaging/Dockerfile`.
- macOS artifacts use `packaging/build-macos`, checksum-pinned cJSON source and
  Apple's system library. The deployment target is macOS 13. Executables are
  ad-hoc signed, not notarised.
- Both package timezone data and dependency licence notices. The `install` script
  separately downloads checksum-pinned Himalaya 1.2.0 for the selected platform.
- `.github/workflows/release.yml` builds and tests all four native targets before
  publishing the archives, `SHA256SUMS` and the installer named `install`.

For a release, keep the installer default, server version, Nix package version and
pinned installation examples in sync. Publish a version tag, then manually check
the public curl installation path. Do not replace those checks with assumptions
based on a successful source build.
