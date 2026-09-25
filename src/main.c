#include "tools.h"
#include "date.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef Result (*ToolFn)(const cJSON *args);

typedef struct {
  const char *name;
  const char *description;
  const char *schema;
  ToolFn fn;
} Tool;

static const Tool tools[] = {
    {"list_emails", "List message envelopes in a folder.",
     "{\"type\":\"object\",\"properties\":{\"folder\":{\"type\":\"string\"},\"page\":{\"type\":\"integer\"},\"page_size\":{\"type\":\"integer\"},\"account\":{\"type\":\"string\"}}}",
     tool_list_emails},
    {"search_emails", "Search envelopes with himalaya filter syntax. Put and/or between conditions.",
     "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"page\":{\"type\":\"integer\"},\"page_size\":{\"type\":\"integer\"},\"account\":{\"type\":\"string\"}},\"required\":[\"query\"]}",
     tool_search_emails},
    {"get_unread_count", "Count unread messages in a folder. Returns an error rather than an exact count if the 2000-message safety limit is reached.",
     "{\"type\":\"object\",\"properties\":{\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}}}",
     tool_get_unread_count},
    {"list_starred", "List flagged messages.",
     "{\"type\":\"object\",\"properties\":{\"folder\":{\"type\":\"string\"},\"page_size\":{\"type\":\"integer\"},\"account\":{\"type\":\"string\"}}}",
     tool_list_starred},
    {"read_email", "Read a message as text without marking it seen.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_read_email},
    {"read_email_html", "Read the HTML part of a message. Himalaya export marks it Seen.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_read_email_html},
    {"read_email_raw", "Read the raw MIME source of a message. Himalaya export marks it Seen.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_read_email_raw},
    {"render_email", "Read Himalaya's human-readable message, which may include attachment MML. Falls back to basic HTML tag stripping, not browser rendering.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_render_email},
    {"flag_email", "Add or remove flags: Seen, Flagged, Answered, Deleted, Draft.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"flags\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},\"action\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\",\"flags\",\"action\"]}",
     tool_flag_email},
    {"move_email", "Move a message to another folder.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"target_folder\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\",\"target_folder\"]}",
     tool_move_email},
    {"list_folders", "List folders for an account.",
     "{\"type\":\"object\",\"properties\":{\"account\":{\"type\":\"string\"}}}", tool_list_folders},
    {"create_folder", "Create a folder.",
     "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"name\"]}",
     tool_create_folder},
    {"delete_folder", "Delete a folder and its messages. Requires confirm=true.",
     "{\"type\":\"object\",\"properties\":{\"name\":{\"type\":\"string\"},\"confirm\":{\"type\":\"boolean\"},\"account\":{\"type\":\"string\"}},\"required\":[\"name\"]}",
     tool_delete_folder},
    {"compose_email", "Compose a new message. Preview first; send only when confirm=true.",
     "{\"type\":\"object\",\"properties\":{\"to\":{\"type\":\"string\"},\"subject\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"},\"cc\":{\"type\":\"string\"},\"bcc\":{\"type\":\"string\"},\"attachments\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},\"confirm\":{\"type\":\"boolean\"},\"account\":{\"type\":\"string\"}},\"required\":[\"to\",\"subject\",\"body\"]}",
     tool_compose_email},
    {"export_to_markdown", "Export a message as markdown with an id header.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_export_to_markdown},
    {"create_action_item", "Return a message so action items, deadlines, and questions can be extracted. Optional destination writes the text.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"},\"destination\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_create_action_item},
    {"draft_reply", "Build a reply template. Does not send.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"body\":{\"type\":\"string\"},\"reply_all\":{\"type\":\"boolean\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_draft_reply},
    {"send_email", "Send a template. Preview first; send only when confirm=true.",
     "{\"type\":\"object\",\"properties\":{\"template\":{\"type\":\"string\"},\"attachments\":{\"type\":\"array\",\"items\":{\"type\":\"string\"}},\"confirm\":{\"type\":\"boolean\"},\"account\":{\"type\":\"string\"}},\"required\":[\"template\"]}",
     tool_send_email},
    {"save_draft", "Save a plain-text message template to Drafts without sending. Use draft_reply output; no MML directives or attachments. Folder defaults to the configured drafts alias. Writes immediately.",
     "{\"type\":\"object\",\"properties\":{\"template\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"template\"]}",
     tool_save_draft},
    {"copy_to_clipboard", "Copy text with wl-copy, xclip, or pbcopy.",
     "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"}},\"required\":[\"text\"]}",
     tool_copy_to_clipboard},
    {"list_attachments", "Download and list attachment filenames and sizes. Body parts are omitted. Marks the message Seen.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_list_attachments},
    {"download_attachment", "Download one attachment and return its private temporary path. Marks the message Seen; caller removes downloaded files when finished.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"filename\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\",\"filename\"]}",
     tool_download_attachment},
    {"extract_calendar_event", "Return the first ICS attachment from a message. Marks the message Seen.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_extract_calendar_event},
    {"create_calendar_event", "Write a new local ICS file, not a calendar-service entry. Accepts flexible dates; preview shows resolved UTC times. Reuse those absolute times when confirming. End must be after start. Requires confirm=true.",
     "{\"type\":\"object\",\"properties\":{\"summary\":{\"type\":\"string\"},\"dtstart\":{\"type\":\"string\"},\"dtend\":{\"type\":\"string\"},\"location\":{\"type\":\"string\"},\"description\":{\"type\":\"string\"},\"confirm\":{\"type\":\"boolean\"}},\"required\":[\"summary\",\"dtstart\",\"dtend\"]}",
     tool_create_calendar_event},
    {"list_threads", "List recent envelopes so conversations can be grouped by subject.",
     "{\"type\":\"object\",\"properties\":{\"folder\":{\"type\":\"string\"},\"page_size\":{\"type\":\"integer\"},\"account\":{\"type\":\"string\"}}}",
     tool_list_threads},
    {"read_thread", "List up to 20 matching subject envelopes, not a complete thread. thread_id is a literal subject fragment without quotes or backslashes.",
     "{\"type\":\"object\",\"properties\":{\"thread_id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"thread_id\"]}",
     tool_read_thread},
    {"snooze_email", "Store a local snooze record with a flexible date, normalised to UTC. Bare tomorrow means the same local clock time next day. Does not move mail, schedule a wake-up or notify.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"},\"subject\":{\"type\":\"string\"},\"snoozeUntil\":{\"type\":\"string\"}},\"required\":[\"id\",\"snoozeUntil\"]}",
     tool_snooze_email},
    {"list_snoozed_emails", "List locally snoozed messages.",
     "{\"type\":\"object\",\"properties\":{}}", tool_list_snoozed_emails},
    {"create_reminder", "Store a reminder locally without scheduling notifications. dueDate accepts flexible dates and is stored/reported in UTC; priority is 0-9.",
     "{\"type\":\"object\",\"properties\":{\"title\":{\"type\":\"string\"},\"notes\":{\"type\":\"string\"},\"dueDate\":{\"type\":\"string\"},\"priority\":{\"type\":\"integer\"}},\"required\":[\"title\"]}",
     tool_create_reminder},
    {"health_check", "List accounts and folders to check that himalaya can reach mail.",
     "{\"type\":\"object\",\"properties\":{\"account\":{\"type\":\"string\"}}}", tool_health_check},
};

static const int tool_count = (int)(sizeof tools / sizeof tools[0]);

typedef struct {
  const char *name;
  const char *description;
  const char *text;
} Prompt;

static const Prompt prompts[] = {
    {"triage_inbox", "Classify recent mail as actionable, FYI, or skip.",
     "List recent mail, read the ones that need context, and classify each as actionable, FYI, or skip. Do not send or delete anything."},
    {"summarize_email", "Summarize one message.",
     "Read the message and summarize who it is from, what they want, and any deadline. Do not send a reply."},
    {"daily_email_digest", "Summarize today's inbox.",
     "List today's messages and group them by whether they need a reply, are informational, or can wait."},
    {"weekly_email_digest", "Summarize the week's inbox.",
     "Search the last seven days and summarize what still needs action."},
    {"draft_reply", "Draft a reply without sending.",
     "Read the message and draft a reply. Show the draft. Do not send unless the user explicitly asks to send."},
    {"morning_briefing", "Morning mail briefing.",
     "Check unread count, list snoozed items that are due, and summarize mail that needs attention today."},
    {"inbox_check", "Short unread inbox check.",
     "Report the unread count and the subjects of the newest unread messages."},
};

static void reply(cJSON *id, cJSON *result, cJSON *error) {
  cJSON *msg = cJSON_CreateObject();
  cJSON_AddStringToObject(msg, "jsonrpc", "2.0");
  if (id) cJSON_AddItemToObject(msg, "id", cJSON_Duplicate(id, 1));
  else cJSON_AddNullToObject(msg, "id");
  if (error) cJSON_AddItemToObject(msg, "error", error);
  else cJSON_AddItemToObject(msg, "result", result);
  char *printed = cJSON_PrintUnformatted(msg);
  fputs(printed, stdout);
  fputc('\n', stdout);
  fflush(stdout);
  free(printed);
  cJSON_Delete(msg);
}

static void rpc_error(cJSON *id, int code, const char *message) {
  cJSON *error = cJSON_CreateObject();
  cJSON_AddNumberToObject(error, "code", code);
  cJSON_AddStringToObject(error, "message", message);
  reply(id, NULL, error);
}

static int valid_arguments(const Tool *tool, const cJSON *args) {
  if (args && !cJSON_IsObject(args)) return 0;
  cJSON *schema = cJSON_Parse(tool->schema);
  const cJSON *required = cJSON_GetObjectItemCaseSensitive(schema, "required"), *key;
  int valid = 1;
  cJSON_ArrayForEach(key, required) if (!cJSON_GetObjectItemCaseSensitive(args, key->valuestring)) valid = 0;
  const cJSON *props = cJSON_GetObjectItemCaseSensitive(schema, "properties"), *value;
  cJSON_ArrayForEach(value, args) {
    const cJSON *spec = cJSON_GetObjectItemCaseSensitive(props, value->string);
    const char *type = arg_str(spec, "type");
    if (!type) { valid = 0; continue; }
    if (!strcmp(type, "string") && !cJSON_IsString(value)) valid = 0;
    if (!strcmp(type, "integer") && (!cJSON_IsNumber(value) || value->valuedouble != value->valueint)) valid = 0;
    if (!strcmp(type, "boolean") && !cJSON_IsBool(value)) valid = 0;
    if (!strcmp(type, "array")) {
      if (!cJSON_IsArray(value)) valid = 0;
      const cJSON *item;
      cJSON_ArrayForEach(item, value) if (!cJSON_IsString(item)) valid = 0;
    }
  }
  cJSON_Delete(schema);
  return valid;
}

static void tool_result(cJSON *id, Result r) {
  cJSON *result = cJSON_CreateObject();
  cJSON *content = cJSON_AddArrayToObject(result, "content");
  cJSON *block = cJSON_CreateObject();
  cJSON_AddStringToObject(block, "type", "text");
  cJSON_AddStringToObject(block, "text", r.text ? r.text : "");
  cJSON_AddItemToArray(content, block);
  if (r.is_error) cJSON_AddBoolToObject(result, "isError", 1);
  reply(id, result, NULL);
  result_free(r);
}

static void handle(cJSON *msg) {
  cJSON *id = cJSON_GetObjectItemCaseSensitive(msg, "id");
  cJSON *method_item = cJSON_GetObjectItemCaseSensitive(msg, "method");
  const char *version = arg_str(msg, "jsonrpc");
  if (!cJSON_IsObject(msg) || !version || strcmp(version, "2.0") || !cJSON_IsString(method_item) || (id && !cJSON_IsString(id) && !cJSON_IsNumber(id) && !cJSON_IsNull(id))) {
    rpc_error(NULL, -32600, "invalid JSON-RPC request"); return;
  }
  if (!id) return; /* Notifications must not trigger tools or receive replies. */
  const char *method = method_item->valuestring;
  cJSON *params = cJSON_GetObjectItemCaseSensitive(msg, "params");
  if (params && !cJSON_IsObject(params)) { rpc_error(id, -32602, "params must be an object"); return; }

  if (!strcmp(method, "initialize")) {
    cJSON *result = cJSON_CreateObject();
    const cJSON *pv = params ? cJSON_GetObjectItemCaseSensitive(params, "protocolVersion") : NULL;
    cJSON_AddStringToObject(result, "protocolVersion", cJSON_IsString(pv) ? pv->valuestring : "2024-11-05");
    cJSON *caps = cJSON_AddObjectToObject(result, "capabilities");
    cJSON_AddObjectToObject(caps, "tools");
    cJSON_AddObjectToObject(caps, "prompts");
    cJSON_AddObjectToObject(caps, "resources");
    cJSON *info = cJSON_AddObjectToObject(result, "serverInfo");
    cJSON_AddStringToObject(info, "name", "himalaya-mcp");
    cJSON_AddStringToObject(info, "version", "0.2.1");
    reply(id, result, NULL);
    return;
  }
  if (!strcmp(method, "ping")) {
    reply(id, cJSON_CreateObject(), NULL);
    return;
  }
  if (!strcmp(method, "tools/list")) {
    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(result, "tools");
    for (int i = 0; i < tool_count; i++) {
      cJSON *tool = cJSON_CreateObject();
      cJSON_AddStringToObject(tool, "name", tools[i].name);
      cJSON_AddStringToObject(tool, "description", tools[i].description);
      cJSON *schema = cJSON_Parse(tools[i].schema);
      cJSON *properties = cJSON_GetObjectItemCaseSensitive(schema, "properties");
      const char *date_keys[] = {"dtstart", "dtend", "dueDate", "snoozeUntil"};
      int has_dates = 0;
      for (int j = 0; j < 4; j++) {
        cJSON *property = cJSON_GetObjectItemCaseSensitive(properties, date_keys[j]);
        if (!property) continue;
        has_dates = 1;
        char description[768];
        snprintf(description, sizeof description,
                 "Examples: 2026-10-01T12:00:00Z, 20261001T120000Z, 2026-10-01 12:00, 1 October 2026 at noon, tomorrow at 9am, in 2 hours, 30m, 2h, 1d. Zone-less dates use %s (HIMALAYA_TIMEZONE; default UTC). Z, UTC, GMT and numeric offsets override that zone. Requires a time; rejects slash dates, impossible dates and DST gaps/overlaps without explicit offset. Results are UTC. Durations are elapsed time; tomorrow follows the local calendar.%s",
                 date_default_zone(), j == 3 ? " Bare tomorrow also keeps the current local clock time." : "");
        cJSON_AddStringToObject(property, "description", description);
      }
      if (has_dates) {
        /* Some clients omit property descriptions from tool discovery. */
        char description[1024];
        snprintf(description, sizeof description,
                 "%s Dates: ISO/calendar timestamps, 2026-10-01 12:00, 1 October 2026 at noon, tomorrow at 9am, in 2 hours. Zone-less input uses %s; explicit offsets override it. Rejects ambiguous dates and DST times; returns UTC.",
                 tools[i].description, date_default_zone());
        cJSON_SetValuestring(cJSON_GetObjectItemCaseSensitive(tool, "description"), description);
      }
      cJSON_AddItemToObject(tool, "inputSchema", schema);
      cJSON_AddItemToArray(arr, tool);
    }
    reply(id, result, NULL);
    return;
  }
  if (!strcmp(method, "tools/call")) {
    const cJSON *name = params ? cJSON_GetObjectItemCaseSensitive(params, "name") : NULL;
    const cJSON *args = params ? cJSON_GetObjectItemCaseSensitive(params, "arguments") : NULL;
    if (!cJSON_IsString(name)) {
      rpc_error(id, -32602, "missing tool name");
      return;
    }
    for (int i = 0; i < tool_count; i++) {
      if (!strcmp(tools[i].name, name->valuestring)) {
        if (!valid_arguments(&tools[i], args)) { rpc_error(id, -32602, "arguments do not match tool schema"); return; }
        tool_result(id, tools[i].fn(args));
        return;
      }
    }
    rpc_error(id, -32602, "unknown tool");
    return;
  }
  if (!strcmp(method, "prompts/list")) {
    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(result, "prompts");
    for (size_t i = 0; i < sizeof prompts / sizeof prompts[0]; i++) {
      cJSON *prompt = cJSON_CreateObject();
      cJSON_AddStringToObject(prompt, "name", prompts[i].name);
      cJSON_AddStringToObject(prompt, "description", prompts[i].description);
      cJSON *arguments = cJSON_AddArrayToObject(prompt, "arguments");
      const char *names[] = {"id", "folder", "account", "instructions"};
      for (int j = 0; j < 4; j++) {
        cJSON *argument = cJSON_CreateObject();
        cJSON_AddStringToObject(argument, "name", names[j]);
        cJSON_AddBoolToObject(argument, "required", 0);
        cJSON_AddItemToArray(arguments, argument);
      }
      cJSON_AddItemToArray(arr, prompt);
    }
    reply(id, result, NULL);
    return;
  }
  if (!strcmp(method, "prompts/get")) {
    const cJSON *name = params ? cJSON_GetObjectItemCaseSensitive(params, "name") : NULL;
    if (!cJSON_IsString(name)) {
      rpc_error(id, -32602, "prompt name is required");
      return;
    }
    const cJSON *arguments = cJSON_GetObjectItemCaseSensitive(params, "arguments"), *argument;
    if (arguments && !cJSON_IsObject(arguments)) { rpc_error(id, -32602, "prompt arguments must be an object"); return; }
    cJSON_ArrayForEach(argument, arguments) {
      if (!cJSON_IsString(argument) || (strcmp(argument->string, "id") && strcmp(argument->string, "folder") && strcmp(argument->string, "account") && strcmp(argument->string, "instructions"))) {
        rpc_error(id, -32602, "unknown or non-string prompt argument"); return;
      }
    }
    for (size_t i = 0; i < sizeof prompts / sizeof prompts[0]; i++) {
      if (strcmp(prompts[i].name, name->valuestring)) continue;
      cJSON *result = cJSON_CreateObject();
      cJSON_AddStringToObject(result, "description", prompts[i].description);
      cJSON *messages = cJSON_AddArrayToObject(result, "messages");
      cJSON *message = cJSON_CreateObject();
      cJSON_AddStringToObject(message, "role", "user");
      cJSON *content = cJSON_AddObjectToObject(message, "content");
      cJSON_AddStringToObject(content, "type", "text");
      char *context = arguments ? cJSON_PrintUnformatted(arguments) : strdup("{}");
      char *text = malloc(strlen(prompts[i].text) + strlen(context) + 64);
      sprintf(text, "%s\n\nUser-supplied context:\n%s", prompts[i].text, context);
      cJSON_AddStringToObject(content, "text", text);
      free(text); free(context);
      cJSON_AddItemToArray(messages, message);
      reply(id, result, NULL);
      return;
    }
    rpc_error(id, -32602, "unknown prompt");
    return;
  }
  if (!strcmp(method, "resources/templates/list")) {
    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(result, "resourceTemplates");
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "uriTemplate", "email://message/{id}");
    cJSON_AddStringToObject(item, "name", "Message in the default account inbox");
    cJSON_AddStringToObject(item, "mimeType", "text/plain");
    cJSON_AddItemToArray(arr, item);
    reply(id, result, NULL); return;
  }
  if (!strcmp(method, "resources/list")) {
    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(result, "resources");
    const char *uris[] = {"email://inbox", "email://folders"};
    for (int i = 0; i < 2; i++) {
      cJSON *resource = cJSON_CreateObject();
      cJSON_AddStringToObject(resource, "uri", uris[i]);
      cJSON_AddStringToObject(resource, "name", uris[i]);
      cJSON_AddStringToObject(resource, "mimeType", "text/plain");
      cJSON_AddItemToArray(arr, resource);
    }
    reply(id, result, NULL);
    return;
  }
  if (!strcmp(method, "resources/read")) {
    const cJSON *uri = params ? cJSON_GetObjectItemCaseSensitive(params, "uri") : NULL;
    if (!cJSON_IsString(uri)) {
      rpc_error(id, -32602, "resource URI is required");
      return;
    }
    Result body = {NULL, 1};
    if (!strcmp(uri->valuestring, "email://inbox")) body = tool_list_emails(NULL);
    else if (!strcmp(uri->valuestring, "email://folders")) body = tool_list_folders(NULL);
    else if (!strncmp(uri->valuestring, "email://message/", 16)) {
      cJSON *args = cJSON_CreateObject();
      cJSON_AddStringToObject(args, "id", uri->valuestring + 16);
      body = tool_read_email(args);
      cJSON_Delete(args);
    }
    if (body.is_error) { rpc_error(id, -32002, body.text ? body.text : "unknown resource"); result_free(body); return; }
    cJSON *result = cJSON_CreateObject();
    cJSON *contents = cJSON_AddArrayToObject(result, "contents");
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "uri", uri->valuestring);
    cJSON_AddStringToObject(item, "mimeType", "text/plain");
    cJSON_AddStringToObject(item, "text", body.text ? body.text : "");
    cJSON_AddItemToArray(contents, item);
    reply(id, result, NULL);
    result_free(body);
    return;
  }
  if (id) {
    cJSON *error = cJSON_CreateObject();
    cJSON_AddNumberToObject(error, "code", -32601);
    cJSON_AddStringToObject(error, "message", "method not found");
    reply(id, NULL, error);
  }
}

static void serve(void) {
  char *line = NULL;
  size_t cap = 0;
  while (getline(&line, &cap, stdin) != -1) {
    const char *end = NULL;
    cJSON *msg = cJSON_ParseWithOpts(line, &end, 1);
    if (msg) {
      handle(msg);
      cJSON_Delete(msg);
    } else rpc_error(NULL, -32700, "invalid JSON");
  }
  free(line);
}

int main(int argc, char **argv) {
  setenv("EDITOR", "true", 0);
  setenv("VISUAL", "true", 0);
  if (argc > 1 && !strcmp(argv[1], "doctor")) {
    Result health = tool_health_check(NULL);
    fputs(health.text ? health.text : "", stdout);
    int rc = health.is_error;
    result_free(health);
    return rc;
  }
  serve();
  return 0;
}
