#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

Result result_ok(char *text) {
  Result r = {text ? text : strdup(""), 0};
  return r;
}

Result result_err(const char *msg) {
  Result r = {strdup(msg ? msg : "error"), 1};
  return r;
}

Result result_errf(const char *fmt, ...) {
  char buf[1024];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  return result_err(buf);
}

void result_free(Result r) { free(r.text); }

void argv_init(Argv *a) {
  a->v = NULL;
  a->n = 0;
  a->cap = 0;
}

void argv_add(Argv *a, const char *s) {
  if (a->n + 1 >= a->cap) {
    a->cap = a->cap ? a->cap * 2 : 8;
    a->v = realloc(a->v, a->cap * sizeof *a->v);
  }
  a->v[a->n++] = s ? strdup(s) : NULL;
}

void argv_free(Argv *a) {
  for (size_t i = 0; i < a->n; i++) free(a->v[i]);
  free(a->v);
  a->v = NULL;
  a->n = a->cap = 0;
}

void capture_free(Capture *c) {
  free(c->out);
  free(c->err);
  c->out = c->err = NULL;
}

static int timeout_sec(void) {
  const char *t = getenv("HIMALAYA_TIMEOUT");
  int n = t && *t ? atoi(t) : 60;
  return n > 0 ? n : 60;
}

static int append_fd(char **buf, size_t *len, size_t *cap, int fd, size_t max) {
  char tmp[4096];
  ssize_t n = read(fd, tmp, sizeof tmp);
  if (n < 0) return errno == EAGAIN || errno == EWOULDBLOCK ? 0 : -1;
  if (n == 0) return 1;
  if (*len + (size_t)n > max) return -2;
  if (*len + (size_t)n + 1 > *cap) {
    *cap = (*cap ? *cap : 4096);
    while (*len + (size_t)n + 1 > *cap) *cap *= 2;
    *buf = realloc(*buf, *cap);
  }
  memcpy(*buf + *len, tmp, (size_t)n);
  *len += (size_t)n;
  (*buf)[*len] = 0;
  return 0;
}

int run_cmd(char *const argv[], Capture *cap) {
  return run_cmd_input(argv, cap, NULL);
}

int run_cmd_input(char *const argv[], Capture *cap, const char *input) {
  /* Anonymous file avoids pipe deadlocks for large templates and argv exposure. */
  FILE *in = tmpfile();
  if (!in) return -1;
  if (input && fwrite(input, 1, strlen(input), in) != strlen(input)) {
    fclose(in);
    return -1;
  }
  if (fflush(in) || fseek(in, 0, SEEK_SET)) { fclose(in); return -1; }
  int outp[2], errp[2];
  if (pipe(outp) < 0) { fclose(in); return -1; }
  if (pipe(errp) < 0) { close(outp[0]); close(outp[1]); fclose(in); return -1; }
  pid_t pid = fork();
  if (pid < 0) {
    close(outp[0]); close(outp[1]); close(errp[0]); close(errp[1]); fclose(in);
    return -1;
  }
  if (pid == 0) {
    setpgid(0, 0);
    dup2(outp[1], STDOUT_FILENO);
    dup2(errp[1], STDERR_FILENO);
    if (dup2(fileno(in), STDIN_FILENO) < 0) _exit(126);
    fclose(in);
    close(outp[0]);
    close(outp[1]);
    close(errp[0]);
    close(errp[1]);
    execvp(argv[0], argv);
    dprintf(STDERR_FILENO, "exec %s: %s\n", argv[0], strerror(errno));
    _exit(127);
  }
  fclose(in);
  setpgid(pid, pid);
  close(outp[1]);
  close(errp[1]);
  fcntl(outp[0], F_SETFL, O_NONBLOCK);
  fcntl(errp[0], F_SETFL, O_NONBLOCK);
  cap->out = calloc(1, 1);
  cap->err = calloc(1, 1);
  size_t ol = 0, oc = 1, el = 0, ec = 1;
  int out_open = 1, err_open = 1, child_done = 0, st = 0, read_failed = 0;
  struct timespec start;
  clock_gettime(CLOCK_MONOTONIC, &start);
  const int limit = timeout_sec();
  while (out_open || err_open || !child_done) {
    if (!child_done) {
      pid_t waited = waitpid(pid, &st, WNOHANG);
      if (waited == pid) child_done = 1;
      else if (waited < 0 && errno != EINTR) { close(outp[0]); close(errp[0]); return -1; }
    }
    if (child_done && !out_open && !err_open) break;
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int elapsed = (int)(now.tv_sec - start.tv_sec);
    if (elapsed >= limit) {
      kill(-pid, SIGKILL);
      if (!child_done) waitpid(pid, &cap->status, 0);
      cap->status = -1;
      close(outp[0]);
      close(errp[0]);
      return -1;
    }
    struct pollfd fds[2] = {
        {out_open ? outp[0] : -1, POLLIN, 0},
        {err_open ? errp[0] : -1, POLLIN, 0},
    };
    poll(fds, 2, 200);
    if (out_open) {
      int rc = append_fd(&cap->out, &ol, &oc, outp[0], 8 * 1024 * 1024);
      if (rc == 1) out_open = 0;
      if (rc < 0) {
        read_failed = 1;
        kill(-pid, SIGKILL);
        break;
      }
    }
    if (err_open) {
      int rc = append_fd(&cap->err, &el, &ec, errp[0], 1024 * 1024);
      if (rc == 1) err_open = 0;
      if (rc < 0) {
        read_failed = 1;
        kill(-pid, SIGKILL);
        break;
      }
    }
  }
  close(outp[0]);
  close(errp[0]);
  if (!child_done && waitpid(pid, &st, 0) < 0) return -1;
  cap->status = WIFEXITED(st) ? WEXITSTATUS(st) : 1;
  return read_failed ? -1 : 0;
}

const char *arg_str(const cJSON *args, const char *key) {
  if (!args) return NULL;
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(args, key);
  return cJSON_IsString(item) ? item->valuestring : NULL;
}

int arg_int(const cJSON *args, const char *key, int fallback) {
  if (!args) return fallback;
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(args, key);
  return cJSON_IsNumber(item) ? item->valueint : fallback;
}

int arg_bool(const cJSON *args, const char *key) {
  if (!args) return 0;
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(args, key);
  return cJSON_IsTrue(item);
}

const cJSON *arg_array(const cJSON *args, const char *key) {
  if (!args) return NULL;
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(args, key);
  return cJSON_IsArray(item) ? item : NULL;
}

static int no_controls(const char *s) {
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p < 32 || *p == 127) return 0;
  }
  return 1;
}

int valid_id(const char *s) {
  if (!s || !*s || strlen(s) > 64) return 0;
  for (const char *p = s; *p; p++) {
    if (!isalnum((unsigned char)*p)) return 0;
  }
  return 1;
}

int valid_folder(const char *s) {
  if (!s || !*s || *s == '-' || strlen(s) > 240) return 0;
  return no_controls(s) && !strchr(s, '\n');
}

int valid_account(const char *s) {
  if (!s || !*s) return 1;
  if (*s == '-' || strlen(s) > 64) return 0;
  for (const char *p = s; *p; p++) {
    if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-' && *p != '.') return 0;
  }
  return 1;
}

int valid_flag(const char *s) {
  if (!s || !*s || strlen(s) > 32) return 0;
  for (const char *p = s; *p; p++) {
    if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') return 0;
  }
  return 1;
}

static const char *him_bin(void) {
  const char *b = getenv("HIMALAYA_BINARY");
  return b && *b ? b : "himalaya";
}

void him_start(Argv *a) {
  argv_init(a);
  argv_add(a, him_bin());
  argv_add(a, "--quiet");
}

void him_opts(Argv *a, const char *account) {
  argv_add(a, "-o");
  argv_add(a, "json");
  if (account && *account) {
    argv_add(a, "-a");
    argv_add(a, account);
  }
}

Result him_run(Argv *a) {
  return him_run_input(a, NULL);
}

Result him_run_input(Argv *a, const char *input) {
  argv_add(a, NULL);
  Capture cap = {0};
  int rc = run_cmd_input(a->v, &cap, input);
  argv_free(a);
  if (rc < 0) {
    capture_free(&cap);
    return result_err("himalaya timed out or failed to start");
  }
  if (cap.status != 0) {
    const char *msg = cap.err && *cap.err ? cap.err : cap.out;
    Result r = result_err(msg && *msg ? msg : "himalaya failed");
    capture_free(&cap);
    return r;
  }
  char *out = cap.out ? cap.out : strdup("");
  cap.out = NULL;
  capture_free(&cap);
  /* Templates use {content, cursor}; other CLI text may be a JSON string. */
  cJSON *parsed = cJSON_Parse(out);
  const cJSON *text = cJSON_IsObject(parsed)
      ? cJSON_GetObjectItemCaseSensitive(parsed, "content") : parsed;
  if (cJSON_IsString(text)) {
    char *decoded = strdup(text->valuestring);
    free(out);
    out = decoded;
  }
  cJSON_Delete(parsed);
  return result_ok(out);
}

static const char *json_start(const char *s) {
  const char *arr = strchr(s, '[');
  const char *obj = strchr(s, '{');
  if (!arr) return obj;
  if (!obj) return arr;
  return arr < obj ? arr : obj;
}

static void addr_text(const cJSON *person, char *dst, size_t n) {
  const cJSON *name = cJSON_GetObjectItemCaseSensitive(person, "name");
  const cJSON *addr = cJSON_GetObjectItemCaseSensitive(person, "addr");
  const char *ns = cJSON_IsString(name) ? name->valuestring : "";
  const char *as = cJSON_IsString(addr) ? addr->valuestring : "";
  if (*ns && *as) snprintf(dst, n, "%s <%s>", ns, as);
  else snprintf(dst, n, "%s", *as ? as : ns);
}

char *format_envelopes(const char *json_text, int *count) {
  if (count) *count = 0;
  const char *start = json_text ? json_start(json_text) : NULL;
  cJSON *root = start ? cJSON_Parse(start) : NULL;
  if (!cJSON_IsArray(root)) {
    cJSON_Delete(root);
    return strdup(json_text ? json_text : "");
  }
  size_t cap = 1024, len = 0;
  char *buf = malloc(cap);
  buf[0] = 0;
  int n = 0;
  const cJSON *env;
  cJSON_ArrayForEach(env, root) {
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(env, "id");
    const cJSON *subject = cJSON_GetObjectItemCaseSensitive(env, "subject");
    const cJSON *date = cJSON_GetObjectItemCaseSensitive(env, "date");
    const cJSON *flags = cJSON_GetObjectItemCaseSensitive(env, "flags");
    char from[256];
    addr_text(cJSON_GetObjectItemCaseSensitive(env, "from"), from, sizeof from);
    char flagbuf[128] = "";
    if (cJSON_IsArray(flags)) {
      const cJSON *flag;
      cJSON_ArrayForEach(flag, flags) {
        if (!cJSON_IsString(flag)) continue;
        if (flagbuf[0]) strncat(flagbuf, ",", sizeof flagbuf - strlen(flagbuf) - 1);
        strncat(flagbuf, flag->valuestring, sizeof flagbuf - strlen(flagbuf) - 1);
      }
    }
    char line[1024];
    snprintf(line, sizeof line, "%s\t%s\t%s\t%s\t%s\n",
             cJSON_IsString(id) ? id->valuestring : "",
             flagbuf,
             cJSON_IsString(date) ? date->valuestring : "",
             from,
             cJSON_IsString(subject) ? subject->valuestring : "");
    size_t add = strlen(line);
    if (len + add + 1 > cap) {
      cap = (len + add + 1) * 2;
      buf = realloc(buf, cap);
    }
    memcpy(buf + len, line, add + 1);
    len += add;
    n++;
  }
  cJSON_Delete(root);
  if (count) *count = n;
  if (!n) {
    free(buf);
    return strdup("(no messages)");
  }
  return buf;
}

char *read_file(const char *path, size_t max_bytes) {
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  char *buf = malloc(max_bytes + 1);
  size_t n = fread(buf, 1, max_bytes, f);
  int failed = ferror(f) || (n == max_bytes && fgetc(f) != EOF);
  fclose(f);
  if (failed) { free(buf); errno = EFBIG; return NULL; }
  buf[n] = 0;
  return buf;
}

char *state_file(const char *name) {
  const char *base = getenv("XDG_STATE_HOME");
  const char *home = getenv("HOME");
  const char *root = base && *base ? base : (home && *home ? home : "/tmp");
  size_t size = strlen(root) + strlen(name) + 40;
  char *path = malloc(size);
  if (base && *base) snprintf(path, size, "%s/himalaya-mcp/%s", root, name);
  else snprintf(path, size, "%s/.local/state/himalaya-mcp/%s", root, name);
  return path;
}

int mkdir_p(const char *path) {
  char *tmp = strdup(path);
  for (char *p = tmp + 1; *p; p++) {
    if (*p != '/') continue;
    *p = 0;
    if (mkdir(tmp, 0700) < 0 && errno != EEXIST) {
      free(tmp);
      return -1;
    }
    *p = '/';
  }
  int rc = mkdir(tmp, 0700);
  free(tmp);
  return rc < 0 && errno != EEXIST ? -1 : 0;
}

int write_file(const char *path, const char *text) {
  char *dir = strdup(path);
  char *slash = strrchr(dir, '/');
  if (slash) {
    *slash = 0;
    if (mkdir_p(dir) < 0) {
      free(dir);
      return -1;
    }
  }
  free(dir);
  char *tmp = malloc(strlen(path) + 12);
  if (!tmp) return -1;
  sprintf(tmp, "%s.XXXXXX", path);
  int fd = mkstemp(tmp);
  if (fd < 0) { free(tmp); return -1; }
  FILE *f = fdopen(fd, "w");
  if (!f) { close(fd); unlink(tmp); free(tmp); return -1; }
  int failed = fputs(text, f) == EOF;
  if (fflush(f) || fsync(fd)) failed = 1;
  if (fclose(f)) failed = 1;
  if (!failed && rename(tmp, path)) failed = 1;
  if (failed) unlink(tmp);
  free(tmp);
  return failed ? -1 : 0;
}
