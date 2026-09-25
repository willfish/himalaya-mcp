#define _POSIX_C_SOURCE 200809L
#ifdef __APPLE__
/* Darwin exposes mkdtemp through its extended unistd.h declarations. */
#define _DARWIN_C_SOURCE
#endif
#include "date.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef DATE_ZONEINFO_DIR
#define DATE_ZONEINFO_DIR "/usr/share/zoneinfo"
#endif

#define NOW ((time_t)1790339696) /* 2026-09-25 12:34:56Z */
#define SPRING_EVE ((time_t)1774688400) /* 2026-03-28 09:00Z */
#define AUTUMN_EVE ((time_t)1792828800) /* 2026-10-24 08:00Z */

static unsigned checks;
static void check(int ok, const char *what) {
  checks++;
  if (!ok) { fprintf(stderr,"date test failed: %s\n",what); exit(1); }
}

static void resolves(const char *input, const char *zone, time_t now, unsigned flags, const char *expected) {
  MailDate value, formatted;
  char error[256];
  int result=date_parse(input,now,zone,flags,&value,error,sizeof error);
  if (result) fprintf(stderr,"%s [%s]: %s\n",input,zone,error);
  check(!result,input);
  if (strcmp(value.iso,expected)) fprintf(stderr,"%s: got %s; expected %s\n",input,value.iso,expected);
  check(!strcmp(value.iso,expected),"normalised UTC ISO timestamp");
  check(!date_format_utc(value.instant,&formatted) && !strcmp(formatted.iso,expected),"instant agrees with UTC output");
  char compact[17]; size_t w=0;
  for (const char *p=expected; *p; p++) if (*p!='-' && *p!=':') compact[w++]=*p;
  compact[w]=0;
  check(!strcmp(value.ical,compact),"normalised calendar timestamp");
  check(!*error,"success clears error");
}

static void rejects(const char *input, const char *zone, time_t now, unsigned flags, const char *reason) {
  MailDate value={.instant=123}; strcpy(value.iso,"unchanged");
  char error[256];
  int result=date_parse(input,now,zone,flags,&value,error,sizeof error);
  if (!result) fprintf(stderr,"unexpected acceptance: %s -> %s\n",input,value.iso);
  check(result<0,input ? input : "null input");
  check(*error && (!reason || strstr(error,reason)),"useful error reason");
  check(value.instant==123 && !strcmp(value.iso,"unchanged"),"failure leaves output unchanged");
}

static void absolute_formats(void) {
  const char *equivalent[]={
    "2026-10-01T12:00:00Z", "20261001T120000Z", "2026-10-01 12:00 UTC",
    "2026-10-01 12:00 GMT", "1 October 2026 at noon UTC", "01 Oct 2026 12pm UTC",
    "2026-10-01T13:00:00+01:00", "2026-10-01 07:00-0500",
    "2026-10-01 17:45+05:45", "  1\tOCTOBER  2026 at 12:00 pm  UTC  "
  };
  for (size_t i=0;i<sizeof equivalent/sizeof *equivalent;i++) resolves(equivalent[i],"Europe/London",NOW,0,"2026-10-01T12:00:00Z");
  resolves("2026-10-01 12:00","Europe/London",NOW,0,"2026-10-01T11:00:00Z");
  resolves("1 October 2026 at noon","Europe/London",NOW,0,"2026-10-01T11:00:00Z");
  resolves("20261001T120000","UTC",NOW,0,"2026-10-01T12:00:00Z");
  resolves("1 October 2026 at midnight","Europe/London",NOW,0,"2026-09-30T23:00:00Z");
  resolves("1 October 2026 at 12am","UTC",NOW,0,"2026-10-01T00:00:00Z");
  resolves("2026-12-31 23:45-01:00","UTC",NOW,0,"2027-01-01T00:45:00Z");
  resolves("2026-01-01 00:30+01:00","UTC",NOW,0,"2025-12-31T23:30:00Z");
  resolves("2028-02-29 14:30:15Z","UTC",NOW,0,"2028-02-29T14:30:15Z");
  resolves("2400-02-29 00:00Z","UTC",NOW,0,"2400-02-29T00:00:00Z");
}

static void named_months(void) {
  const char *months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
  for (int i=0;i<12;i++) {
    char input[80], expected[32];
    snprintf(expected,sizeof expected,"2026-%02d-12T12:00:00Z",i+1);
    snprintf(input,sizeof input,"12 %s 2026 at noon",months[i]);
    resolves(input,"UTC",NOW,0,expected);
    snprintf(input,sizeof input,"12 %.3s 2026 at noon",months[i]);
    resolves(input,"UTC",NOW,0,expected);
  }
}

static void relative_dates(void) {
  resolves("in 2 hours","Europe/London",NOW,0,"2026-09-25T14:34:56Z");
  resolves("2h","UTC",NOW,0,"2026-09-25T14:34:56Z");
  resolves("in 30 minutes","UTC",NOW,0,"2026-09-25T13:04:56Z");
  resolves("1m","UTC",NOW,0,"2026-09-25T12:35:56Z");
  resolves("in 1 day","UTC",NOW,0,"2026-09-26T12:34:56Z");
  resolves("2d","UTC",NOW,0,"2026-09-27T12:34:56Z");
  resolves("tomorrow at 9am","Europe/London",NOW,0,"2026-09-26T08:00:00Z");
  resolves("today at 2:30pm","Europe/London",NOW,0,"2026-09-25T13:30:00Z");
  resolves("tomorrow","Europe/London",NOW,DATE_ALLOW_BARE_TOMORROW,"2026-09-26T12:34:56Z");
  /* The local date is already Sep 26 in London, but still Sep 25 in UTC. */
  resolves("today at noon","Europe/London",1790379000,0,"2026-09-26T11:00:00Z");
  resolves("today at noon UTC","Europe/London",1790379000,0,"2026-09-25T12:00:00Z");
  resolves("tomorrow at 9am","UTC",1798759800,0,"2027-01-01T09:00:00Z");
  resolves("tomorrow at 9am","UTC",1835344800,0,"2028-02-29T09:00:00Z");
  resolves("tomorrow at 9am","UTC",1772272800,0,"2026-03-01T09:00:00Z");
}

static void daylight_saving(void) {
  resolves("tomorrow at 9am","Europe/London",SPRING_EVE,0,"2026-03-29T08:00:00Z");
  resolves("tomorrow","Europe/London",SPRING_EVE,DATE_ALLOW_BARE_TOMORROW,"2026-03-29T08:00:00Z");
  resolves("in 1 day","Europe/London",SPRING_EVE,0,"2026-03-29T09:00:00Z");
  resolves("tomorrow at 9am","Europe/London",AUTUMN_EVE,0,"2026-10-25T09:00:00Z");
  resolves("in 1 day","Europe/London",AUTUMN_EVE,0,"2026-10-25T08:00:00Z");
  rejects("2026-03-29 01:30","Europe/London",NOW,0,"DST gap");
  rejects("2026-10-25 01:30","Europe/London",NOW,0,"DST overlap");
  rejects("tomorrow at 1:30am","Europe/London",SPRING_EVE,0,"DST gap");
  rejects("tomorrow at 1:30am","Europe/London",AUTUMN_EVE,0,"DST overlap");
  resolves("2026-10-25 01:30+01:00","Europe/London",NOW,0,"2026-10-25T00:30:00Z");
  resolves("2026-10-25 01:30+00:00","Europe/London",NOW,0,"2026-10-25T01:30:00Z");
  resolves("2026-03-29 01:30Z","Europe/London",NOW,0,"2026-03-29T01:30:00Z");
  rejects("2026-11-01 01:30","America/New_York",NOW,0,"DST overlap");
  rejects("2026-03-08 02:30","America/New_York",NOW,0,"DST gap");
}

static void invalid_inputs(void) {
  const char *bad[]={"", " ", "01/02/2026 12:00", "2026-10-01", "1 October 2026", "2026-02-29 12:00", "2100-02-29 12:00", "2026-04-31 12:00", "2026-13-01 12:00", "2026-00-01 12:00", "2026-01-00 12:00", "2026-01-01 24:00", "2026-01-01 12:60", "2026-01-01 12:00:60", "2026-01-01 0pm", "2026-01-01 13am", "2026-01-01 12:00+24:00", "2026-01-01 12:00+01:60", "2026-01-01 12:00+1:00", "2026-01-01 12:00+ 1:00", "2026-01-01 12:00 BST", "2026-01-01 12:00Zjunk", "next Friday", "tomorrow", "today", "tomorrow at", "in 0 hours", "-2h", "1.5h", "99999999999999d", "in 36501 days", "2026-01-01\n12:00", "1969-12-31 23:00Z", "9999-12-31 23:59-01:00"};
  for (size_t i=0;i<sizeof bad/sizeof *bad;i++) rejects(bad[i],"UTC",NOW,0,NULL);
  rejects(NULL,"UTC",NOW,0,"nonempty");
  char long_input[300]; memset(long_input,'x',sizeof long_input-1); long_input[sizeof long_input-1]=0;
  rejects(long_input,"UTC",NOW,0,"255");
  rejects("tomorrow at 9am","Not/AZone",NOW,0,"Unknown timezone");
  rejects("tomorrow at 9am","../etc/passwd",NOW,0,"Invalid timezone");
  rejects("tomorrow at 9am","EST5EDT,M3.2.0,M11.1.0",NOW,0,"Invalid IANA");
  rejects("in 2 hours","UTC",(time_t)-1,0,"Reference clock");
}

static void timezone_contract(void) {
  unsetenv("HIMALAYA_TIMEZONE"); setenv("TZ","UTC0",1);
  check(!strcmp(date_default_zone(),"UTC"),"UTC POSIX environment detection");
  setenv("HIMALAYA_TIMEZONE","Europe/London",1); check(!strcmp(date_default_zone(),"Europe/London"),"configured timezone");
  setenv("TZ","Pacific/Honolulu",1); tzset();
  resolves("2026-10-01 12:00",date_default_zone(),NOW,0,"2026-10-01T11:00:00Z");
  check(getenv("TZ") && !strcmp(getenv("TZ"),"Pacific/Honolulu"),"TZ restored after success");
  rejects("2026-03-29 01:30","Europe/London",NOW,0,"DST gap");
  check(getenv("TZ") && !strcmp(getenv("TZ"),"Pacific/Honolulu"),"TZ restored after failure");
  unsetenv("TZ"); tzset();
  resolves("2026-10-01 12:00","UTC",NOW,0,"2026-10-01T12:00:00Z");
  check(!getenv("TZ"),"unset TZ restored");
  resolves("2026-10-01 12:00","Asia/Kolkata",NOW,0,"2026-10-01T06:30:00Z");
}

static void bundled_timezone_data(void) {
  char directory[]="/tmp/himalaya zones-XXXXXX", link[128];
  check(mkdtemp(directory)!=NULL,"private timezone directory");
  snprintf(link,sizeof link,"%s/bundle",directory);
  check(!symlink(DATE_ZONEINFO_DIR,link),"bundled timezone fixture");
  setenv("HIMALAYA_ZONEINFO_DIR",directory,1);
  resolves("2026-10-01 12:00","bundle/Europe/London",NOW,0,"2026-10-01T11:00:00Z");
  rejects("2026-10-25 01:30","bundle/Europe/London",NOW,0,"DST overlap");
  setenv("HIMALAYA_ZONEINFO_DIR","relative/path",1);
  rejects("2026-10-01 12:00","Europe/London",NOW,0,"absolute path");
  char oversized[1100]; memset(oversized,'a',sizeof oversized-1);
  oversized[0]='/'; oversized[sizeof oversized-1]=0;
  setenv("HIMALAYA_ZONEINFO_DIR",oversized,1);
  rejects("2026-10-01 12:00","Europe/London",NOW,0,"too long");
  setenv("HIMALAYA_ZONEINFO_DIR","/nonexistent-himalaya-zoneinfo",1);
  rejects("2026-10-01 12:00","Europe/London",NOW,0,"Unknown timezone");
  resolves("2026-10-01 12:00Z","UTC",NOW,0,"2026-10-01T12:00:00Z");
  unsetenv("HIMALAYA_ZONEINFO_DIR");
  check(!unlink(link) && !rmdir(directory),"timezone fixture cleanup");
}

static void system_timezone_detection(void) {
  char directory[]="/tmp/himalaya-system-zone-XXXXXX", local[160], config[160], zone[129];
  check(mkdtemp(directory)!=NULL,"system timezone fixture directory");
  snprintf(local,sizeof local,"%s/localtime",directory);
  snprintf(config,sizeof config,"%s/timezone",directory);
  check(!date_detect_zone(NULL,local,config,zone,sizeof zone) && !strcmp(zone,"UTC"),"missing system configuration falls back to UTC");
  check(!date_detect_zone("Europe/London",local,config,zone,sizeof zone) && !strcmp(zone,"Europe/London"),"TZ IANA name");
  check(!date_detect_zone(":Europe/London",local,config,zone,sizeof zone) && !strcmp(zone,"Europe/London"),"TZ colon prefix");
  check(!date_detect_zone(":/usr/share/zoneinfo/Asia/Kolkata",local,config,zone,sizeof zone) && !strcmp(zone,"Asia/Kolkata"),"TZ absolute zoneinfo path");
  check(!date_detect_zone("UTC0",local,config,zone,sizeof zone) && !strcmp(zone,"UTC"),"TZ UTC0 normalisation");
  check(!date_detect_zone("",local,config,zone,sizeof zone) && !strcmp(zone,"UTC"),"empty TZ explicitly means UTC");
  FILE *file=fopen(config,"w"); check(file!=NULL,"timezone text fixture");
  fputs("  Asia/Kolkata\n",file); fclose(file);
  check(!date_detect_zone(NULL,local,config,zone,sizeof zone) && !strcmp(zone,"Asia/Kolkata"),"timezone text file");
  check(!symlink(DATE_ZONEINFO_DIR "/Europe/London",local),"localtime IANA symlink fixture");
  check(!date_detect_zone(NULL,local,config,zone,sizeof zone) && !strcmp(zone,"Europe/London"),"symlink wins over stale text file");
  check(!date_detect_zone("America/New_York",local,config,zone,sizeof zone) && !strcmp(zone,"America/New_York"),"TZ wins over system files");
  check(!date_detect_zone("invalid zone",local,config,zone,sizeof zone) && !strcmp(zone,"Europe/London"),"unrecognised TZ falls through to system configuration");
  check(!unlink(local),"remove symlink fixture");
  FILE *source=fopen(DATE_ZONEINFO_DIR "/Europe/London","rb");
  file=fopen(local,"wb"); check(source && file,"copied TZif fixture");
  int c; while ((c=fgetc(source))!=EOF) fputc(c,file);
  check(!ferror(source) && !ferror(file),"copy complete timezone data"); fclose(source); fclose(file);
  check(!date_detect_zone(NULL,local,config,zone,sizeof zone) && !strcmp(zone,"system"),"copied localtime rules win over stale text file");
  file=fopen(local,"w"); check(file!=NULL,"invalid localtime fixture"); fputs("invalid",file); fclose(file);
  check(!date_detect_zone(NULL,local,config,zone,sizeof zone) && !strcmp(zone,"Asia/Kolkata"),"invalid localtime falls through to timezone file");
  file=fopen(config,"w"); check(file!=NULL,"invalid timezone fixture"); fputs("Not/AZone\n",file); fclose(file);
  setenv("LANG","en_GB.UTF-8",1);
  check(!date_detect_zone("invalid",local,config,zone,sizeof zone) && !strcmp(zone,"UTC"),"locale does not imply a timezone");
  check(date_detect_zone(NULL,local,config,zone,3)<0,"reject small output buffer");
  setenv("TZ","UTC0",1); setenv("HIMALAYA_TIMEZONE","Asia/Kolkata",1);
  check(!strcmp(date_default_zone(),"Asia/Kolkata"),"explicit environment override wins");
  setenv("HIMALAYA_TIMEZONE","Not/AZone",1);
  check(!strcmp(date_default_zone(),"Not/AZone"),"invalid explicit override is not silently replaced");
  unsetenv("HIMALAYA_TIMEZONE"); unsetenv("TZ"); tzset();
  check(!unlink(local) && !unlink(config) && !rmdir(directory),"system timezone fixture cleanup");
}

int main(void) {
  unsetenv("HIMALAYA_ZONEINFO_DIR");
  absolute_formats(); named_months(); relative_dates(); daylight_saving(); invalid_inputs(); timezone_contract(); bundled_timezone_data(); system_timezone_detection();
  printf("date helpers: %u checks passed (formats, offsets, relative dates, DST, errors, TZ restoration)\n",checks);
  return 0;
}
