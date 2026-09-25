<div align="center">

# himalaya-mcp

**Your email, connected to your AI assistant.**

A small C server for your existing [Himalaya](https://github.com/pimalaya/himalaya) accounts.
No Node.js or Python runtime. No new credential store. No hosted middleware.

[![Release](https://img.shields.io/github/v/release/willfish/himalaya-mcp?style=flat-square)](https://github.com/willfish/himalaya-mcp/releases/latest)
[![C](https://img.shields.io/badge/written_in-C-555?style=flat-square)](docs/development.md)
[![Platforms](https://img.shields.io/badge/Linux_%7C_macOS-x86__64_%7C_ARM64-5277c3?style=flat-square)](#quick-start)
[![MCP](https://img.shields.io/badge/MCP-stdio-0f766e?style=flat-square)](#connect-your-client)

[Get started](#quick-start) · [What it does](#what-it-does) · [Documentation](#documentation)

</div>

## Why this exists

Himalaya already handles email accounts, authentication, IMAP and SMTP. An MCP
server should connect that work to your assistant, not build another email stack.

| | What you get |
| --- | --- |
| **Less to install** | A compiled C server and a private Himalaya CLI. No language runtime or compiler needed with the curl installer. |
| **Accounts you already use** | Reuses Himalaya configuration and credentials, including Gmail accounts. No separate provider API setup. |
| **Portable downloads** | Linux and macOS binaries for x86_64 and ARM64, with checksum verification and timezone data included. |
| **Real integration checks** | Tested with real Himalaya, synthetic messages and attachments, and manual cross-distribution installs, not just mocked commands. |

Manual curl-install checks cover Ubuntu, Arch, Fedora, Alpine and an Apple Silicon
Mac. See [verification coverage](docs/development.md#verification-coverage) for the
release versions, test scope and remaining gaps.

## Quick start

- **Linux:** x86_64 or ARM64, including glibc and musl distributions.
- **macOS:** Intel or Apple Silicon, macOS 13 or newer.

```sh
curl -fsSL https://github.com/willfish/himalaya-mcp/releases/latest/download/install | sh
```

Installs to `~/.local/bin/himalaya-mcp` by default. No sudo or Homebrew needed.
Your existing `himalaya` command and account settings are left alone.

You need a [configured Himalaya account](https://github.com/pimalaya/himalaya#configuration)
to use the mail tools. Existing accounts are reused. Check the connection with:

```sh
"$HOME/.local/bin/himalaya-mcp" doctor
```

Other options: [Nix](docs/nix.md) · [Build from source](docs/building.md) ·
[Custom paths, upgrades and uninstalling](docs/installation.md)

### Connect your client

Add this to your client's MCP configuration, using the **absolute launcher path
printed by the installer** rather than `~`. Then reload its MCP connections.

```json
{
  "mcpServers": {
    "himalaya": {
      "command": "/home/you/.local/bin/himalaya-mcp"
    }
  }
}
```

On macOS, the path normally starts with `/Users/you/`. The curl launcher already
locates its bundled Himalaya CLI; no environment overrides are needed.

## What it does

Search and read mail, draft replies, send messages, organise folders and
flags, download attachments, and export messages or calendar files.

Try asking your assistant:

> Find the invoices Alice sent this month.
>
> Draft a reply to this message, but don't send it.
>
> Download the attachments from this email.

[Explore the tools, prompts and resources →](docs/tools.md)

## Documentation

[Installation](docs/installation.md) · [Nix](docs/nix.md) ·
[Configuration and dates](docs/configuration.md) · [Tool reference](docs/tools.md) ·
[Building](docs/building.md) · [Development](docs/development.md)
