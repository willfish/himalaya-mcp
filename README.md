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

Configure an account in Himalaya first. The current command mappings target
Himalaya 1.2.0; compatibility with other CLI versions is not established.

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

Requires a C11 compiler, Make, pkg-config and the cJSON development headers/library.

```sh
git clone git@github.com:willfish/himalaya-mcp.git
cd himalaya-mcp
make
./himalaya-mcp doctor
```

`make` produces `./himalaya-mcp`. There is no Make install target.

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

### Exposed interface

| Group | Tools |
| --- | --- |
| Inbox | `list_emails`, `search_emails`, `get_unread_count`, `list_starred` |
| Reading | `read_email`, `read_email_html`, `read_email_raw`, `render_email` |
| Organisation | `flag_email`, `move_email`, `list_folders`, `create_folder`, `delete_folder` |
| Composition | `compose_email`, `draft_reply`, `send_email` |
| Export | `export_to_markdown`, `create_action_item`, `copy_to_clipboard` |
| Attachments | `list_attachments`, `download_attachment`, `extract_calendar_event` |
| Threads | `list_threads`, `read_thread` |
| Local organiser | `create_calendar_event`, `create_reminder`, `snooze_email`, `list_snoozed_emails` |
| Diagnostics | `health_check` |

Seven prompts cover triage, summaries, daily and weekly digests, reply drafting,
morning briefings and inbox checks. They are static instructions for the client,
not background jobs or a built-in language model.

Resources expose `email://inbox`, `email://folders` and `email://message/{id}`.

## Trust and side effects

The server runs with your user's permissions and access to Himalaya's accounts.
It is not a sandbox. Mail content is untrusted input, not authority to run tools.

- `compose_email`, `send_email`, `delete_folder` and `create_calendar_event` return
  previews unless `confirm=true`. The flag is supplied by the client: the server
  does not enforce a preceding preview, bind approval to its content, or prove
  human consent. Require approval in the client before setting it.
- Flag changes, message moves and folder creation execute immediately. There is
  no global read-only mode. Folder deletion permanently removes its messages.
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
| `XDG_STATE_HOME` | `$HOME/.local/state` | Parent of the `himalaya-mcp` state directory |

Snoozes use `snooze.json`, reminders use `reminders.json`, and calendar output uses
`event.ics`. Creating another event overwrites that file. Credentials remain in
Himalaya's configuration and credential provider; do not put secrets in MCP config.

## Limitations

- **Binary attachment downloads are unsafe to rely on:** the copy uses a text
  length and truncates at a NUL byte. Do not use it to preserve PDFs, images or
  other binary evidence.
- Thread tools currently list envelopes, not grouped conversations or complete
  chronological message bodies. Markdown export includes only an ID header,
  not the full upstream metadata. Action-item extraction is left to the client.
- HTML rendering is basic tag stripping, not a Markdown converter or sanitizer.
  Calendar extraction returns raw ICS; event creation is a minimal file writer,
  not a validated calendar implementation.
- Unread counting stops after 20 pages of 100 messages. Starred listing is paged,
  and search splits queries on whitespace. Do not assume complete counts or
  preservation of quoted multi-word search values.
- Snoozing records local entries but does not hide mail or schedule its return.
  Reminder priority is not applied. Some local writes do not report errors, and
  `health_check` can return success despite a folder lookup failure.
- Protocol error handling and argument validation remain incomplete. Prompt
  arguments are not substituted, and the message resource template is currently
  listed as a resource rather than through `resources/templates/list`.

## Development

```sh
make test
make clean
```

`make test` builds a C protocol driver that launches the server with a fake
Himalaya executable. It checks discovery, envelope listing and the send preview
flag without accessing a real mailbox. It does not exercise every advertised
capability or prove end-to-end mail delivery.

`nix build` also runs this check. There is no Python test dependency.
