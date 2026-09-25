# Build from source

[Back to README](../README.md)

For installation without a compiler, use the [prebuilt releases](installation.md).

## Prerequisites

Requires a C11 compiler, C library headers, Make, pkg-config and cJSON development
headers/library. Install prerequisites using your distribution's package manager
with the appropriate privileges:

```sh
# Ubuntu / Debian
apt-get update
apt-get install gcc libc6-dev make pkg-config libcjson-dev tzdata ca-certificates

# Arch: upgrade the system together with its packages
pacman -Syu --needed gcc make pkgconf cjson tzdata ca-certificates

# Fedora
dnf install gcc make pkgconf-pkg-config cjson-devel tzdata ca-certificates

# Alpine
apk add gcc musl-dev make pkgconf cjson-dev tzdata ca-certificates
```

Install [Himalaya 1.2.0](https://github.com/pimalaya/himalaya/releases/tag/v1.2.0)
separately, selecting the asset for your platform and verifying its SHA-256 digest.
Keep the cJSON runtime library, timezone data and CA certificates installed;
ordinary source builds do not bundle them. Clipboard operations also require
`wl-copy`, `xclip` or `pbcopy` and a working desktop session.

## Build and install

With Git available, clone and build as your normal user:

```sh
git clone git@github.com:willfish/himalaya-mcp.git
cd himalaya-mcp
make
make test
make install PREFIX="$HOME/.local"
```

Add `$HOME/.local/bin` to `PATH`, or use the absolute executable path in your MCP
client. Configure a Himalaya account before running:

```sh
"$HOME/.local/bin/himalaya-mcp" doctor
```

Non-Nix builds use `/usr/share/zoneinfo`. Override it at build time with
`make TZDIR=/path/to/zoneinfo`, or at runtime using `HIMALAYA_ZONEINFO_DIR`.

## Other installation layouts

The default `make install` destination is `/usr/local/bin` and normally needs root
privileges. `PREFIX` changes the installation root, `BINDIR` selects an exact
executable directory, and `DESTDIR` supports package staging.

```sh
sudo make install                            # /usr/local/bin/himalaya-mcp
make install PREFIX="$HOME/tools"            # ~/tools/bin/himalaya-mcp
make install PREFIX=/usr DESTDIR="$PWD/stage" # stage/usr/bin/himalaya-mcp
make uninstall PREFIX="$HOME/.local"
```

Use the same path overrides when uninstalling. Only the MCP executable is removed;
Himalaya, configuration and local records are left alone. The Makefile does not
install dependencies, edit shell startup files or invoke sudo.

See [development](development.md) for test and packaging details, or the
[manual installation checks](manual-install-checks.md) for isolated validation.
