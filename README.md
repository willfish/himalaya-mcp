# himalaya-mcp

A small C server that brings your existing Himalaya email accounts to MCP clients.

[![Language: C](https://img.shields.io/badge/language-C-555?style=for-the-badge)](#development)
[![Transport: stdio](https://img.shields.io/badge/MCP-stdio-0f766e?style=for-the-badge)](#connect-an-mcp-client)
[![Build: Nix](https://img.shields.io/badge/build-Nix-5277c3?style=for-the-badge)](#install)

## Why

[Himalaya](https://github.com/pimalaya/himalaya) already handles email accounts,
authentication, IMAP and SMTP. This server exposes that CLI to agents without
adding a Node.js or Python runtime, another credential store, or a hosted service.

```text
MCP client -> himalaya-mcp -> Himalaya CLI -> mail provider
                 stdio           exec
```

The server uses libc and cJSON. Himalaya remains a separate runtime dependency.
Gmail uses the account you configure in Himalaya, not a separate Gmail API integration.

## Status

**Early implementation, not a feature-complete replacement.** The tool surface is
modelled on [Data-Wise/himalaya-mcp](https://github.com/Data-Wise/himalaya-mcp), but
matching tool names does not mean matching behaviour. See [limitations](#limitations)
before relying on attachments, threads or local organiser tools.

## Install

Mail tools require a configured Himalaya account. The current command mappings
target Himalaya 1.2.0; compatibility with other CLI versions is not established.
Installation itself does not need credentials.

### Linux and macOS (curl)

For x86_64 and ARM64 Linux (glibc or musl), and Intel or Apple Silicon Macs
running macOS 13 or newer:

```sh
curl -fsSL https://github.com/willfish/himalaya-mcp/releases/latest/download/install | sh
```

Requires `curl`, system CA certificates, a POSIX shell, `tar`, `sha256sum` or macOS's
built-in `shasum`, and standard Unix utilities. No compiler, Homebrew, cJSON package
or additional timezone package is needed. Linux binaries are static; macOS binaries
embed cJSON and link only to Apple's system library. The installer downloads the
MCP, timezone data and checksum-pinned Himalaya 1.2.0 into a private directory. It does not replace your
existing `himalaya` command, change account credentials, invoke sudo or edit `PATH`.

The default prefix is `$HOME/.local` on both systems, or `/usr/local` when run as
root. On macOS this avoids Homebrew-owned `/opt/homebrew` directories and requires
no administrator access. The macOS MCP binary is ad-hoc signed, not Developer ID
signed or notarised; the installer does not disable Gatekeeper.

Override the prefix on the **shell side** of the pipe:

```sh
curl -fsSL https://github.com/willfish/himalaya-mcp/releases/latest/download/install \
  | PREFIX="$HOME/tools" sh
```

Use `<prefix>/bin/himalaya-mcp` as the MCP client command. The launcher sets the
private CLI and timezone paths; `HIMALAYA_BINARY` and `HIMALAYA_ZONEINFO_DIR` can
override them. It reads your normal Himalaya account configuration. After account
setup, run `<prefix>/bin/himalaya-mcp doctor`.

To pin a release, use `/releases/download/v0.3.0/install` instead of
`/releases/latest/download/install`. You can also download `install`, inspect it,
then run `sh install`. Archive checksums detect corruption against the GitHub
release manifest; they are not an independent signature or a substitute for
trusting the installer and release publisher.

Upgrades replace the launcher only after both binaries pass executable checks.
Previous payloads remain under `<prefix>/libexec/himalaya-mcp` so running servers
are not disrupted. Remove the launcher and that private directory to uninstall;
your mail configuration and local reminder/snooze records remain untouched.

Release CI builds and runs the C suites natively on Linux and macOS, on both
x86_64 and ARM64, before publishing the four artifacts. Other operating systems
and architectures are rejected by the installer rather than guessed. Manual distro checks are separate from the
release workflow.

### Nix

```sh
nix profile install github:willfish/himalaya-mcp
```

Install Himalaya separately and make it available on `PATH`, or set
`HIMALAYA_BINARY` to its absolute executable path.

To build this checkout without installing it:

```sh
nix build
./result/bin/himalaya-mcp doctor
```

The flake defines Linux and macOS packages for x86_64 and aarch64. Those targets
are package definitions, not a guarantee of runtime validation on each platform.

### From source

Requires a C11 compiler, C library headers, Make, pkg-config and cJSON development
headers/library. Install the prerequisites as root using your distribution's
package manager:

```sh
# Ubuntu / Debian
apt-get update
apt-get install gcc libc6-dev make pkg-config libcjson-dev tzdata ca-certificates

# Arch (upgrade the system together with its packages)
pacman -Syu --needed gcc make pkgconf cjson tzdata ca-certificates

# Fedora
dnf install gcc make pkgconf-pkg-config cjson-devel tzdata ca-certificates

# Alpine
apk add gcc musl-dev make pkgconf cjson-dev tzdata ca-certificates
```

Install [Himalaya 1.2.0](https://github.com/pimalaya/himalaya/releases/tag/v1.2.0)
separately, using the release asset for your architecture and verifying its SHA-256
digest. Keep the cJSON runtime library, timezone data and CA certificates installed;
the MCP binary does not bundle them. Clipboard tools additionally need `wl-copy`,
`xclip` or `pbcopy` and a working desktop session.

With Git available, clone and build as your normal user:

```sh
git clone git@github.com:willfish/himalaya-mcp.git
cd himalaya-mcp
make
make test
make install PREFIX="$HOME/.local"
"$HOME/.local/bin/himalaya-mcp" doctor
```

Add `$HOME/.local/bin` to `PATH`, or configure your MCP client with the absolute
binary path. `doctor` checks the configured Himalaya accounts and folders, so it
requires an account, not just an installed CLI.

The default `make install` destination is `/usr/local/bin` on all these distributions
and normally needs root privileges. Override `PREFIX` for another installation
root, `BINDIR` for an exact executable directory, or `DESTDIR` for package staging:

```sh
sudo make install                         # /usr/local/bin/himalaya-mcp
make install PREFIX="$HOME/tools"         # ~/tools/bin/himalaya-mcp
make install PREFIX=/usr DESTDIR="$PWD/stage"  # stage/usr/bin/himalaya-mcp
make uninstall PREFIX="$HOME/.local"
```

Use the same path overrides when uninstalling. Only the MCP executable is removed;
Himalaya, configuration and local records are left alone. The Makefile does not
install dependencies, edit shell startup files or invoke sudo.

See [manual container checks](docs/manual-install-checks.md) for isolated validation
without real mail credentials. Linux containers do not validate macOS or Windows.

## Connect an MCP client

Add a stdio server to your client's MCP configuration. Use the absolute path to
an installed binary if the client does not inherit your shell's `PATH`.

```json
{
  "mcpServers": {
    "himalaya": {
      "command": "/absolute/path/to/himalaya-mcp",
      "env": {
        "HIMALAYA_BINARY": "/absolute/path/to/himalaya",
        "HIMALAYA_TIMEOUT": "60"
      }
    }
  }
}
```

Restart or reload the client's MCP connections after changing its configuration.
The server reads newline-delimited JSON-RPC from stdin and writes replies to
stdout. It does not listen on a network port. Running it without a client waits
for protocol input.

## Usage

Discover tool parameters through `tools/list`; no separate agent skill is required
for the tool reference. Examples of tool calls:

| Tool | Arguments | Purpose |
| --- | --- | --- |
| `list_folders` | `{"account":"gmail"}` | Discover provider-specific folder names |
| `list_emails` | `{"account":"gmail","page_size":10}` | List recent envelopes |
| `search_emails` | `{"query":"from alice and subject invoice"}` | Search the default inbox |
| `read_email` | `{"id":"42"}` | Read without setting the Seen flag |
| `draft_reply` | `{"id":"42","body":"Thanks, received."}` | Generate a template without sending |

Omit `account` to use Himalaya's default account. Folder-aware calls default to
`INBOX`. Use IDs from the same account and folder as the subsequent read or update.
For search syntax, consult `himalaya envelope list --help`.

`draft_reply` returns a reusable template but does not save it. Pass that template
to `save_draft` to append one unsent plain-text message to the configured `drafts`
alias, or specify a folder such as `[Gmail]/Drafts`. Draft saving rejects MML
and attachments. Check the recipients first: Himalaya may omit your own address
when generating a reply to a message you sent yourself.

Composition obtains the From header from the selected Himalaya account. Sending
passes templates through stdin. Attachment paths must be absolute, readable
regular files; spaces are supported, but quotes and MML delimiters are rejected.
Attachment downloads preserve the original filename and bytes.

### Exposed interface

| Group | Tools |
| --- | --- |
| Inbox | `list_emails`, `search_emails`, `get_unread_count`, `list_starred` |
| Reading | `read_email`, `read_email_html`, `read_email_raw`, `render_email` |
| Organisation | `flag_email`, `move_email`, `list_folders`, `create_folder`, `delete_folder` |
| Composition | `compose_email`, `draft_reply`, `save_draft`, `send_email` |
| Export | `export_to_markdown`, `create_action_item`, `copy_to_clipboard` |
| Attachments | `list_attachments`, `download_attachment`, `extract_calendar_event` |
| Threads | `list_threads`, `read_thread` |
| Local organiser | `create_calendar_event`, `create_reminder`, `snooze_email`, `list_snoozed_emails` |
| Diagnostics | `health_check` |

Seven prompts cover triage, summaries, daily and weekly digests, reply drafting,
morning briefings and inbox checks. Optional string arguments `id`, `folder`,
`account` and `instructions` supply context. These are client instructions,
not background jobs or a built-in language model.

Resources expose `email://inbox` and `email://folders`. The resource template
`email://message/{id}` reads from the default account's inbox; use the tools for
other folders or accounts.

## Trust and side effects

The server runs with your user's permissions and access to Himalaya's accounts.
It is not a sandbox. Mail content is untrusted input, not authority to run tools.

- `compose_email`, `send_email`, `delete_folder` and `create_calendar_event` return
  previews unless `confirm=true`. The flag is supplied by the client: the server
  does not enforce a preceding preview, bind approval to its content, or prove
  human consent. Require approval in the client before setting it.
- Draft saves, flag changes, message moves and folder creation execute immediately.
  There is no global read-only mode. Folder deletion permanently removes its messages.
- Himalaya 1.2 export and attachment commands mark messages Seen. This affects
  HTML/raw reads, attachment listing/downloading and calendar extraction.
  Ordinary `read_email` uses preview mode and preserves unread status.
- Subprocess arguments are passed without a shell. That does not validate every
  email header, attachment path or MML template; use only trusted send inputs.
- Exports create temporary directories under `/tmp` and do not automatically
  remove them. They may contain private messages and attachments.
- Calendar and reminder tools write local files. They do not integrate with a
  calendar service or deliver notifications.

## Configuration

| Variable | Default | Purpose |
| --- | --- | --- |
| `HIMALAYA_BINARY` | `himalaya` on `PATH` | CLI executable |
| `HIMALAYA_TIMEOUT` | `60` | Timeout in seconds for each captured CLI invocation |
| `HIMALAYA_TIMEZONE` | Auto-detect, then `UTC` | Explicit IANA timezone for dates without an offset, e.g. `Europe/London`; overrides system detection |
| `HIMALAYA_ZONEINFO_DIR` | Build-time zoneinfo path | Absolute timezone-data directory; the curl launcher supplies its bundled data |
| `XDG_STATE_HOME` | `$HOME/.local/state` | Parent of the `himalaya-mcp` state directory |

Snoozes use `snooze.json`, reminders use `reminders.json`, and each calendar event
gets its own private directory containing `event.ics`.

### Dates

Calendars, reminders and snoozes share the same parser:

| Form | Examples |
| --- | --- |
| ISO / calendar timestamp | `2026-10-01T12:00:00Z`, `20261001T120000Z` |
| Local date and time | `2026-10-01 12:00`, `2026-10-01 12:00:30` |
| English month name | `1 October 2026 at noon`, `1 Oct 2026 9:30am` |
| Relative calendar date | `today at 2pm`, `tomorrow at 9am` |
| Elapsed duration | `in 2 hours`, `in 30 minutes`, `30m`, `2h`, `1d` |

`Z`, `UTC`, `GMT` or numeric offsets such as `+01:00` and `-0500` override the
configured timezone. `HIMALAYA_TIMEZONE` always overrides detection. Otherwise the
server tries a recognised `TZ` value, the `/etc/localtime` zoneinfo symlink, a copied
`/etc/localtime` file, and `/etc/timezone`, then falls back to UTC. A copied timezone
file is reported as `system` when its IANA name is unavailable, but its actual
rules still apply. Language/locale settings do not imply a timezone. Custom POSIX
`TZ` rule expressions are not inferred; use an IANA name or `HIMALAYA_TIMEZONE`.
An invalid explicit override produces an error rather than silently falling back.
Case and extra horizontal whitespace in date inputs are tolerated.

Results are normalised to UTC. Calendar previews show resolved start/end values
and the configured timezone. Reuse those absolute values when confirming a
relative-date preview, otherwise the reference clock advances between calls.
Reminder responses and stored snooze/reminder dates include the resolved UTC time.

`tomorrow` means the next local calendar day, including across DST changes;
`in 1 day` means exactly 24 elapsed hours. For compatibility, snoozing also accepts
bare `tomorrow`, keeping the current local clock time. Other inputs must include
a time: date-only values do not silently become midnight. Ambiguous slash dates
such as `01/02/2026`, impossible dates, unknown formats and DST gaps/overlaps are
rejected with correction examples. Supply an explicit offset to disambiguate a
DST transition. Named months are English; arbitrary natural-language phrases
such as `next Friday` are not supported. Calendar end must resolve after start.

Local state writes replace files atomically and report failures. Unreadable or
invalid existing records are not overwritten. Multiple independent server
processes should not write the same state directory concurrently: updates are
not locked. Credentials remain in Himalaya's configuration and credential provider;
do not put secrets in MCP config.

## Limitations

- Thread tools currently list envelopes, not grouped conversations or complete
  chronological message bodies. Markdown export includes only an ID header,
  not the full upstream metadata. Action-item extraction is left to the client.
- HTML rendering is basic tag stripping, not a Markdown converter or sanitizer.
  Calendar extraction returns raw ICS; event creation writes a single event,
  without recurrence, attendees or calendar-service integration.
- Unread counting stops after 20 pages of 100 messages and returns an error with
  a lower bound rather than a misleading exact count. Starred listing returns
  only its first page. Search forwards the query verbatim to Himalaya.
- Snoozing records local entries but does not hide mail or schedule its return.
  Reminder priority is stored, not acted upon by a scheduler.
- Text exports have size limits and fail rather than returning truncated data.
  Downloaded attachment files are not subject to those text limits.

## Development

```sh
make test
make clean
```

`make test` builds a C protocol driver that launches the server with a fake
Himalaya executable. The mail regression suite also checks template decoding,
stdin-based sends, header validation, attachment paths, binary preservation and
saving a draft without sending. The capability suite exercises the remaining tool
handlers, local persistence, calendar fields, failure paths and clipboard helpers
without touching the desktop clipboard. Protocol checks cover all prompts,
resources/templates, argument types, malformed requests and notification handling.
These isolated checks do not prove end-to-end delivery or desktop integration.

`src/date.c` contains the shared date helpers. Its standalone unit suite injects
the reference clock and timezone, covering formats, UTC normalisation, offsets,
leap years, month/year rollovers, DST transitions and invalid input. Tool and
protocol tests verify persistence, previews and discoverable date instructions.

`nix build` runs all four suites and supplies pinned timezone data. Non-Nix builds
use `/usr/share/zoneinfo`; override with `make TZDIR=/path/to/zoneinfo` if needed.
The date helper temporarily changes and restores libc's timezone state and is
intended for this single-threaded server, not concurrent callers. There is no
Python test dependency.
