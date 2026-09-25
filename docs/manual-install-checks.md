# Manual Linux installation checks

Use disposable containers, not host packages or real mail credentials. This
walkthrough targets x86_64 Linux and needs Docker, curl, tar and sha256sum on the
host. It is a manual recipe, not CI or an installation script.

## Prepare a distribution

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
