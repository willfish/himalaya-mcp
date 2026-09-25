#pragma once

#include <stddef.h>
#include <time.h>

typedef struct {
  time_t instant;
  char iso[21];  /* YYYY-MM-DDTHH:MM:SSZ */
  char ical[17]; /* YYYYMMDDTHHMMSSZ */
} MailDate;

enum { DATE_ALLOW_BARE_TOMORROW = 1 };

/* Explicit HIMALAYA_TIMEZONE wins; otherwise detect TZ/system configuration,
   falling back to UTC. "system" means the rules in /etc/localtime, without a name.
   The returned detection buffer is process-local; use only from one thread. */
const char *date_default_zone(void);
/* Injectable system paths for deterministic detection tests. Output needs at
   least 7 bytes. Returns 0 on detection/fallback, -1 for invalid API arguments. */
int date_detect_zone(const char *tz, const char *localtime_path, const char *timezone_path,
                     char *out, size_t size);
int date_format_utc(time_t instant, MailDate *out);
/* Deterministic reference clock. Output is always UTC; no date-only defaults.
   Uses libc TZ temporarily and restores it. Call only from a single thread. */
int date_parse(const char *input, time_t now, const char *zone, unsigned flags,
               MailDate *out, char *error, size_t error_size);
