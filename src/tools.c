#include "tools.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static const char *account_of(const cJSON *args) {
  const char *account = arg_str(args, "account");
  if (account && !valid_account(account)) return NULL;
  return account ? account : "";
}

static const char *folder_of(const cJSON *args) {
  const char *folder = arg_str(args, "folder");
  if (!folder || !*folder) return "INBOX";
  return valid_folder(folder) ? folder : NULL;
}

static Result envelopes_page(const cJSON *args, const char *query, int page, int page_size, int *count_out) {
  const char *account = account_of(args);
  const char *folder = folder_of(args);
  if (!account || !folder) return result_err("invalid account or folder");
  if (page < 1) page = 1;
  if (page_size < 1) page_size = 25;
  if (page_size > 200) page_size = 200;
  Argv a;
  him_start(&a);
  argv_add(&a, "envelope");
  argv_add(&a, "list");
  him_opts(&a, account);
  argv_add(&a, "-f");
  argv_add(&a, folder);
  char page_s[16], size_s[16];
  snprintf(page_s, sizeof page_s, "%d", page);
  snprintf(size_s, sizeof size_s, "%d", page_size);
  argv_add(&a, "-p");
  argv_add(&a, page_s);
  argv_add(&a, "-s");
  argv_add(&a, size_s);
  if (query && *query) {
    argv_add(&a, "--");
    argv_add(&a, query);
  }
  Result raw = him_run(&a);
  if (raw.is_error) return raw;
  cJSON *rows = cJSON_Parse(raw.text);
  if (!cJSON_IsArray(rows)) { cJSON_Delete(rows); result_free(raw); return result_err("invalid envelope response"); }
  if (count_out) *count_out = cJSON_GetArraySize(rows);
  cJSON_Delete(rows);
  char *text = format_envelopes(raw.text, NULL);
  result_free(raw);
  return result_ok(text);
}

static Result envelopes(const cJSON *args, const char *query, int page, int page_size) {
  return envelopes_page(args, query, page, page_size, NULL);
}

Result tool_list_emails(const cJSON *args) {
  return envelopes(args, NULL, arg_int(args, "page", 1), arg_int(args, "page_size", 25));
}

Result tool_search_emails(const cJSON *args) {
  const char *query = arg_str(args, "query");
  if (!query || !*query) return result_err("query is required");
  if (strchr(query, '\n')) return result_err("query must be one line");
  return envelopes(args, query, arg_int(args, "page", 1), arg_int(args, "page_size", 25));
}

Result tool_get_unread_count(const cJSON *args) {
  int total = 0;
  for (int page = 1; page <= 20; page++) {
    int count = 0;
    Result page_text = envelopes_page(args, "not flag Seen", page, 100, &count);
    if (page_text.is_error) return page_text;
    result_free(page_text);
    total += count;
    if (count < 100) break;
    if (page == 20) return result_err("unread count reached safety limit: at least 2000; exact total unknown");
  }
  char *text = malloc(32);
  snprintf(text, 32, "%d\n", total);
  return result_ok(text);
}

Result tool_list_starred(const cJSON *args) {
  return envelopes(args, "flag Flagged", 1, arg_int(args, "page_size", 50));
}

static Result read_plain(const cJSON *args) {
  const char *id = arg_str(args, "id");
  const char *account = account_of(args);
  const char *folder = folder_of(args);
  if (!valid_id(id) || !account || !folder) return result_err("id, account, or folder is invalid");
  Argv a;
  him_start(&a);
  argv_add(&a, "message");
  argv_add(&a, "read");
  him_opts(&a, account);
  argv_add(&a, "-p");
  argv_add(&a, "-f");
  argv_add(&a, folder);
  argv_add(&a, "--");
  argv_add(&a, id);
  Result r = him_run(&a);
  if (!r.is_error && (!r.text || !*r.text)) { result_free(r); return result_err("message not found or empty response"); }
  return r;
}

Result tool_read_email(const cJSON *args) { return read_plain(args); }

static char *export_dir(const cJSON *args, int full) {
  const char *id = arg_str(args, "id");
  const char *account = account_of(args);
  const char *folder = folder_of(args);
  if (!valid_id(id) || !account || !folder) return NULL;
  char tmpl[] = "/tmp/himalaya-mcp-XXXXXX";
  char *dir = mkdtemp(tmpl);
  if (!dir) return NULL;
  dir = strdup(dir);
  Argv a;
  him_start(&a);
  argv_add(&a, "message");
  argv_add(&a, "export");
  him_opts(&a, account);
  argv_add(&a, "-f");
  argv_add(&a, folder);
  argv_add(&a, "-d");
  argv_add(&a, dir);
  if (full) argv_add(&a, "-F");
  argv_add(&a, "--");
  argv_add(&a, id);
  Result raw = him_run(&a);
  if (raw.is_error) {
    free(dir);
    result_free(raw);
    return NULL;
  }
  result_free(raw);
  return dir;
}

Result tool_read_email_html(const cJSON *args) {
  char *dir = export_dir(args, 0);
  if (!dir) return result_err("could not export message");
  DIR *d = opendir(dir);
  char path[512] = "";
  if (d) {
    struct dirent *ent;
    while ((ent = readdir(d))) {
      size_t n = strlen(ent->d_name);
      if (n > 5 && strcmp(ent->d_name + n - 5, ".html") == 0) {
        snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
        break;
      }
    }
    closedir(d);
  }
  char *text = path[0] ? read_file(path, 2 * 1024 * 1024) : NULL;
  free(dir);
  if (!text) return result_err("no HTML part in message");
  return result_ok(text);
}

Result tool_read_email_raw(const cJSON *args) {
  char *dir = export_dir(args, 1);
  if (!dir) return result_err("could not export raw message");
  DIR *d = opendir(dir);
  char path[512] = "";
  if (d) {
    struct dirent *ent;
    while ((ent = readdir(d))) {
      if (ent->d_name[0] == '.') continue;
      snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
      break;
    }
    closedir(d);
  }
  char *text = path[0] ? read_file(path, 4 * 1024 * 1024) : NULL;
  free(dir);
  if (!text) return result_err("raw export was empty");
  return result_ok(text);
}

static void strip_tags(const char *html, char *out, size_t n) {
  int tag = 0;
  size_t w = 0;
  for (const char *p = html; *p && w + 1 < n; p++) {
    if (*p == '<') {
      tag = 1;
      continue;
    }
    if (*p == '>') {
      tag = 0;
      continue;
    }
    if (!tag) out[w++] = *p;
  }
  out[w] = 0;
}

Result tool_render_email(const cJSON *args) {
  Result plain = read_plain(args);
  if (!plain.is_error && plain.text && *plain.text) return plain;
  result_free(plain);
  Result html = tool_read_email_html(args);
  if (html.is_error) return html;
  char *text = malloc(strlen(html.text) + 1);
  strip_tags(html.text, text, strlen(html.text) + 1);
  result_free(html);
  return result_ok(text);
}

Result tool_flag_email(const cJSON *args) {
  const char *id = arg_str(args, "id");
  const char *action = arg_str(args, "action");
  const char *account = account_of(args);
  const char *folder = folder_of(args);
  const cJSON *flags = arg_array(args, "flags");
  if (!valid_id(id) || !account || !folder || !flags) return result_err("id, flags, account, or folder is invalid");
  if (!action || (strcmp(action, "add") != 0 && strcmp(action, "remove") != 0))
    return result_err("action must be add or remove");
  Argv a;
  him_start(&a);
  argv_add(&a, "flag");
  argv_add(&a, action);
  him_opts(&a, account);
  argv_add(&a, "-f");
  argv_add(&a, folder);
  argv_add(&a, "--");
  argv_add(&a, id);
  const cJSON *flag;
  cJSON_ArrayForEach(flag, flags) {
    if (!cJSON_IsString(flag) || !valid_flag(flag->valuestring)) {
      argv_free(&a);
      return result_err("invalid flag");
    }
    argv_add(&a, flag->valuestring);
  }
  Result raw = him_run(&a);
  if (raw.is_error) return raw;
  result_free(raw);
  return result_ok(strdup("flags updated\n"));
}

Result tool_move_email(const cJSON *args) {
  const char *id = arg_str(args, "id");
  const char *target = arg_str(args, "target_folder");
  const char *account = account_of(args);
  const char *folder = folder_of(args);
  if (!valid_id(id) || !account || !folder || !target || !valid_folder(target))
    return result_err("id, folder, or target_folder is invalid");
  Argv a;
  him_start(&a);
  argv_add(&a, "message");
  argv_add(&a, "move");
  him_opts(&a, account);
  argv_add(&a, "-f");
  argv_add(&a, folder);
  argv_add(&a, "--");
  argv_add(&a, target);
  argv_add(&a, id);
  Result raw = him_run(&a);
  if (raw.is_error) return raw;
  result_free(raw);
  return result_ok(strdup("moved\n"));
}

Result tool_list_folders(const cJSON *args) {
  const char *account = account_of(args);
  if (!account) return result_err("invalid account");
  Argv a;
  him_start(&a);
  argv_add(&a, "folder");
  argv_add(&a, "list");
  him_opts(&a, account);
  return him_run(&a);
}

Result tool_create_folder(const cJSON *args) {
  const char *name = arg_str(args, "name");
  const char *account = account_of(args);
  if (!account || !name || !valid_folder(name)) return result_err("invalid folder name");
  Argv a;
  him_start(&a);
  argv_add(&a, "folder");
  argv_add(&a, "add");
  him_opts(&a, account);
  argv_add(&a, "--");
  argv_add(&a, name);
  Result raw = him_run(&a);
  if (raw.is_error) return raw;
  result_free(raw);
  return result_ok(strdup("folder created\n"));
}

Result tool_delete_folder(const cJSON *args) {
  const char *name = arg_str(args, "name");
  const char *account = account_of(args);
  if (!account || !name || !valid_folder(name)) return result_err("invalid folder name");
  if (!arg_bool(args, "confirm")) {
    char *text = malloc(256);
    snprintf(text, 256, "PREVIEW: delete folder \"%s\" and every message in it. Call again with confirm=true to delete.\n", name);
    return result_ok(text);
  }
  Argv a;
  him_start(&a);
  argv_add(&a, "folder");
  argv_add(&a, "delete");
  him_opts(&a, account);
  argv_add(&a, "-y");
  argv_add(&a, "--");
  argv_add(&a, name);
  Result raw = him_run(&a);
  if (raw.is_error) return raw;
  result_free(raw);
  return result_ok(strdup("folder deleted\n"));
}

/* Let Himalaya supply the selected account's From header and signature. */
static Result template_from_args(const cJSON *args) {
  const char *account = account_of(args);
  const char *body = arg_str(args, "body");
  if (!account || !arg_str(args, "to") || !arg_str(args, "subject") || !body)
    return result_err("to, subject, body and a valid account are required");
  /* Plain composition must not turn body text into local-file MML directives. */
  if (strstr(body, "<#")) return result_err("MML directives are not accepted in plain body text");
  Argv a;
  him_start(&a);
  argv_add(&a, "template");
  argv_add(&a, "write");
  him_opts(&a, account);
  const char *keys[] = {"to", "subject", "cc", "bcc"};
  const char *headers[] = {"To", "Subject", "Cc", "Bcc"};
  for (size_t i = 0; i < 4; i++) {
    const char *value = arg_str(args, keys[i]);
    if (!value) continue;
    if (strpbrk(value, "\r\n")) {
      argv_free(&a);
      return result_err("email headers must not contain CR or LF");
    }
    size_t size = strlen(value) + strlen(headers[i]) + 3;
    char *header = malloc(size);
    snprintf(header, size, "%s: %s", headers[i], value);
    argv_add(&a, "-H");
    argv_add(&a, header);
    free(header);
  }
  argv_add(&a, "--");
  argv_add(&a, body);
  return him_run(&a);
}

static Result attach_files(const cJSON *args, const char *template) {
  char *text = strdup(template);
  const cJSON *files = cJSON_GetObjectItemCaseSensitive(args, "attachments");
  if (files && !cJSON_IsArray(files)) { free(text); return result_err("attachments must be an array"); }
  const cJSON *file;
  cJSON_ArrayForEach(file, files) {
    struct stat st;
    const char *path = cJSON_IsString(file) ? file->valuestring : NULL;
    if (!path || *path != '/' || strpbrk(path, "\r\n\"\\<>") ||
        stat(path, &st) != 0 || !S_ISREG(st.st_mode) || access(path, R_OK) != 0) {
      free(text);
      return result_err("attachment must be a readable absolute file path without MML delimiters");
    }
    size_t used = strlen(text), size = used + strlen(path) + 80;
    char *next = realloc(text, size);
    if (!next) { free(text); return result_err("out of memory"); }
    text = next;
    snprintf(text + used, size - used,
             "\n<#part disposition=attachment filename=\"%s\"><#/part>\n", path);
  }
  return result_ok(text);
}

static Result send_template(const char *account, const char *template) {
  Argv a;
  him_start(&a);
  argv_add(&a, "template");
  argv_add(&a, "send");
  him_opts(&a, account);
  Result raw = him_run_input(&a, template);
  if (raw.is_error) return raw;
  result_free(raw);
  return result_ok(strdup("sent\n"));
}

Result tool_compose_email(const cJSON *args) {
  const char *account = account_of(args);
  Result generated = template_from_args(args);
  if (generated.is_error) return generated;
  Result attached = attach_files(args, generated.text);
  result_free(generated);
  if (attached.is_error) return attached;
  char *template = attached.text;
  if (!arg_bool(args, "confirm")) {
    char *preview = malloc(strlen(template) + 80);
    sprintf(preview, "PREVIEW: not sent. Call again with confirm=true to send.\n\n%s", template);
    free(template);
    return result_ok(preview);
  }
  Result sent = send_template(account, template);
  free(template);
  return sent;
}

Result tool_draft_reply(const cJSON *args) {
  const char *id = arg_str(args, "id");
  const char *body = arg_str(args, "body");
  const char *account = account_of(args);
  const char *folder = folder_of(args);
  if (!valid_id(id) || !account || !folder) return result_err("invalid id or folder");
  Argv a;
  him_start(&a);
  argv_add(&a, "template");
  argv_add(&a, "reply");
  him_opts(&a, account);
  argv_add(&a, "-f");
  argv_add(&a, folder);
  if (arg_bool(args, "reply_all")) argv_add(&a, "-A");
  argv_add(&a, "--");
  argv_add(&a, id);
  if (body && *body) argv_add(&a, body);
  Result raw = him_run(&a);
  if (raw.is_error) return raw;
  /* Return a reusable MML template, not a status prefix that breaks parsing. */
  return raw;
}

Result tool_send_email(const cJSON *args) {
  const char *template = arg_str(args, "template");
  const char *account = account_of(args);
  if (!account || !template || !*template) return result_err("template is required");
  Result attached = attach_files(args, template);
  if (attached.is_error) return attached;
  char *with_attachments = attached.text;
  if (!arg_bool(args, "confirm")) {
    char *preview = malloc(strlen(with_attachments) + 80);
    sprintf(preview, "PREVIEW: not sent. Call again with confirm=true to send.\n\n%s", with_attachments);
    free(with_attachments);
    return result_ok(preview);
  }
  Result sent = send_template(account, with_attachments);
  free(with_attachments);
  return sent;
}

Result tool_save_draft(const cJSON *args) {
  const char *template = arg_str(args, "template");
  const char *account = account_of(args);
  const char *folder = arg_str(args, "folder");
  if (!folder) folder = "drafts"; /* Himalaya's configured folder alias. */
  if (!template || !*template || !account || !valid_folder(folder))
    return result_err("template and valid account/folder are required");
  if (strstr(template, "<#") || cJSON_GetObjectItemCaseSensitive(args, "attachments"))
    return result_err("save_draft accepts plain-text message templates only, not MML or attachments");
  if (strncmp(template, "From:", 5) || !strstr(template, "\n\n"))
    return result_err("draft needs headers starting with From and a blank line before the body");
  const char *separator = strstr(template, "\n\n");
  for (const char *line = template; line < separator; line = strchr(line, '\n') + 1) {
    if (!strncasecmp(line, "Content-", 8) || !strncasecmp(line, "MIME-Version:", 13))
      return result_err("save_draft expects a plain-text template without MIME headers");
  }
  Argv a;
  him_start(&a);
  /* Himalaya 1.2 template save appends twice. Raw message save appends once.
     Its JSON mode ignores stdin, so select plain output explicitly. */
  argv_add(&a, "message");
  argv_add(&a, "save");
  argv_add(&a, "-o");
  argv_add(&a, "plain");
  if (*account) { argv_add(&a, "-a"); argv_add(&a, account); }
  argv_add(&a, "-f");
  argv_add(&a, folder);
  const char *body = strstr(template, "\n\n");
  size_t headers = (size_t)(body - template);
  const char *mime = "\nMIME-Version: 1.0\nContent-Type: text/plain; charset=UTF-8\nContent-Transfer-Encoding: 8bit";
  size_t size = strlen(template) + strlen(mime) + 1;
  char *message = malloc(size);
  if (!message) { argv_free(&a); return result_err("out of memory"); }
  memcpy(message, template, headers);
  strcpy(message + headers, mime);
  strcat(message, body);
  Result saved = him_run_input(&a, message);
  free(message);
  if (saved.is_error) return saved;
  result_free(saved);
  return result_ok(strdup("draft saved; not sent\n"));
}

Result tool_export_to_markdown(const cJSON *args) {
  Result body = read_plain(args);
  if (body.is_error) return body;
  const char *id = arg_str(args, "id");
  char *text = malloc(strlen(body.text) + 128);
  sprintf(text, "---\nid: \"%s\"\n---\n\n%s", id ? id : "", body.text);
  result_free(body);
  return result_ok(text);
}

Result tool_create_action_item(const cJSON *args) {
  Result body = read_plain(args);
  if (body.is_error) return body;
  size_t cap = strlen(body.text) + 256;
  char *out = malloc(cap);
  snprintf(out, cap, "Review this message and treat lines below as candidate actions.\n\n%s", body.text);
  const char *dest = arg_str(args, "destination");
  if (dest && *dest && (dest[0] != '/' || strchr(dest, '\n') || write_file(dest, out))) {
    free(out); result_free(body); return result_err("could not write action-item destination; use a writable absolute path");
  }
  result_free(body);
  return result_ok(out);
}

Result tool_copy_to_clipboard(const cJSON *args) {
  const char *text = arg_str(args, "text");
  if (!text) return result_err("text is required");
  const char *bins[] = {"wl-copy", "xclip", "pbcopy", NULL};
  for (int i = 0; bins[i]; i++) {
    Argv a;
    argv_init(&a);
    argv_add(&a, bins[i]);
    if (strcmp(bins[i], "xclip") == 0) {
      argv_add(&a, "-selection");
      argv_add(&a, "clipboard");
    }
    argv_add(&a, NULL);
    Capture cap = {0};
    int rc = run_cmd_input(a.v, &cap, text);
    int success = rc == 0 && cap.status == 0;
    capture_free(&cap);
    argv_free(&a);
    if (success) return result_ok(strdup("copied\n"));
  }
  return result_err("no clipboard tool found (wl-copy, xclip, or pbcopy)");
}

static Result attachment_dir(const cJSON *args, char **dir_out) {
  const char *id = arg_str(args, "id"), *account = account_of(args), *folder = folder_of(args);
  if (!valid_id(id) || !account || !folder) return result_err("invalid id, account or folder");
  char dir[] = "/tmp/himalaya-mcp-attachments-XXXXXX";
  if (!mkdtemp(dir)) return result_err("could not create attachment directory");
  Argv a;
  him_start(&a);
  argv_add(&a, "attachment");
  argv_add(&a, "download");
  him_opts(&a, account);
  argv_add(&a, "-f");
  argv_add(&a, folder);
  argv_add(&a, "-d");
  argv_add(&a, dir);
  argv_add(&a, "--");
  argv_add(&a, id);
  Result downloaded = him_run(&a);
  if (downloaded.is_error) { rmdir(dir); return downloaded; }
  *dir_out = strdup(dir);
  return downloaded;
}

Result tool_list_attachments(const cJSON *args) {
  char *dir = NULL;
  Result prep = attachment_dir(args, &dir);
  if (prep.is_error) return prep;
  result_free(prep);
  DIR *d = opendir(dir);
  char *text = strdup("");
  if (d) {
    struct dirent *ent;
    while ((ent = readdir(d))) {
      if (ent->d_name[0] == '.') continue;
      char path[512];
      snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
      struct stat st;
      long size = stat(path, &st) == 0 ? (long)st.st_size : 0;
      char *next = malloc(strlen(text) + strlen(ent->d_name) + 64);
      sprintf(next, "%s%s\t%ld\n", text, ent->d_name, size);
      free(text);
      text = next;
    }
    closedir(d);
  }
  free(dir);
  if (!text[0]) {
    free(text);
    return result_ok(strdup("(no attachments)\n"));
  }
  return result_ok(text);
}

Result tool_download_attachment(const cJSON *args) {
  const char *filename = arg_str(args, "filename");
  if (!filename || strchr(filename, '/') || strchr(filename, '\\')) return result_err("filename must be a bare name");
  char *dir = NULL;
  Result prep = attachment_dir(args, &dir);
  if (prep.is_error) return prep;
  result_free(prep);
  size_t size = strlen(dir) + strlen(filename) + 2;
  char *path = malloc(size);
  snprintf(path, size, "%s/%s", dir, filename);
  free(dir);
  struct stat st;
  if (lstat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
    free(path);
    return result_err("attachment not found or not a regular file");
  }
  /* Export already wrote the original bytes in a private directory. No copy. */
  return result_ok(path);
}

Result tool_extract_calendar_event(const cJSON *args) {
  char *dir = NULL;
  Result prep = attachment_dir(args, &dir);
  if (prep.is_error) return prep;
  result_free(prep);
  DIR *d = opendir(dir);
  char path[512] = "";
  if (d) {
    struct dirent *ent;
    while ((ent = readdir(d))) {
      size_t n = strlen(ent->d_name);
      if (n > 4 && (ent->d_name[n - 4] == '.') &&
          tolower((unsigned char)ent->d_name[n - 3]) == 'i' &&
          tolower((unsigned char)ent->d_name[n - 2]) == 'c' &&
          tolower((unsigned char)ent->d_name[n - 1]) == 's') {
        snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);
        break;
      }
    }
    closedir(d);
  }
  char *ics = path[0] ? read_file(path, 1024 * 1024) : NULL;
  free(dir);
  if (!ics) return result_err("no calendar attachment");
  return result_ok(ics);
}

static int valid_date(int year, int month, int day, int hour, int min, int sec);

static int calendar_time(const char *s) {
  if (!s || strlen(s) != 16 || s[8] != 'T' || s[15] != 'Z') return 0;
  for (int i = 0; i < 15; i++) if (i != 8 && !isdigit((unsigned char)s[i])) return 0;
  int y, mo, d, h, mi, sec;
  return sscanf(s, "%4d%2d%2dT%2d%2d%2dZ", &y,&mo,&d,&h,&mi,&sec) == 6 && valid_date(y,mo,d,h,mi,sec);
}

static char *calendar_escape(const char *s) {
  if (!s) s = "";
  char *out = malloc(strlen(s) * 2 + 1), *w = out;
  for (; *s; s++) {
    if (*s == '\r') continue;
    if (*s == '\n') { *w++ = '\\'; *w++ = 'n'; }
    else { if (strchr("\\,;", *s)) *w++ = '\\'; *w++ = *s; }
  }
  *w = 0;
  return out;
}

Result tool_create_calendar_event(const cJSON *args) {
  const char *summary = arg_str(args, "summary"), *start = arg_str(args, "dtstart"), *end = arg_str(args, "dtend");
  if (!summary || !*summary || !calendar_time(start) || !calendar_time(end) || strcmp(start, end) >= 0)
    return result_err("summary and increasing UTC timestamps YYYYMMDDTHHMMSSZ are required");
  char stamp[32];
  time_t now = time(NULL);
  struct tm tm;
  gmtime_r(&now, &tm);
  strftime(stamp, sizeof stamp, "%Y%m%dT%H%M%SZ", &tm);
  char *s = calendar_escape(summary), *location = calendar_escape(arg_str(args, "location")), *description = calendar_escape(arg_str(args, "description"));
  static unsigned sequence;
  size_t size = strlen(s) + strlen(location) + strlen(description) + 512;
  char *raw = malloc(size);
  snprintf(raw, size, "BEGIN:VCALENDAR\r\nVERSION:2.0\r\nPRODID:-//himalaya-mcp//EN\r\nBEGIN:VEVENT\r\nUID:%s-%ld-%u@himalaya-mcp\r\nDTSTAMP:%s\r\nSUMMARY:%s\r\nDTSTART:%s\r\nDTEND:%s\r\nLOCATION:%s\r\nDESCRIPTION:%s\r\nEND:VEVENT\r\nEND:VCALENDAR\r\n", stamp, (long)getpid(), ++sequence, stamp, s, start, end, location, description);
  free(s); free(location); free(description);
  /* Fold at 75 octets, without splitting UTF-8 continuation bytes. */
  char *ics = malloc(strlen(raw) * 2 + 1), *w = ics;
  size_t column = 0;
  for (const unsigned char *p = (unsigned char *)raw; *p; p++) {
    if (column >= 71 && (*p & 0xc0) != 0x80 && *p != '\r' && *p != '\n') { memcpy(w, "\r\n ", 3); w += 3; column = 1; }
    *w++ = (char)*p;
    if (*p == '\n') column = 0; else column++;
  }
  *w = 0;
  free(raw);
  if (!arg_bool(args, "confirm")) {
    char *preview = malloc(strlen(ics) + 96);
    sprintf(preview, "PREVIEW: calendar event not created. Call again with confirm=true.\n\n%s", ics);
    free(ics);
    return result_ok(preview);
  }
  char *dir = state_file("");
  if (mkdir_p(dir)) { free(dir); free(ics); return result_err("could not create calendar directory"); }
  free(dir);
  dir = state_file("event-XXXXXX");
  if (!mkdtemp(dir)) { free(dir); free(ics); return result_err("could not create event directory"); }
  char *path = malloc(strlen(dir) + 12);
  if (!path) { rmdir(dir); free(dir); free(ics); return result_err("out of memory"); }
  sprintf(path, "%s/event.ics", dir);
  int failed = write_file(path, ics);
  free(ics);
  if (failed) { rmdir(dir); free(dir); free(path); return result_err("could not write calendar event"); }
  free(dir);
  char *text = malloc(strlen(path) + 16);
  sprintf(text, "wrote %s\n", path);
  free(path);
  return result_ok(text);
}

Result tool_list_threads(const cJSON *args) {
  Result listed = envelopes(args, NULL, 1, arg_int(args, "page_size", 50));
  if (listed.is_error) return listed;
  return listed;
}

Result tool_read_thread(const cJSON *args) {
  const char *thread_id = arg_str(args, "thread_id");
  if (!thread_id || !*thread_id || strchr(thread_id, '\n')) return result_err("thread_id is required");
  if (strpbrk(thread_id, "\r\"\\")) return result_err("thread subject cannot contain quotes, backslashes or line breaks");
  char *query = malloc(strlen(thread_id) + 12);
  sprintf(query, "subject \"%s\"", thread_id);
  Result listed = envelopes(args, query, 1, 20);
  free(query);
  if (listed.is_error) return listed;
  char *out = malloc(strlen(listed.text) + strlen(thread_id) + 16);
  sprintf(out, "Thread %s\n%s", thread_id, listed.text);
  result_free(listed);
  return result_ok(out);
}

static void iso_now(char *dst, size_t n, time_t when) {
  struct tm tm;
  localtime_r(&when, &tm);
  strftime(dst, n, "%Y-%m-%dT%H:%M:%S", &tm);
}

static int valid_date(int year, int month, int day, int hour, int min, int sec) {
  const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (year < 1970 || year > 9999 || month < 1 || month > 12 || day < 1 || hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 59) return 0;
  int leap = month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
  return day <= days[month - 1] + leap;
}

static int valid_iso(const char *text) {
  int y, mo, d, h, mi, s, pos = 0;
  if (sscanf(text, "%4d-%2d-%2dT%2d:%2d:%2d%n", &y, &mo, &d, &h, &mi, &s, &pos) != 6 || pos != 19 || !valid_date(y,mo,d,h,mi,s)) return 0;
  char canonical[32];
  snprintf(canonical, sizeof canonical, "%04d-%02d-%02dT%02d:%02d:%02d", y,mo,d,h,mi,s);
  if (strncmp(text, canonical, 19)) return 0;
  const char *zone = text + 19;
  if (!*zone || !strcmp(zone, "Z")) return 1;
  if (strlen(zone) != 6 || zone[3] != ':' || !isdigit((unsigned char)zone[1]) || !isdigit((unsigned char)zone[2]) || !isdigit((unsigned char)zone[4]) || !isdigit((unsigned char)zone[5])) return 0;
  int zh, zm, used = 0;
  return (*zone == '+' || *zone == '-') && sscanf(zone + 1, "%2d:%2d%n", &zh, &zm, &used) == 2 && used == 5 && strlen(zone) == 6 && zh >= 0 && zh <= 23 && zm >= 0 && zm <= 59;
}

static int parse_until(const char *text, char *dst, size_t n) {
  time_t now = time(NULL);
  if (!text || !*text) return -1;
  if (!strcmp(text, "tomorrow")) {
    iso_now(dst, n, now + 24 * 60 * 60);
    return 0;
  }
  if (text[strlen(text) - 1] == 'h' || text[strlen(text) - 1] == 'd') {
    char *end;
    errno = 0;
    long num = strtol(text, &end, 10);
    if (errno || num <= 0 || num > 36500 || end != text + strlen(text) - 1) return -1;
    iso_now(dst, n, now + (text[strlen(text) - 1] == 'h' ? num * 3600L : num * 86400L));
    return 0;
  }
  if (!valid_iso(text) || strlen(text) >= n) return -1;
  snprintf(dst, n, "%s", text);
  return 0;
}

static cJSON *load_records(const char *path) {
  errno = 0;
  char *existing = read_file(path, 1024 * 1024);
  if (!existing) return errno == ENOENT ? cJSON_CreateArray() : NULL;
  cJSON *arr = cJSON_Parse(existing);
  free(existing);
  if (!cJSON_IsArray(arr)) { cJSON_Delete(arr); return NULL; }
  return arr;
}

Result tool_snooze_email(const cJSON *args) {
  const char *id = arg_str(args, "id");
  const char *until_in = arg_str(args, "snoozeUntil");
  const char *account = account_of(args);
  const char *folder = folder_of(args);
  if (!valid_id(id) || !account || !folder || !until_in) return result_err("id and snoozeUntil are required");
  char until[64];
  if (parse_until(until_in, until, sizeof until) < 0) return result_err("could not parse snoozeUntil");
  char *path = state_file("snooze.json");
  cJSON *arr = load_records(path);
  if (!arr) { free(path); return result_err("cannot read snooze records; existing state left unchanged"); }
  cJSON *item = cJSON_CreateObject();
  cJSON_AddStringToObject(item, "id", id);
  cJSON_AddStringToObject(item, "folder", folder);
  cJSON_AddStringToObject(item, "account", account);
  cJSON_AddStringToObject(item, "subject", arg_str(args, "subject") ? arg_str(args, "subject") : "");
  cJSON_AddStringToObject(item, "snoozeUntil", until);
  cJSON_AddItemToArray(arr, item);
  char *printed = cJSON_Print(arr);
  int failed = write_file(path, printed);
  char *text = malloc(strlen(id) + strlen(until) + 32);
  sprintf(text, "snoozed %s until %s\n", id, until);
  free(printed);
  free(path);
  cJSON_Delete(arr);
  if (failed) { free(text); return result_err("could not write snooze records"); }
  return result_ok(text);
}

Result tool_list_snoozed_emails(const cJSON *args) {
  (void)args;
  char *path = state_file("snooze.json");
  cJSON *records = load_records(path);
  free(path);
  if (!records) return result_err("cannot read snooze records");
  char *printed = cJSON_Print(records);
  cJSON_Delete(records);
  return result_ok(printed);
}

Result tool_create_reminder(const cJSON *args) {
  const char *title = arg_str(args, "title");
  if (!title || !*title || strpbrk(title, "\r\n")) return result_err("title is required");
  const char *due = arg_str(args, "dueDate");
  if (due && !valid_iso(due)) return result_err("dueDate must be an ISO timestamp");
  int priority = arg_int(args, "priority", 0);
  if (priority < 0 || priority > 9) return result_err("priority must be between 0 and 9");
  char *path = state_file("reminders.json");
  cJSON *arr = load_records(path);
  if (!arr) { free(path); return result_err("cannot read reminder records; existing state left unchanged"); }
  cJSON *item = cJSON_CreateObject();
  cJSON_AddStringToObject(item, "title", title);
  if (arg_str(args, "notes")) cJSON_AddStringToObject(item, "notes", arg_str(args, "notes"));
  if (due) cJSON_AddStringToObject(item, "dueDate", due);
  cJSON_AddNumberToObject(item, "priority", priority);
  cJSON_AddItemToArray(arr, item);
  char *printed = cJSON_Print(arr);
  int failed = write_file(path, printed);
  char *text = malloc(strlen(title) + 32);
  sprintf(text, "reminder stored: %s\n", title);
  free(printed);
  free(path);
  cJSON_Delete(arr);
  if (failed) { free(text); return result_err("could not write reminder records"); }
  return result_ok(text);
}

Result tool_health_check(const cJSON *args) {
  const char *account = account_of(args);
  if (!account) return result_err("invalid account");
  Argv listed;
  him_start(&listed);
  argv_add(&listed, "account");
  argv_add(&listed, "list");
  him_opts(&listed, "");
  Result accounts = him_run(&listed);
  if (accounts.is_error) return accounts;
  Argv folders;
  him_start(&folders);
  argv_add(&folders, "folder");
  argv_add(&folders, "list");
  him_opts(&folders, account);
  Result folder_text = him_run(&folders);
  if (folder_text.is_error) { result_free(accounts); return folder_text; }
  char *text = malloc(strlen(accounts.text) + (folder_text.text ? strlen(folder_text.text) : 0) + 64);
  sprintf(text, "accounts:\n%s\nfolders:\n%s", accounts.text, folder_text.text);
  result_free(accounts);
  result_free(folder_text);
  return result_ok(text);
}
