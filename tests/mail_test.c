#define _POSIX_C_SOURCE 200809L
#include "tools.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const unsigned char bytes[] = {'A', 0, 'B', 255, 1};
static void check(int ok, const char *message) {
  if (!ok) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
static const char *option(int argc, char **argv, const char *key) {
  for (int i = 0; i + 1 < argc; i++) if (!strcmp(argv[i], key)) return argv[i + 1];
  return NULL;
}
static int fake(int argc, char **argv) {
  if (argc < 4) return 2;
  if (!strcmp(argv[2], "template") &&
      (!strcmp(argv[3], "write") || !strcmp(argv[3], "reply"))) {
    puts("{\"content\":\"From: Test <self@example.invalid>\\nTo: self@example.invalid\\nSubject: synthetic\\n\\nHello\\n\",\"cursor\":{\"row\":6,\"col\":1}}");
    return 0;
  }
  if ((!strcmp(argv[2], "template") && !strcmp(argv[3], "send")) ||
      (!strcmp(argv[2], "message") && !strcmp(argv[3], "save"))) {
    if (!strcmp(argv[3], "save") && strcmp(option(argc, argv, "-o"), "plain")) return 8;
    char template[8192];
    size_t length = fread(template, 1, sizeof template - 1, stdin);
    template[length] = 0;
    if (strncmp(template, "From:", 5)) return 3;
    FILE *log = fopen(getenv("TEST_LOG"), "a");
    if (!log) return 4;
    fprintf(log, "%s\n", argv[3]);
    fclose(log);
    puts("\"ok\"");
    return 0;
  }
  if (!strcmp(argv[2], "attachment") && !strcmp(argv[3], "download")) {
    const char *dir = option(argc, argv, "-d");
    if (!dir) return 5;
    char path[1024];
    snprintf(path, sizeof path, "%s/test.bin", dir);
    FILE *f = fopen(path, "wb");
    if (!f) return 6;
    fwrite(bytes, 1, sizeof bytes, f);
    fclose(f);
    puts("\"exported\"");
    return 0;
  }
  return 7;
}
int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "--quiet")) return fake(argc, argv);
  char cwd[4096], executable[8192];
  check(getcwd(cwd, sizeof cwd) != NULL, "cwd");
  snprintf(executable, sizeof executable, "%s/%s", cwd, argv[0]);
  setenv("HIMALAYA_BINARY", executable, 1);
  char dir[] = "/tmp/himalaya-mail-test-XXXXXX";
  check(mkdtemp(dir) != NULL, "temporary directory");
  char file[512], log[512];
  snprintf(file, sizeof file, "%s/space name.bin", dir);
  snprintf(log, sizeof log, "%s/actions", dir);
  setenv("TEST_LOG", log, 1);
  FILE *f = fopen(file, "wb");
  check(f != NULL, "fixture file");
  check(fwrite(bytes, 1, sizeof bytes, f) == sizeof bytes, "fixture write");
  fclose(f);

  cJSON *args = cJSON_Parse("{\"to\":\"self@example.invalid\",\"subject\":\"synthetic\",\"body\":\"Hello\"}");
  cJSON *files = cJSON_AddArrayToObject(args, "attachments");
  cJSON_AddItemToArray(files, cJSON_CreateString(file));
  Result r = tool_compose_email(args);
  check(!r.is_error && strstr(r.text, "From: Test") && strstr(r.text, "filename=\"") &&
        strstr(r.text, "space name.bin\"") && strstr(r.text, "disposition=attachment"), "compose preview includes sender and quoted attachment");
  check(access(log, F_OK) != 0, "preview must not send");
  result_free(r);
  cJSON_AddBoolToObject(args, "confirm", 1);
  r = tool_compose_email(args);
  check(!r.is_error, "confirmed compose");
  result_free(r);
  cJSON_AddStringToObject(args, "cc", "bad\rBcc: other@example.invalid");
  r = tool_compose_email(args);
  check(r.is_error, "reject header injection");
  result_free(r);
  cJSON_DeleteItemFromObject(args, "cc");
  cJSON_ReplaceItemInArray(files, 0, cJSON_CreateString("/no/such/attachment"));
  r = tool_compose_email(args);
  check(r.is_error, "reject missing attachment instead of silently sending without it");
  result_free(r);
  cJSON_Delete(args);

  args = cJSON_Parse("{\"id\":\"7\"}");
  r = tool_draft_reply(args);
  check(!r.is_error && !strncmp(r.text, "From:", 5) && strchr(r.text, '\n'), "draft is decoded reusable MML");
  cJSON *draft = cJSON_CreateObject();
  cJSON_AddStringToObject(draft, "template", r.text);
  result_free(r);
  r = tool_save_draft(draft);
  check(!r.is_error, "save draft");
  result_free(r);
  char *actions = read_file(log, 4096);
  check(actions && !strcmp(actions, "send\nsave\n"), "draft saves without sending");
  free(actions);
  cJSON_Delete(draft);

  cJSON_AddStringToObject(args, "filename", "test.bin");
  r = tool_download_attachment(args);
  check(!r.is_error, "download attachment");
  unsigned char actual[sizeof bytes + 1];
  f = fopen(r.text, "rb");
  check(f != NULL, "open downloaded attachment");
  size_t count = fread(actual, 1, sizeof actual, f);
  fclose(f);
  check(count == sizeof bytes && !memcmp(bytes, actual, sizeof bytes), "binary bytes including NUL preserved");
  unlink(r.text);
  char *slash = strrchr(r.text, '/');
  if (slash) { *slash = 0; rmdir(r.text); }
  result_free(r);
  cJSON_Delete(args);
  unlink(file); unlink(log); rmdir(dir);
  puts("mail regressions passed: composition, validation, draft save, binary attachment");
  return 0;
}
