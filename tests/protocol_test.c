#define _POSIX_C_SOURCE 200809L

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
  cJSON_Delete(tools);

  cJSON *prompts = rpc(to, from, "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"prompts/list\"}");
  cJSON *prompt_arr = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(prompts, "result"), "prompts");
  if (!cJSON_IsArray(prompt_arr) || cJSON_GetArraySize(prompt_arr) != 7) fail("prompt count");
  cJSON_Delete(prompts);

  cJSON *resources = rpc(to, from, "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"resources/list\"}");
  cJSON *resource_arr = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItemCaseSensitive(resources, "result"), "resources");
  if (!cJSON_IsArray(resource_arr) || cJSON_GetArraySize(resource_arr) != 3) fail("resource count");
  cJSON_Delete(resources);

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
