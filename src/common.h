#pragma once

#define _POSIX_C_SOURCE 200809L
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
/* Darwin keeps mkdtemp and related declarations behind its extension flag. */
#define _DARWIN_C_SOURCE
#endif

#include <cjson/cJSON.h>
#include <stddef.h>

typedef struct {
  char *text;
  int is_error;
} Result;

Result result_ok(char *text);
Result result_err(const char *msg);
Result result_errf(const char *fmt, ...);
void result_free(Result r);

typedef struct {
  char **v;
  size_t n;
  size_t cap;
} Argv;

void argv_init(Argv *a);
void argv_add(Argv *a, const char *s);
void argv_free(Argv *a);

typedef struct {
  char *out;
  char *err;
  int status;
} Capture;

void capture_free(Capture *c);
int run_cmd(char *const argv[], Capture *cap);
int run_cmd_input(char *const argv[], Capture *cap, const char *input);

const char *arg_str(const cJSON *args, const char *key);
int arg_int(const cJSON *args, const char *key, int fallback);
int arg_bool(const cJSON *args, const char *key);
const cJSON *arg_array(const cJSON *args, const char *key);

int valid_id(const char *s);
int valid_folder(const char *s);
int valid_account(const char *s);
int valid_flag(const char *s);

void him_start(Argv *a);
void him_opts(Argv *a, const char *account);
Result him_run(Argv *a);
Result him_run_input(Argv *a, const char *input);
char *format_envelopes(const char *json_text, int *count);
char *read_file(const char *path, size_t max_bytes);
char *state_file(const char *name);
int write_file(const char *path, const char *text);
int mkdir_p(const char *path);
