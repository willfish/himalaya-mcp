#pragma once

#include <stddef.h>
#include <time.h>

typedef struct {
  time_t instant;
  char iso[21];  /* YYYY-MM-DDTHH:MM:SSZ */
  char ical[17]; /* YYYYMMDDTHHMMSSZ */
} MailDate;

enum { DATE_ALLOW_BARE_TOMORROW = 1 };

/* HIMALAYA_TIMEZONE, or UTC when unset. IANA zone names, not abbreviations. */
const char *date_default_zone(void);
int date_format_utc(time_t instant, MailDate *out);
/* Deterministic reference clock. Output is always UTC; no date-only defaults.
   Uses libc TZ temporarily and restores it. Call only from a single thread. */
int date_parse(const char *input, time_t now, const char *zone, unsigned flags,
               MailDate *out, char *error, size_t error_size);
