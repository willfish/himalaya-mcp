#define _POSIX_C_SOURCE 200809L
#include "date.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef DATE_ZONEINFO_DIR
#define DATE_ZONEINFO_DIR "/usr/share/zoneinfo"
#endif

static int fail(char *error, size_t size, const char *message) {
  if (error && size) snprintf(error, size, "%s", message);
  return -1;
}

static int zonefile(const char *path) {
  FILE *file=fopen(path,"rb");
  char magic[4];
  int valid=file && fread(magic,1,4,file)==4 && !memcmp(magic,"TZif",4);
  if (file) fclose(file);
  return valid;
}

static int detected_name(const char *value, char *out, size_t size) {
  if (!value) return 0;
  if (*value==':') value++;
  const char *suffix=strstr(value,"/zoneinfo/");
  if (suffix) value=suffix+10;
  if (!*value || !strcmp(value,"UTC0") || !strcmp(value,"GMT0")) value="UTC";
  size_t length=strlen(value);
  if (!length || length>128 || length>=size || *value=='/' || strstr(value,"..")) return 0;
  for (const char *p=value; *p; p++) if (!isalnum((unsigned char)*p) && !strchr("/_+-",*p)) return 0;
  if (strcmp(value,"UTC")) {
    const char *root=getenv("HIMALAYA_ZONEINFO_DIR");
    if (!root || !*root) root=DATE_ZONEINFO_DIR;
    if (*root!='/') return 0;
    char path[1024];
    int n=snprintf(path,sizeof path,"%s/%s",root,value);
    if (n<0 || (size_t)n>=sizeof path || !zonefile(path)) return 0;
  }
  memcpy(out,value,length+1);
  return 1;
}

int date_detect_zone(const char *tz, const char *localtime_path, const char *timezone_path,
                     char *out, size_t size) {
  if (!out || size<7 || !localtime_path || !timezone_path) return -1;
  if (tz && detected_name(tz,out,size)) return 0;
  char link[1024];
  ssize_t n=readlink(localtime_path,link,sizeof link-1);
  if (n>0 && (size_t)n<sizeof link-1) {
    link[n]=0;
    /* Only infer a name from an actual zoneinfo path, not an arbitrary symlink. */
    if (strstr(link,"/zoneinfo/") && detected_name(link,out,size)) return 0;
  }
  /* A copied TZif file is authoritative even when its IANA name is unavailable.
     Do not substitute a potentially stale /etc/timezone value for its rules. */
  if (zonefile(localtime_path)) { strcpy(out,"system"); return 0; }
  FILE *file=fopen(timezone_path,"r");
  char name[160];
  if (file) {
    if (fgets(name,sizeof name,file)) {
      size_t length=strlen(name);
      while (length && isspace((unsigned char)name[length-1])) name[--length]=0;
      char *start=name; while (*start && isspace((unsigned char)*start)) start++;
      if (*start && detected_name(start,out,size)) { fclose(file); return 0; }
    }
    fclose(file);
  }
  strcpy(out,"UTC");
  return 0;
}

const char *date_default_zone(void) {
  const char *zone=getenv("HIMALAYA_TIMEZONE");
  if (zone && *zone) return zone;
  static char detected[129];
  date_detect_zone(getenv("TZ"),"/etc/localtime","/etc/timezone",detected,sizeof detected);
  return detected;
}

int date_format_utc(time_t instant, MailDate *out) {
  struct tm utc;
  if (!out || !gmtime_r(&instant, &utc) || utc.tm_year < 70 || utc.tm_year > 8099) return -1;
  if (!strftime(out->iso, sizeof out->iso, "%Y-%m-%dT%H:%M:%SZ", &utc) ||
      !strftime(out->ical, sizeof out->ical, "%Y%m%dT%H%M%SZ", &utc)) return -1;
  out->instant = instant;
  return 0;
}

static int leap(int year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
static int valid_fields(const struct tm *t) {
  const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  int year = t->tm_year + 1900;
  return year >= 1970 && year <= 9999 && t->tm_mon >= 0 && t->tm_mon < 12 &&
    t->tm_mday >= 1 && t->tm_mday <= days[t->tm_mon] + (t->tm_mon == 1 && leap(year)) &&
    t->tm_hour >= 0 && t->tm_hour < 24 && t->tm_min >= 0 && t->tm_min < 60 && t->tm_sec >= 0 && t->tm_sec < 60;
}

static int same_fields(const struct tm *a, const struct tm *b) {
  return a->tm_year == b->tm_year && a->tm_mon == b->tm_mon && a->tm_mday == b->tm_mday &&
    a->tm_hour == b->tm_hour && a->tm_min == b->tm_min && a->tm_sec == b->tm_sec;
}

/* Evaluate both DST choices, round-trip and deduplicate. mktime's automatic
   normalisation must never silently repair a nonexistent local clock time. */
static int local_instant(const struct tm *fields, time_t *out, char *error, size_t size) {
  int count = 0;
  time_t found = 0;
  for (int dst = -1; dst <= 1; dst++) {
    struct tm candidate = *fields, back;
    candidate.tm_isdst = dst;
    time_t instant = mktime(&candidate);
    if (instant == (time_t)-1 || !localtime_r(&instant, &back) || !same_fields(fields, &back)) continue;
    if (!count || instant != found) { found = instant; count++; }
  }
  if (!count) return fail(error, size, "Local time does not exist (DST gap). Choose another time or provide an explicit UTC offset.");
  if (count > 1) return fail(error, size, "Local time is ambiguous (DST overlap). Provide an explicit offset, e.g. +01:00 or +00:00.");
  *out = found;
  return 0;
}

/* Gregorian arithmetic avoids nonstandard timegm and process TZ changes when
   converting explicit offsets. Supported civil years are 1970 through 9999. */
static time_t offset_instant(const struct tm *t, int offset) {
  int year = t->tm_year + 1900;
  int64_t days = (int64_t)(year - 1970) * 365 +
    (year-1)/4 - (year-1)/100 + (year-1)/400 - (1969/4 - 1969/100 + 1969/400);
  const int before[] = {0,31,59,90,120,151,181,212,243,273,304,334};
  days += before[t->tm_mon] + (t->tm_mon > 1 && leap(year)) + t->tm_mday - 1;
  return (time_t)(days * 86400 + t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec - offset);
}

static int digits(const char **cursor, int min, int max, int *out) {
  const char *p = *cursor;
  int value = 0, count = 0;
  while (count < max && isdigit((unsigned char)*p)) { value = value*10 + *p++-'0'; count++; }
  if (count < min) return -1;
  *cursor = p; *out = value;
  return 0;
}

static void spaces(const char **p) { while (**p == ' ') (*p)++; }

static int clock_fields(const char *p, struct tm *t, int compact) {
  spaces(&p);
  if (!strcmp(p,"noon")) { t->tm_hour=12; t->tm_min=t->tm_sec=0; return 0; }
  if (!strcmp(p,"midnight")) { t->tm_hour=t->tm_min=t->tm_sec=0; return 0; }
  int hour, minute=0, second=0;
  if (digits(&p,compact?2:1,2,&hour)) return -1;
  if (compact) {
    if (digits(&p,2,2,&minute) || digits(&p,2,2,&second)) return -1;
  } else if (*p == ':') {
    p++;
    if (digits(&p,2,2,&minute)) return -1;
    if (*p == ':') { p++; if (digits(&p,2,2,&second)) return -1; }
  }
  spaces(&p);
  if (!strcmp(p,"am") || !strcmp(p,"pm")) {
    if (hour < 1 || hour > 12) return -1;
    hour = hour % 12 + (*p == 'p' ? 12 : 0); p += 2;
  }
  if (*p || hour > 23 || minute > 59 || second > 59) return -1;
  t->tm_hour=hour; t->tm_min=minute; t->tm_sec=second;
  return 0;
}

static int civil_fields(const char *p, struct tm *t) {
  int year, month, day, compact=0;
  if (strlen(p) >= 10 && p[4] == '-' && p[7] == '-') {
    if (digits(&p,4,4,&year) || *p++ != '-' || digits(&p,2,2,&month) || *p++ != '-' || digits(&p,2,2,&day)) return -1;
  } else if (strlen(p) >= 9 && p[8] == 't' && strspn(p,"0123456789") == 8) {
    if (digits(&p,4,4,&year) || digits(&p,2,2,&month) || digits(&p,2,2,&day)) return -1;
    compact=1;
  } else {
    if (digits(&p,1,2,&day) || *p++ != ' ') return -1;
    const char *begin=p;
    while (isalpha((unsigned char)*p)) p++;
    size_t length=(size_t)(p-begin);
    const char *months[]={"january","february","march","april","may","june","july","august","september","october","november","december"};
    month=0;
    for (int i=0;i<12;i++) if ((length==3 || length==strlen(months[i])) && !strncmp(begin,months[i],length)) month=i+1;
    if (!month || *p++ != ' ' || digits(&p,4,4,&year)) return -1;
  }
  /* A time is mandatory: never invent midnight for a date-only input. */
  if (*p != ' ' && *p != 't') return -1;
  p++;
  if (!strncmp(p,"at ",3)) p+=3;
  t->tm_year=year-1900; t->tm_mon=month-1; t->tm_mday=day;
  return clock_fields(p,t,compact);
}

/* Remove an explicit suffix, retaining whether the input was zone-less. */
static int take_offset(char *input, int *explicit_zone, int *offset) {
  size_t n=strlen(input), cut=n;
  *explicit_zone=0; *offset=0;
  if (n && input[n-1]=='z') cut=n-1;
  else if (n>=4 && (!strcmp(input+n-4," utc") || !strcmp(input+n-4," gmt"))) cut=n-4;
  else {
    int width=0;
    if (n>=6 && (input[n-6]=='+' || input[n-6]=='-') && input[n-3]==':') width=6;
    else if (n>=5 && (input[n-5]=='+' || input[n-5]=='-')) width=5;
    if (!width) return 0;
    const char *p=input+n-width+1;
    int hour,minute;
    if (digits(&p,2,2,&hour)) return -1;
    if (width==6 && *p++!=':') return -1;
    if (digits(&p,2,2,&minute) || *p || hour>23 || minute>59) return -1;
    cut=n-(size_t)width;
    *offset=(hour*60+minute)*60*(input[cut]=='-'?-1:1);
  }
  *explicit_zone=1;
  while (cut && input[cut-1]==' ') cut--;
  input[cut]=0;
  return 0;
}

static int duration(const char *input, int64_t *seconds) {
  const char *p=input;
  if (!strncmp(p,"in ",3)) p+=3;
  int amount;
  if (digits(&p,1,6,&amount) || amount<=0) return 0;
  spaces(&p);
  int unit=0;
  if (!strcmp(p,"m") || !strcmp(p,"minute") || !strcmp(p,"minutes")) unit=60;
  if (!strcmp(p,"h") || !strcmp(p,"hour") || !strcmp(p,"hours")) unit=3600;
  if (!strcmp(p,"d") || !strcmp(p,"day") || !strcmp(p,"days")) unit=86400;
  if (!unit || amount>36500) return 0;
  *seconds=(int64_t)amount*unit;
  return 1;
}

static int parse_in_zone(char *input, time_t now, unsigned flags, MailDate *out, char *error, size_t size) {
  int64_t seconds;
  if (duration(input,&seconds)) {
    struct tm ref;
    if (!gmtime_r(&now,&ref) || ref.tm_year<70 || ref.tm_year>8099 || date_format_utc(now+(time_t)seconds,out))
      return fail(error,size,"Relative date is outside supported years 1970-9999.");
    return 0;
  }
  int explicit_zone, offset;
  if (take_offset(input,&explicit_zone,&offset)) return fail(error,size,"Invalid UTC offset. Use Z or an offset such as +01:00.");
  struct tm fields={0};
  const char *p=NULL;
  int tomorrow=0;
  if (!strncmp(input,"tomorrow",8) && (!input[8] || input[8]==' ')) { p=input+8; tomorrow=1; }
  else if (!strncmp(input,"today",5) && (!input[5] || input[5]==' ')) p=input+5;
  if (p) {
    time_t reference=now+(explicit_zone ? offset : 0);
    if (!(explicit_zone ? gmtime_r(&reference,&fields) : localtime_r(&reference,&fields))) return fail(error,size,"Invalid reference clock.");
    if (tomorrow) {
      /* Advance the civil date, not 24 elapsed hours across a DST boundary. */
      struct tm day=fields;
      day.tm_hour=12; day.tm_min=day.tm_sec=0;
      time_t next=offset_instant(&day,0)+86400;
      struct tm nextday;
      if (!gmtime_r(&next,&nextday)) return fail(error,size,"Relative date out of range.");
      fields.tm_year=nextday.tm_year; fields.tm_mon=nextday.tm_mon; fields.tm_mday=nextday.tm_mday;
    }
    spaces(&p);
    if (!strncmp(p,"at ",3)) p+=3;
    if (!*p) {
      if (!tomorrow || !(flags & DATE_ALLOW_BARE_TOMORROW)) return fail(error,size,"Include a time, e.g. tomorrow at 9am. No midnight default is assumed.");
    } else if (clock_fields(p,&fields,0)) return fail(error,size,"Invalid time. Try tomorrow at 9am or today at 14:30.");
  } else if (civil_fields(input,&fields)) {
    return fail(error,size,"Use an explicit date and time, e.g. 2026-10-01 12:00, 1 October 2026 at noon, tomorrow at 9am, or in 2 hours. Numeric slash dates are ambiguous.");
  }
  if (!valid_fields(&fields)) return fail(error,size,"Impossible date or time. Check the month length, leap year and 24-hour clock (years 1970-9999).");
  time_t instant;
  if (explicit_zone) instant=offset_instant(&fields,offset);
  else if (local_instant(&fields,&instant,error,size)) return -1;
  if (date_format_utc(instant,out)) return fail(error,size,"Resolved date is outside supported years 1970-9999.");
  return 0;
}

int date_parse(const char *input, time_t now, const char *zone, unsigned flags,
               MailDate *out, char *error, size_t error_size) {
  if (!input || !out || !*input || strlen(input)>255) return fail(error,error_size,"Date must be nonempty and at most 255 bytes.");
  MailDate reference;
  if (date_format_utc(now,&reference)) return fail(error,error_size,"Reference clock is outside supported years 1970-9999.");
  if (!zone || !*zone) zone="UTC";
  char input_copy[256]; size_t used=0;
  for (const unsigned char *p=(const unsigned char *)input; *p; p++) {
    if (*p=='\r' || *p=='\n') return fail(error,error_size,"Date must be on one line.");
    if (*p==' ' || *p=='\t') { if (used && input_copy[used-1]!=' ') input_copy[used++]=' '; }
    else input_copy[used++]=(char)tolower(*p);
  }
  if (used && input_copy[used-1]==' ') used--;
  input_copy[used]=0;
  char tz[1024];
  if (!strcmp(zone,"UTC")) strcpy(tz,"UTC0");
  else if (!strcmp(zone,"system")) {
    if (!zonefile("/etc/localtime")) return fail(error,error_size,"System timezone data is unavailable; set HIMALAYA_TIMEZONE explicitly.");
    strcpy(tz,":/etc/localtime");
  } else {
    if (strlen(zone)>128 || *zone=='/' || strstr(zone,"..")) return fail(error,error_size,"Invalid timezone. Use an IANA name such as Europe/London, or UTC.");
    for (const char *p=zone; *p; p++) if (!isalnum((unsigned char)*p) && !strchr("/_+-",*p)) return fail(error,error_size,"Invalid IANA timezone name.");
    const char *directory=getenv("HIMALAYA_ZONEINFO_DIR");
    if (!directory || !*directory) directory=DATE_ZONEINFO_DIR;
    if (*directory!='/') return fail(error,error_size,"HIMALAYA_ZONEINFO_DIR must be an absolute path.");
    int written=snprintf(tz,sizeof tz,":%s/%s",directory,zone);
    if (written<0 || (size_t)written>=sizeof tz) return fail(error,error_size,"Timezone directory path is too long.");
    if (!zonefile(tz+1)) return fail(error,error_size,"Unknown timezone. Set HIMALAYA_TIMEZONE to an installed IANA name such as Europe/London, or UTC.");
  }
  const char *old=getenv("TZ");
  char *saved=old ? strdup(old) : NULL;
  if (old && !saved) return fail(error,error_size,"Out of memory.");
  if (setenv("TZ",tz,1)) { free(saved); return fail(error,error_size,"Could not set parsing timezone."); }
  tzset();
  MailDate parsed;
  int result=parse_in_zone(input_copy,now,flags,&parsed,error,error_size);
  if (saved) setenv("TZ",saved,1); else unsetenv("TZ");
  free(saved); tzset();
  if (!result) { *out=parsed; if (error && error_size) *error=0; }
  return result;
}
