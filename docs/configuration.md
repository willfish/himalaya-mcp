# Configuration and dates

[Back to README](../README.md)

## Environment

| Variable | Default | Purpose |
| --- | --- | --- |
| `HIMALAYA_BINARY` | `himalaya` on `PATH`; private CLI with curl installation | CLI executable |
| `HIMALAYA_TIMEOUT` | `60` | Timeout in seconds for each captured CLI invocation |
| `HIMALAYA_TIMEZONE` | Auto-detect, then `UTC` | Explicit IANA timezone for dates without an offset, e.g. `Europe/London` |
| `HIMALAYA_ZONEINFO_DIR` | Build-time zoneinfo path; bundled data with curl installation | Absolute timezone-data directory |
| `XDG_STATE_HOME` | `$HOME/.local/state` | Parent of the `himalaya-mcp` state directory |

Credentials remain in Himalaya's configuration and credential provider. Do not put
secrets in MCP configuration. The curl launcher supplies the CLI and timezone-data
paths automatically; overrides are optional.

## Dates

Calendars, reminders and snoozes share a parser:

| Form | Examples |
| --- | --- |
| ISO / calendar timestamp | `2026-10-01T12:00:00Z`, `20261001T120000Z` |
| Local date and time | `2026-10-01 12:00`, `2026-10-01 12:00:30` |
| English month name | `1 October 2026 at noon`, `1 Oct 2026 9:30am` |
| Relative calendar date | `today at 2pm`, `tomorrow at 9am` |
| Elapsed duration | `in 2 hours`, `in 30 minutes`, `30m`, `2h`, `1d` |

Case and extra horizontal whitespace are tolerated. Results are normalised to UTC.
Calendar previews show resolved start/end values and the timezone. Reuse those
absolute values when confirming a relative-date preview: otherwise the reference
clock advances between calls. Reminder responses and stored snooze/reminder dates
include the resolved UTC time.

`tomorrow` means the next local calendar day, including across DST changes;
`in 1 day` means exactly 24 elapsed hours. Snoozing also accepts bare `tomorrow`,
keeping the current local clock time. Other inputs must include a time: date-only
values do not silently become midnight.

Ambiguous slash dates such as `01/02/2026`, impossible dates, unknown formats and
DST gaps/overlaps are rejected with correction examples. Supply an explicit offset
to disambiguate a DST transition. Named months are English; arbitrary phrases
such as `next Friday` are not supported. Calendar end must resolve after start.

### Timezone selection

`Z`, `UTC`, `GMT` or numeric offsets such as `+01:00` and `-0500` in an input take
precedence over the configured timezone. For inputs without an offset:

1. `HIMALAYA_TIMEZONE` overrides detection.
2. A recognised `TZ` value is used next.
3. The server checks the `/etc/localtime` zoneinfo symlink, a copied
   `/etc/localtime` file, and `/etc/timezone`.
4. If no timezone is found, it uses UTC.

A copied timezone file is reported as `system` when its IANA name is unavailable,
but its actual rules still apply. Language/locale settings do not imply a timezone.
Custom POSIX `TZ` rule expressions are not inferred; use an IANA name or
`HIMALAYA_TIMEZONE`. Invalid explicit overrides fail rather than silently falling
back.

## Local state

Records live under `$XDG_STATE_HOME/himalaya-mcp`, defaulting to
`$HOME/.local/state/himalaya-mcp`. Snoozes use `snooze.json`, reminders use
`reminders.json`, and each calendar event gets a private directory containing
`event.ics`.

Writes replace files atomically and report failures. Unreadable or invalid
existing records are not overwritten. Multiple independent server processes
should not write the same state directory concurrently: updates are not locked.

These records do not schedule notifications or integrate with a calendar service.
See [tool behaviour and side effects](tools.md#safety-and-side-effects).
