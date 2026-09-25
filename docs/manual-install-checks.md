# Manual installation checks

Use disposable containers or an isolated temporary home, not real mail credentials.
The Linux walkthrough targets x86_64 and needs Docker, curl, tar and sha256sum on
the host. macOS must be checked natively, as described below. These are manual
recipes, not a CI platform matrix.

## Check the public curl installer

Use a fresh runtime-only container for each distribution. Install only curl and
CA certificates, not the source-build prerequisites:

```sh
# Ubuntu
apt-get update && apt-get install -y --no-install-recommends curl ca-certificates
# Arch
pacman -Syu --noconfirm --needed curl ca-certificates
# Fedora
# The base image supplies curl-minimal; keep it rather than adding conflicting curl.
dnf install -y curl-minimal ca-certificates
# Alpine
apk add --no-cache curl ca-certificates
```

As a normal container user with a writable HOME, run the exact public command:

```sh
curl -fsSL https://github.com/willfish/himalaya-mcp/releases/latest/download/install | sh
```

Check `$HOME/.local/bin/himalaya-mcp`. Repeat as root for `/usr/local/bin`, and with
`PREFIX` containing spaces. Check that GCC and Make are absent. Timezone previews
must work even without `/usr/share/zoneinfo`: the release supplies its own data.
After installation, disconnect the network and use the synthetic Maildir recipe
below with the installed launcher. Do not require a separate `himalaya` command on
PATH; the launcher uses its private CLI.

Keep negative installer checks local with stubbed downloads: corrupt the archive,
fail a download, and verify that the existing launcher is unchanged and temporary
files are removed. Unsupported architectures, relative prefixes and malformed
versions must fail rather than select another binary. Check repeat installations
and that another user's system installation remains untouched.

The remaining sections cover source builds, not validation of the curl flow.

## Prepare a source-build distribution

From the checkout, choose `ubuntu:24.04`, `archlinux:base`, `fedora:43` or
`alpine:3.22`. Repeat with a fresh container for each distribution.

```sh
image=ubuntu:24.04
docker run -d --name himalaya-check "$image" sleep infinity
docker image inspect --format '{{json .RepoDigests}}' "$image"
docker exec -it himalaya-check sh
```

Inside that root shell, run only the matching prerequisite commands from the
[README](../README.md#from-source), then `exit`. The package names differ; Ubuntu
needs `libc6-dev` and Alpine needs `musl-dev`, not just GCC.

Download the real Himalaya CLI on the host. This checksum is for the **x86_64
Linux v1.2.0** release asset, not another architecture or version:

```sh
scratch=$(mktemp -d)
curl --fail --location --proto '=https' --tlsv1.2 \
  https://github.com/pimalaya/himalaya/releases/download/v1.2.0/himalaya.x86_64-linux.tgz \
  -o "$scratch/himalaya.tgz"
printf 'e04e6382e3e664ef34b01afa1a2216113194a2975d2859727647b22d9b36d4e4  %s\n' \
  "$scratch/himalaya.tgz" | sha256sum -c - || exit 1
tar -xzf "$scratch/himalaya.tgz" -C "$scratch" himalaya
docker exec himalaya-check mkdir -p /usr/local/bin /work /home/tester
docker cp "$scratch/himalaya" himalaya-check:/usr/local/bin/himalaya
tar -cf - Makefile src tests | docker cp - himalaya-check:/work
docker exec himalaya-check chown -R 10001:10001 /work /home/tester
docker network disconnect bridge himalaya-check
```

Only source and tests enter the container. Do not mount your home directory,
Himalaya configuration, SSH agent or Docker socket. Disconnecting the network
after dependency installation keeps runtime checks offline.

## Build and check installation paths

```sh
docker exec -u 10001:10001 -e HOME=/home/tester -w /work himalaya-check \
  sh -ec 'himalaya --version; make clean; make test; make install PREFIX="$HOME/.local"; ./protocol_test "$HOME/.local/bin/himalaya-mcp"'

docker exec -w /work himalaya-check make install
docker exec -u 10001:10001 -w /work himalaya-check \
  ./protocol_test /usr/local/bin/himalaya-mcp

docker exec -u 10001:10001 -e HOME=/home/tester -w /work himalaya-check \
  sh -ec 'make install PREFIX=/usr DESTDIR="$HOME/stage root"; ./protocol_test "$HOME/stage root/usr/bin/himalaya-mcp"; make uninstall PREFIX=/usr DESTDIR="$HOME/stage root"; test ! -e "$HOME/stage root/usr/bin/himalaya-mcp"'
```

Check all four C suites, not just compilation. Also exercise custom `PREFIX` and
`BINDIR` directories containing spaces, executable permissions, repeated installs
and uninstalling one prefix without removing another. `/usr` is a package-manager
prefix; test it in the container, not by overwriting host-managed files.

## Exercise the real CLI without a mail server

Open an unprivileged shell:

```sh
docker exec -it -u 10001:10001 -e HOME=/home/tester himalaya-check sh
```

Inside it, configure a synthetic Maildir account:

```sh
mkdir -p "$HOME/.config/himalaya" "$HOME/mail/INBOX/cur" \
  "$HOME/mail/INBOX/new" "$HOME/mail/INBOX/tmp"
cat > "$HOME/.config/himalaya/config.toml" <<'EOF'
[accounts.local]
default = true
email = "tester@example.invalid"
display-name = "Container Test"
backend.type = "maildir"
backend.root-dir = "/home/tester/mail"
backend.maildirpp = false
EOF
himalaya-mcp doctor
printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"list_emails","arguments":{"account":"local","folder":"INBOX"}}}' | himalaya-mcp
```

`doctor` should report the local account and INBOX; the MCP call should return an
empty list without a tool error. Add synthetic MIME files under `INBOX/new` to
exercise listing, preview reads, draft saving and attachment downloads. Compare
binary attachment hashes, including filenames containing spaces. Never configure
SMTP for these checks.

Set `HIMALAYA_TIMEZONE=Europe/London` to check calendar previews and DST rejection.
Set `XDG_STATE_HOME` to a private container directory to check reminder/snooze
persistence. Test an absolute `HIMALAYA_BINARY` with `/usr/local/bin` removed from
`PATH`, as desktop MCP clients may have a reduced environment.

These checks cover distro libraries and installation paths, not real IMAP/SMTP,
desktop clipboard integration, other CPU architectures, macOS or Windows.

## Clean up

Exit the container shell, then remove only this test container and download:

```sh
docker rm -f himalaya-check
rm -rf "$scratch"
```

## Native macOS curl check

Run on an actual Intel or Apple Silicon Mac with macOS 13+. Do not infer macOS
compatibility from Linux containers. No compiler or Homebrew is needed to install.
Use a subshell and temporary home so the check cannot replace an existing launcher:

```sh
(
  set -eu
  scratch=$(mktemp -d)
  trap 'rm -rf "$scratch"' EXIT
  export HOME="$scratch/test home"
  export XDG_CONFIG_HOME="$HOME/.config" XDG_STATE_HOME="$HOME/.local/state"
  export XDG_CACHE_HOME="$HOME/.cache" XDG_DATA_HOME="$HOME/.local/share"
  export PATH=/usr/bin:/bin
  mkdir -p "$HOME"
  curl -fsSL https://github.com/willfish/himalaya-mcp/releases/latest/download/install | sh
  printf '%s\n' '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{}}' \
    | "$HOME/.local/bin/himalaya-mcp"
)
```

Check the selected Darwin architecture, checksum success, initialization response
and paths containing spaces. Where `sha256sum` is absent, installation must use the
built-in `shasum`. Use an isolated Maildir account for real CLI checks, not the
machine's existing account. Calendar previews should follow the Mac's timezone,
with `HIMALAYA_TIMEZONE` still taking precedence. Repeat with a custom `PREFIX`.
Do not disable Gatekeeper to make a failed check pass.
