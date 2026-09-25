#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#include <cjson/cJSON.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void fail(const char *msg) {
  fprintf(stderr, "protocol test failed: %s\n", msg);
  exit(1);
}

static void write_fake(const char *path, const char *log) {
  FILE *f = fopen(path, "w");
  if (!f) fail("could not write fake himalaya");
  fprintf(f,
          "#!/bin/sh\n"
          "printf '%%s\\n' \"$*\" >> '%s'\n"
          "case \"$*\" in\n"
          "  *'envelope list'*) printf '%%s\\n' "
          "'[{\"id\":\"7\",\"flags\":[],\"subject\":\"Hello\",\"from\":{\"name\":\"A\",\"addr\":\"a@example.com\"},\"to\":{\"name\":\"\",\"addr\":\"me@example.com\"},\"date\":\"2026-09-25\",\"has_attachment\":false}]'\n"
          "    ;;\n"
          "  *'template send'*) exit 0 ;;\n"
          "  *'account list'*) printf '%%s\\n' '[{\"name\":\"gmail\",\"default\":true}]' ;;\n"
          "  *'folder list'*) printf '%%s\\n' '[{\"name\":\"INBOX\"}]' ;;\n"
          "  *) printf '%%s\\n' '{}' ;;\n"
          "esac\n",
          log);
  fclose(f);
  if (chmod(path, 0755) < 0) fail("could not chmod fake himalaya");
}

static cJSON *rpc(FILE *to, FILE *from, const char *payload) {
  if (fputs(payload, to) == EOF || fputc('\n', to) == EOF || fflush(to) != 0) fail("write rpc");
  char *line = NULL;
  size_t cap = 0;
  if (getline(&line, &cap, from) < 0) fail("read rpc");
  cJSON *msg = cJSON_Parse(line);
  free(line);
  if (!msg) fail("parse rpc");
  return msg;
}

static void expect_error(FILE *to, FILE *from, const char *payload, int code) {
  cJSON *msg = rpc(to, from, payload);
  cJSON *actual = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(msg,"error"),"code");
  if (!cJSON_IsNumber(actual) || actual->valueint != code) fail("RPC error code");
  cJSON_Delete(msg);
}

static int contains(const char *path, const char *needle) {
  FILE *f = fopen(path, "r");
  if (!f) return 0;
  char buf[4096];
  size_t n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[n] = 0;
  return strstr(buf, needle) != NULL;
}

int main(int argc, char **argv) {
  if (argc != 2) fail("usage: protocol_test <himalaya-mcp>");
  char dir[] = "/tmp/himalaya-mcp-test-XXXXXX";
  if (!mkdtemp(dir)) fail("mkdtemp");
  char fake[512], log[512], state[512];
  snprintf(fake, sizeof fake, "%s/himalaya", dir);
  snprintf(log, sizeof log, "%s/argv.log", dir);
  snprintf(state, sizeof state, "%s/state", dir);
  write_fake(fake, log);
  setenv("HIMALAYA_BINARY", fake, 1);
  setenv("XDG_STATE_HOME", state, 1);
  setenv("HIMALAYA_TIMEZONE", "Europe/London", 1);

  int in[2], out[2];
  if (pipe(in) < 0 || pipe(out) < 0) fail("pipe");
  pid_t pid = fork();
  if (pid < 0) fail("fork");
  if (pid == 0) {
    dup2(in[0], STDIN_FILENO);
    dup2(out[1], STDOUT_FILENO);
    close(in[1]);
    close(out[0]);
    execl(argv[1], argv[1], (char *)NULL);
    _exit(127);
  }
  close(in[0]);
  close(out[1]);
  FILE *to = fdopen(in[1], "w");
  FILE *from = fdopen(out[0], "r");
  if (!to || !from) fail("fdopen");

  cJSON *init = rpc(to, from, "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{}}");
  cJSON *name = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(init, "result"), "serverInfo"), "name");
  if (!cJSON_IsString(name) || strcmp(name->valuestring, "himalaya-mcp") != 0) fail("server name");
  cJSON_Delete(init);

  cJSON *tools = rpc(to, from, "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}");
  cJSON *tool_arr = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(tools, "result"), "tools");
  if (!cJSON_IsArray(tool_arr) || cJSON_GetArraySize(tool_arr) != 30) fail("tool count");
  int date_fields = 0;
  cJSON *tool;
  cJSON_ArrayForEach(tool, tool_arr) {
    cJSON *schema = cJSON_GetObjectItemCaseSensitive(tool,"inputSchema");
    cJSON *properties = cJSON_GetObjectItemCaseSensitive(schema,"properties");
    const char *keys[] = {"dtstart","dtend","snoozeUntil","dueDate"};
    for (int i=0;i<4;i++) {
      cJSON *property = cJSON_GetObjectItemCaseSensitive(properties,keys[i]);
      if (!property) continue;
      cJSON *tool_help = cJSON_GetObjectItemCaseSensitive(tool,"description");
      if (!cJSON_IsString(tool_help) || !strstr(tool_help->valuestring,"Europe/London") || !strstr(tool_help->valuestring,"tomorrow at 9am")) fail("date instructions for clients omitting property descriptions");
      cJSON *help = cJSON_GetObjectItemCaseSensitive(property,"description");
      if (!cJSON_IsString(help) || !strstr(help->valuestring,"Europe/London") || !strstr(help->valuestring,"tomorrow at 9am")) fail("date discovery instructions");
      date_fields++;
    }
  }
  if (date_fields!=4) fail("date field coverage");
  cJSON_Delete(tools);
  cJSON *calendar = rpc(to,from,"{\"jsonrpc\":\"2.0\",\"id\":40,\"method\":\"tools/call\",\"params\":{\"name\":\"create_calendar_event\",\"arguments\":{\"summary\":\"Synthetic\",\"dtstart\":\"1 October 2026 at noon\",\"dtend\":\"2026-10-01 12:30\"}}}");
  char *calendar_text = cJSON_PrintUnformatted(calendar);
  if (!strstr(calendar_text,"Start: 2026-10-01T11:00:00Z") || !strstr(calendar_text,"End: 2026-10-01T11:30:00Z") || !strstr(calendar_text,"Europe/London")) fail("wire calendar date normalisation");
  free(calendar_text); cJSON_Delete(calendar);

  cJSON *prompts = rpc(to, from, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"prompts/list\"}");
  cJSON *prompt_arr = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(prompts, "result"), "prompts");
  if (!cJSON_IsArray(prompt_arr) || cJSON_GetArraySize(prompt_arr) != 7) fail("prompt count");
  for (int i = 0; i < 7; i++) {
    const char *prompt_name = cJSON_GetObjectItemCaseSensitive(cJSON_GetArrayItem(prompt_arr,i),"name")->valuestring;
    char payload[1024];
    snprintf(payload,sizeof payload,"{\"jsonrpc\":\"2.0\",\"id\":20,\"method\":\"prompts/get\",\"params\":{\"name\":\"%s\",\"arguments\":{\"id\":\"123\",\"instructions\":\"Synthetic context\"}}}",prompt_name);
    cJSON *prompt=rpc(to,from,payload);
    char *printed=cJSON_PrintUnformatted(prompt);
    if (!strstr(printed,"Synthetic context") || !strstr(printed,"123")) fail("prompt context");
    free(printed); cJSON_Delete(prompt);
  }
  cJSON_Delete(prompts);

  cJSON *resources = rpc(to, from, "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"resources/list\"}");
  cJSON *resource_arr = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(resources, "result"), "resources");
  if (!cJSON_IsArray(resource_arr) || cJSON_GetArraySize(resource_arr) != 2) fail("resource count");
  cJSON_Delete(resources);
  cJSON *templates=rpc(to,from,"{\"jsonrpc\":\"2.0\",\"id\":21,\"method\":\"resources/templates/list\"}");
  cJSON *template_arr=cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(templates,"result"),"resourceTemplates");
  if (cJSON_GetArraySize(template_arr)!=1) fail("resource templates");
  cJSON_Delete(templates);
  const char *uris[]={"email://inbox","email://folders","email://message/7"};
  for (int i=0;i<3;i++) {
    char payload[512]; snprintf(payload,sizeof payload,"{\"jsonrpc\":\"2.0\",\"id\":22,\"method\":\"resources/read\",\"params\":{\"uri\":\"%s\"}}",uris[i]);
    cJSON *resource=rpc(to,from,payload);
    if (!cJSON_GetObjectItemCaseSensitive(resource,"result")) fail("resource read");
    cJSON_Delete(resource);
  }
  expect_error(to,from,"{bad json",-32700);
  expect_error(to,from,"{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"ping\"}junk",-32700);
  expect_error(to,from,"[]",-32600);
  expect_error(to,from,"{\"id\":30,\"method\":\"ping\"}",-32600);
  expect_error(to,from,"{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"missing\"}",-32601);
  expect_error(to,from,"{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"prompts/get\",\"params\":{\"name\":\"missing\"}}",-32602);
  expect_error(to,from,"{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"resources/read\",\"params\":{\"uri\":\"email://missing\"}}",-32002);
  expect_error(to,from,"{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"tools/call\",\"params\":{\"name\":\"read_email\",\"arguments\":{}}}",-32602);
  expect_error(to,from,"{\"jsonrpc\":\"2.0\",\"id\":30,\"method\":\"tools/call\",\"params\":{\"name\":\"send_email\",\"arguments\":{\"template\":\"x\",\"confirm\":\"true\"}}}",-32602);
  /* A notification must neither execute the mutation nor emit a reply. */
  fputs("{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"send_email\",\"arguments\":{\"template\":\"x\",\"confirm\":true}}}\n",to);
  cJSON *pong=rpc(to,from,"{\"jsonrpc\":\"2.0\",\"id\":99,\"method\":\"ping\"}");
  if (cJSON_GetObjectItemCaseSensitive(pong,"id")->valueint!=99 || contains(log,"template send")) fail("notification handling");
  cJSON_Delete(pong);

  cJSON *emails = rpc(to, from,
                      "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"tools/call\",\"params\":{\"name\":\"list_emails\",\"arguments\":{\"account\":\"gmail\"}}}");
  cJSON *email_text = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(emails, "result"), "content"), 0), "text");
  if (!cJSON_IsString(email_text) || !strstr(email_text->valuestring, "Hello")) fail("list_emails");
  cJSON_Delete(emails);

  cJSON *preview = rpc(to, from,
                       "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\",\"params\":{\"name\":\"send_email\",\"arguments\":{\"template\":\"To: a@example.com\\n\\nHi\\n\"}}}");
  cJSON *preview_text = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(preview, "result"), "content"), 0), "text");
  if (!cJSON_IsString(preview_text) || !strstr(preview_text->valuestring, "PREVIEW")) fail("send preview");
  cJSON_Delete(preview);
  if (contains(log, "template send")) fail("preview sent mail");

  cJSON *sent = rpc(to, from,
                    "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\",\"params\":{\"name\":\"send_email\",\"arguments\":{\"template\":\"To: a@example.com\\n\\nHi\\n\",\"confirm\":true}}}");
  cJSON *sent_text = cJSON_GetObjectItemCaseSensitive(
      cJSON_GetArrayItem(cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(sent, "result"), "content"), 0), "text");
  if (!cJSON_IsString(sent_text) || !strstr(sent_text->valuestring, "sent")) fail("send confirm");
  cJSON_Delete(sent);
  if (!contains(log, "template send")) fail("confirm did not send");

  fclose(to);
  int status = 0;
  if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) fail("server exit");
  fclose(from);
  puts("ok 30 tools");
  return 0;
}
