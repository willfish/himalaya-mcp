# himalaya-mcp

Stdio MCP server for the [himalaya](https://github.com/pimalaya/himalaya) CLI. It speaks newline-delimited JSON-RPC and runs himalaya with `exec`, not a shell.

Mail is read through the himalaya account already configured on the machine, including Gmail over IMAP. Sending, and deleting a folder, return a preview until the same call is repeated with `confirm` set to true.

```sh
nix build
himalaya-mcp
```

`HIMALAYA_BINARY` selects the himalaya executable. `HIMALAYA_TIMEOUT` is the per-command timeout in seconds. Snoozes and reminders are stored under `$XDG_STATE_HOME/himalaya-mcp`.

`himalaya-mcp doctor` prints account and folder status without starting the MCP loop.
