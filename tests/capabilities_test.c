#include "tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static unsigned checks;
static void check(int ok, const char *what) {
  checks++;
  if (!ok) { fprintf(stderr, "FAIL: %s\n", what); exit(1); }
}
static const char *option(int argc, char **argv, const char *name) {
  for (int i=1; i+1<argc; i++) if (!strcmp(argv[i],name)) return argv[i+1];
  return "";
}
static int fake(int argc, char **argv) {
  const char *query = getenv("EXPECT_QUERY");
  if (!strcmp(argv[2], "envelope")) {
    if (query && strcmp(query,argv[argc-1])) return 9;
    int count = getenv("MANY_ROWS") ? 100 : 1;
    putchar('[');
    for (int i=0;i<count;i++) printf("%s{\"id\":\"1\",\"subject\":\"Hello\\nworld\",\"flags\":[],\"from\":{}}",i?",":"");
    puts("]"); return 0;
  }
  if (!strcmp(argv[2],"message") && !strcmp(argv[3],"read")) {
    puts("\"Hi\""); return 0;
  }
  if (!strcmp(argv[2],"message") && !strcmp(argv[3],"export")) {
    char file[1024];
    int full = 0;
    for (int i=1;i<argc;i++) if (!strcmp(argv[i],"-F")) full=1;
    snprintf(file,sizeof file,"%s/%s",option(argc,argv,"-d"),full?"message.eml":"index.html");
    return write_file(file,full?"From: test@example.invalid\r\nSubject: Test\r\n\r\nHi":"<p>Test HTML</p>");
  }
  if (!strcmp(argv[2],"attachment")) {
    char file[1024];
    snprintf(file,sizeof file,"%s/event.ics",option(argc,argv,"-d"));
    return write_file(file,"BEGIN:VCALENDAR\r\nVERSION:2.0\r\nEND:VCALENDAR\r\n");
  }
  if (!strcmp(argv[2],"folder") && !strcmp(option(argc,argv,"-a"),"bad")) return 1;
  puts("[]"); return 0;
}
static Result call(Result (*fn)(const cJSON *), const char *json) {
  cJSON *args=cJSON_Parse(json);
  check(args != NULL,"test JSON");
  Result r=fn(args); cJSON_Delete(args); return r;
}
static void expect(Result (*fn)(const cJSON *), const char *json, int error, const char *needle) {
  Result r=call(fn,json);
  if (r.is_error != error || (needle && !strstr(r.text,needle))) fprintf(stderr,"result: %s\n",r.text);
  check(r.is_error == error,"result error status");
  check(!needle || strstr(r.text,needle),"result content");
  result_free(r);
}
int main(int argc,char **argv) {
  if (argc == 2 && !strcmp(argv[1], "--large-output")) {
    char block[4096]; memset(block,'x',sizeof block);
    for (int i=0;i<2304;i++) if (fwrite(block,1,sizeof block,stdout)!=sizeof block) return 1;
    return 0;
  }
  if (argc == 2 && !strcmp(argv[1], "--close-and-sleep")) { close(1); close(2); sleep(3); return 0; }
  if (!strcmp(argv[0], "wl-copy")) {
    char buffer[4096]; size_t count;
    FILE *dest=fopen(getenv("CLIPBOARD_TEST_FILE"),"wb");
    if (!dest) return 3;
    while ((count=fread(buffer,1,sizeof buffer,stdin))) if (fwrite(buffer,1,count,dest)!=count) return 4;
    return fclose(dest) ? 5 : 0;
  }
  if (argc>2) return fake(argc,argv);
  char self[4096];
#ifdef __APPLE__
  char executable[4096]; uint32_t self_size=sizeof executable;
  check(!_NSGetExecutablePath(executable,&self_size) && realpath(executable,self),"executable path");
#else
  ssize_t length=readlink("/proc/self/exe",self,sizeof self-1);
  check(length>0 && (size_t)length<sizeof self-1,"executable path"); self[length]=0;
#endif
  setenv("HIMALAYA_BINARY",self,1);
  setenv("HIMALAYA_TIMEZONE","UTC",1);
  setenv("HIMALAYA_TIMEOUT","1",1);
  char *sleep_args[] = {self,"--close-and-sleep",NULL};
  Capture captured={0};
  check(run_cmd(sleep_args,&captured)<0,"timeout remains enforced after child closes output");
  capture_free(&captured); unsetenv("HIMALAYA_TIMEOUT");
  sleep_args[1]="--large-output";
  check(run_cmd(sleep_args,&captured)<0,"excess CLI output fails instead of truncating silently");
  capture_free(&captured);
  char dir[]="/tmp/himalaya-capabilities-test-XXXXXX";
  check(mkdtemp(dir)!=NULL,"state directory"); setenv("XDG_STATE_HOME",dir,1);
  expect(tool_list_emails,"{}",0,"Hello");
  setenv("EXPECT_QUERY","subject \"two  spaces\"",1);
  expect(tool_search_emails,"{\"query\":\"subject \\\"two  spaces\\\"\"}",0,"Hello");
  unsetenv("EXPECT_QUERY");
  expect(tool_get_unread_count,"{}",0,"1\n");
  setenv("MANY_ROWS","1",1);
  expect(tool_get_unread_count,"{}",1,"at least 2000");
  unsetenv("MANY_ROWS");
  expect(tool_list_starred,"{}",0,"Hello");
  expect(tool_read_email,"{\"id\":\"1\"}",0,"Hi");
  expect(tool_render_email,"{\"id\":\"1\"}",0,"Hi");
  expect(tool_read_email_raw,"{\"id\":\"1\"}",0,"Subject: Test");
  expect(tool_read_email_html,"{\"id\":\"1\"}",0,"<p>Test HTML</p>");
  expect(tool_export_to_markdown,"{\"id\":\"1\"}",0,"id: \"1\"");
  expect(tool_create_action_item,"{\"id\":\"1\",\"destination\":\"/dev/null/no-file\"}",1,"could not write");
  expect(tool_create_action_item,"{\"id\":\"1\"}",0,"Hi");
  expect(tool_flag_email,"{\"id\":\"1\",\"action\":\"add\",\"flags\":[\"Seen\"]}",0,"updated");
  expect(tool_flag_email,"{\"id\":\"1\",\"action\":\"add\",\"flags\":[\"invalid flag\"]}",1,"invalid");
  expect(tool_move_email,"{\"id\":\"1\",\"target_folder\":\"Test\"}",0,"moved");
  expect(tool_list_folders,"{}",0,"[]");
  expect(tool_create_folder,"{\"name\":\"Test\"}",0,"created");
  expect(tool_delete_folder,"{\"name\":\"Test\"}",0,"PREVIEW");
  expect(tool_delete_folder,"{\"name\":\"Test\",\"confirm\":true}",0,"deleted");
  expect(tool_extract_calendar_event,"{\"id\":\"1\"}",0,"VCALENDAR");
  expect(tool_list_threads,"{}",0,"Hello");
  cJSON *args=cJSON_CreateObject(); char subject[6001]; memset(subject,'x',6000); subject[6000]=0;
  cJSON_AddStringToObject(args,"thread_id",subject);
  Result thread=tool_read_thread(args); check(!thread.is_error && strstr(thread.text,subject),"long thread subject not truncated or overflowing");
  result_free(thread); cJSON_Delete(args);
  expect(tool_read_thread,"{\"thread_id\":\"subject\\\" or all\"}",1,"cannot contain");
  expect(tool_health_check,"{}",0,"accounts:");
  expect(tool_health_check,"{\"account\":\"bad\"}",1,NULL);
  expect(tool_list_snoozed_emails,"{}",0,"[]");
  expect(tool_snooze_email,"{\"id\":\"1\",\"snoozeUntil\":\"nonsense\"}",1,"parse");
  expect(tool_snooze_email,"{\"id\":\"1\",\"snoozeUntil\":\"2027-02-30T12:00:00Z\"}",1,"parse");
  expect(tool_snooze_email,"{\"id\":\"1\",\"snoozeUntil\":\"1junkh\"}",1,"parse");
  expect(tool_snooze_email,"{\"id\":\"1\",\"snoozeUntil\":\"99999999999999d\"}",1,"parse");
  expect(tool_snooze_email,"{\"id\":\"1\",\"snoozeUntil\":\"2h\"}",0,"snoozed");
  expect(tool_snooze_email,"{\"id\":\"2\",\"snoozeUntil\":\"2028-02-29T12:00:00Z\"}",0,"2028-02-29");
  expect(tool_list_snoozed_emails,"{}",0,"snoozeUntil");
  expect(tool_snooze_email,"{\"id\":\"3\",\"snoozeUntil\":\"1 October 2026 at noon +01:00\"}",0,"2026-10-01T11:00:00Z");
  expect(tool_list_snoozed_emails,"{}",0,"2026-10-01T11:00:00Z");
  expect(tool_create_reminder,"{\"title\":\"Named date\",\"dueDate\":\"1 October 2026 at noon +01:00\"}",0,"2026-10-01T11:00:00Z");
  expect(tool_create_reminder,"{\"title\":\"Ambiguous date\",\"dueDate\":\"01/02/2026 12:00\"}",1,"ambiguous");
  expect(tool_create_reminder,"{\"title\":\"Test\",\"notes\":\"Notes\",\"priority\":3,\"dueDate\":\"2027-01-01T12:00:00Z\"}",0,"stored");
  char *path=state_file("reminders.json"), *text=read_file(path,4096);
  check(text && strstr(text,"\"priority\":\t3"),"reminder priority preserved");
  check(text && strstr(text,"2026-10-01T11:00:00Z"),"normalised reminder persisted"); free(text);
  check(!write_file(path,"broken JSON"),"corrupt fixture");
  expect(tool_create_reminder,"{\"title\":\"Test\"}",1,"unchanged");
  text=read_file(path,4096); check(text && !strcmp(text,"broken JSON"),"corrupt state not erased"); free(text);
  check(read_file(path,2)==NULL,"oversized text file rejected");
  text=read_file(path,11); check(text && !strcmp(text,"broken JSON"),"exact size text file accepted"); free(text);
  struct stat mode; check(!stat(path,&mode) && (mode.st_mode & 0777)==0600,"private state file permissions");
  unlink(path); free(path);
  const char *event="{\"summary\":\"Test, semi; slash\\\\ and\\nnewline\",\"dtstart\":\"20261001T120000Z\",\"dtend\":\"20261001T123000Z\",\"location\":\"Here\",\"description\":\"Notes\"}";
  expect(tool_create_calendar_event,event,0,"PREVIEW");
  expect(tool_create_calendar_event,"{\"summary\":\"Named date\",\"dtstart\":\"1 October 2026 at noon +01:00\",\"dtend\":\"2026-10-01T12:30:00+01:00\"}",0,"Start: 2026-10-01T11:00:00Z");
  expect(tool_create_calendar_event,"{\"summary\":\"Offset ordering\",\"dtstart\":\"2026-10-01T12:00:00Z\",\"dtend\":\"2026-10-01T12:30:00+01:00\"}",1,"after dtstart");
  expect(tool_create_calendar_event,"{\"summary\":\"Relative\",\"dtstart\":\"in 2 hours\",\"dtend\":\"in 3 hours\"}",0,"Zone-less input timezone: UTC");
  args=cJSON_Parse(event); cJSON_AddBoolToObject(args,"confirm",1);
  Result first=tool_create_calendar_event(args), second=tool_create_calendar_event(args);
  check(!first.is_error && !second.is_error && strcmp(first.text,second.text),"calendar does not overwrite preceding event");
  first.text[strcspn(first.text,"\n")]=0;
  text=read_file(first.text+6,8192);
  check(text && strstr(text,"VERSION:2.0\r\n") && strstr(text,"UID:") && strstr(text,"DTSTAMP:") && strstr(text,"LOCATION:Here") && strstr(text,"DESCRIPTION:Notes") && strstr(text,"Test\\, semi\\;"),"calendar RFC fields and escaping");
  free(text); result_free(first); result_free(second); cJSON_Delete(args);
  expect(tool_create_calendar_event,"{\"summary\":\"x\",\"dtstart\":\"bad\",\"dtend\":\"bad\"}",1,"dtstart");
  setenv("XDG_STATE_HOME","/dev/null",1);
  expect(tool_create_reminder,"{\"title\":\"x\"}",1,NULL);
  expect(tool_snooze_email,"{\"id\":\"1\",\"snoozeUntil\":\"2h\"}",1,NULL);
  args=cJSON_Parse(event); cJSON_AddBoolToObject(args,"confirm",1); first=tool_create_calendar_event(args); check(first.is_error,"calendar write failure"); result_free(first); cJSON_Delete(args);
  char *oldpath=strdup(getenv("PATH")); setenv("PATH","/no-such-directory",1);
  args=cJSON_CreateObject(); char *large=malloc(1024*1024+1); memset(large,'x',1024*1024); large[1024*1024]=0;
  cJSON_AddStringToObject(args,"text",large); free(large);
  first=tool_copy_to_clipboard(args); check(first.is_error,"clipboard absence survives without SIGPIPE"); result_free(first); cJSON_Delete(args);
  char clipboard[4096], copied[4096];
  snprintf(clipboard,sizeof clipboard,"%s/wl-copy",dir);
  snprintf(copied,sizeof copied,"%s/copied.txt",dir);
  check(!symlink(self,clipboard),"isolated clipboard helper");
  setenv("CLIPBOARD_TEST_FILE",copied,1); setenv("PATH",dir,1);
  expect(tool_copy_to_clipboard,"{\"text\":\"Synthetic clipboard\\ncontents\"}",0,"copied");
  text=read_file(copied,4096); check(text && !strcmp(text,"Synthetic clipboard\ncontents"),"clipboard bytes"); free(text);
  unlink(clipboard); unlink(copied);
  setenv("PATH",oldpath,1); free(oldpath);
  printf("capability regressions: %u checks passed\n",checks);
  return 0;
}
