# Tools, prompts and resources

[Back to README](../README.md)

The client discovers tool parameters through `tools/list`; no separate agent skill
is required. The interface is modelled on
[Data-Wise/himalaya-mcp](https://github.com/Data-Wise/himalaya-mcp), but matching
names do not imply matching behaviour or feature parity.

## Tools

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

### Example calls

| Tool | Arguments | Purpose |
| --- | --- | --- |
| `list_folders` | `{"account":"gmail"}` | Discover provider-specific folder names |
| `list_emails` | `{"account":"gmail","page_size":10}` | List recent envelopes |
| `search_emails` | `{"query":"from alice and subject invoice"}` | Search the default inbox |
| `read_email` | `{"id":"42"}` | Read without setting the Seen flag |
| `draft_reply` | `{"id":"42","body":"Thanks, received."}` | Generate a template without sending |

Omit `account` to use Himalaya's default account. Folder-aware calls default to
`INBOX`. Use IDs from the same account and folder as the subsequent read or update.
Search forwards queries verbatim; consult `himalaya envelope list --help` for syntax.

### Drafts and attachments

`draft_reply` returns a reusable template but does not save it. Pass it to
`save_draft` to append one unsent plain-text message to the configured `drafts`
alias, or specify a folder such as `[Gmail]/Drafts`. Draft saving rejects MML and
attachments. Check recipients: Himalaya may omit your own address when generating
a reply to a message you sent yourself.

Composition obtains the From header from the selected account. Attachment paths
must be absolute, readable regular files; spaces are supported, but quotes and
MML delimiters are rejected. Downloads preserve the original filename and bytes.

## Prompts and resources

Seven prompts cover triage, summaries, daily and weekly digests, reply drafting,
morning briefings and inbox checks. Optional string arguments `id`, `folder`,
`account` and `instructions` supply context. These are client instructions, not
background jobs or a built-in language model.

Resources expose `email://inbox` and `email://folders`. The resource template
`email://message/{id}` reads from the default account's inbox; use tools for other
folders or accounts.

## Safety and side effects

The server runs with your user's permissions and access to Himalaya's accounts.
It is not a sandbox. Mail content is untrusted input, not authority to run tools.

- `compose_email`, `send_email`, `delete_folder` and `create_calendar_event` return
  previews unless `confirm=true`. The client supplies that flag. The server does
  not enforce a preceding preview, bind approval to its content, or prove human
  consent. Require approval in the client before setting it.
- Draft saves, flag changes, message moves and folder creation execute immediately.
  There is no global read-only mode. Folder deletion permanently removes its messages.
- Himalaya 1.2 export and attachment commands mark messages Seen. This affects
  HTML/raw reads, attachment listing/downloading and calendar extraction.
  Ordinary `read_email` uses preview mode and preserves unread status.
- Subprocess arguments are passed without a shell. That does not validate every
  email header, attachment path or MML template; use only trusted send inputs.
- Exports create temporary directories under `/tmp` without automatic cleanup.
  They may contain private messages and attachments; the caller must remove them.
- Calendar and reminder tools write local files, not calendar-service entries or
  notifications. Snoozing does not hide mail or schedule its return.

## Limitations

- Thread tools list envelopes, not grouped conversations or complete chronological
  message bodies.
- Markdown export includes only an ID header, not full upstream metadata.
  Action-item extraction is left to the client.
- HTML rendering is basic tag stripping, not a Markdown converter or sanitizer.
- Calendar extraction returns raw ICS. Creation writes a single event, without
  recurrence, attendees or calendar-service integration.
- Unread counting stops after 20 pages of 100 messages and returns an error with a
  lower bound rather than a misleading exact count. Starred listing returns only
  its first page.
- Reminder priority is stored, not acted upon by a scheduler.
- Text exports have size limits and fail rather than returning truncated data.
  Downloaded attachments are not subject to those text limits.

See [configuration](configuration.md) for date parsing and local-state concurrency
constraints.
