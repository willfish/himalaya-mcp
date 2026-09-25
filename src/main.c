#include "tools.h"

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
    {"get_unread_count", "Count unread messages in a folder.",
     "{\"type\":\"object\",\"properties\":{\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}}}",
     tool_get_unread_count},
    {"list_starred", "List flagged messages.",
     "{\"type\":\"object\",\"properties\":{\"folder\":{\"type\":\"string\"},\"page_size\":{\"type\":\"integer\"},\"account\":{\"type\":\"string\"}}}",
     tool_list_starred},
    {"read_email", "Read a message as text without marking it seen.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_read_email},
    {"read_email_html", "Read the HTML part of a message.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_read_email_html},
    {"read_email_raw", "Read the raw MIME source of a message.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_read_email_raw},
    {"render_email", "Read a message as plain text, stripping HTML when needed.",
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
    {"list_attachments", "List attachment filenames and sizes. Body parts are omitted.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_list_attachments},
    {"download_attachment", "Download one attachment and return its path.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"filename\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\",\"filename\"]}",
     tool_download_attachment},
    {"extract_calendar_event", "Return the first ICS attachment from a message.",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"id\"]}",
     tool_extract_calendar_event},
    {"create_calendar_event", "Write an ICS event. Requires confirm=true.",
     "{\"type\":\"object\",\"properties\":{\"summary\":{\"type\":\"string\"},\"dtstart\":{\"type\":\"string\"},\"dtend\":{\"type\":\"string\"},\"location\":{\"type\":\"string\"},\"description\":{\"type\":\"string\"},\"confirm\":{\"type\":\"boolean\"}},\"required\":[\"summary\",\"dtstart\",\"dtend\"]}",
     tool_create_calendar_event},
    {"list_threads", "List recent envelopes so conversations can be grouped by subject.",
     "{\"type\":\"object\",\"properties\":{\"folder\":{\"type\":\"string\"},\"page_size\":{\"type\":\"integer\"},\"account\":{\"type\":\"string\"}}}",
     tool_list_threads},
    {"read_thread", "List messages whose subject matches a thread id.",
     "{\"type\":\"object\",\"properties\":{\"thread_id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"}},\"required\":[\"thread_id\"]}",
     tool_read_thread},
    {"snooze_email", "Remember a message until snoozeUntil (ISO time, tomorrow, Nh, or Nd).",
     "{\"type\":\"object\",\"properties\":{\"id\":{\"type\":\"string\"},\"folder\":{\"type\":\"string\"},\"account\":{\"type\":\"string\"},\"subject\":{\"type\":\"string\"},\"snoozeUntil\":{\"type\":\"string\"}},\"required\":[\"id\",\"snoozeUntil\"]}",
     tool_snooze_email},
    {"list_snoozed_emails", "List locally snoozed messages.",
     "{\"type\":\"object\",\"properties\":{}}", tool_list_snoozed_emails},
    {"create_reminder", "Store a reminder locally.",
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
  if (!cJSON_IsString(method_item)) return;
  const char *method = method_item->valuestring;
  cJSON *params = cJSON_GetObjectItemCaseSensitive(msg, "params");
  if (!id && strncmp(method, "notifications/", 14) == 0) return;
  if (!id && strcmp(method, "initialized") == 0) return;

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
    cJSON_AddStringToObject(info, "version", "0.1.0");
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
      cJSON_AddItemToObject(tool, "inputSchema", cJSON_Parse(tools[i].schema));
      cJSON_AddItemToArray(arr, tool);
    }
    reply(id, result, NULL);
    return;
  }
  if (!strcmp(method, "tools/call")) {
    const cJSON *name = params ? cJSON_GetObjectItemCaseSensitive(params, "name") : NULL;
    const cJSON *args = params ? cJSON_GetObjectItemCaseSensitive(params, "arguments") : NULL;
    if (!cJSON_IsString(name)) {
      tool_result(id, result_err("missing tool name"));
      return;
    }
    for (int i = 0; i < tool_count; i++) {
      if (!strcmp(tools[i].name, name->valuestring)) {
        tool_result(id, tools[i].fn(cJSON_IsObject(args) ? args : NULL));
        return;
      }
    }
    tool_result(id, result_err("unknown tool"));
    return;
  }
  if (!strcmp(method, "prompts/list")) {
    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(result, "prompts");
    for (size_t i = 0; i < sizeof prompts / sizeof prompts[0]; i++) {
      cJSON *prompt = cJSON_CreateObject();
      cJSON_AddStringToObject(prompt, "name", prompts[i].name);
      cJSON_AddStringToObject(prompt, "description", prompts[i].description);
      cJSON_AddItemToArray(arr, prompt);
    }
    reply(id, result, NULL);
    return;
  }
  if (!strcmp(method, "prompts/get")) {
    const cJSON *name = params ? cJSON_GetObjectItemCaseSensitive(params, "name") : NULL;
    if (!cJSON_IsString(name)) {
      reply(id, NULL, cJSON_CreateObject());
      return;
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
      cJSON_AddStringToObject(content, "text", prompts[i].text);
      cJSON_AddItemToArray(messages, message);
      reply(id, result, NULL);
      return;
    }
    reply(id, NULL, cJSON_CreateObject());
    return;
  }
  if (!strcmp(method, "resources/list")) {
    cJSON *result = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(result, "resources");
    const char *uris[] = {"email://inbox", "email://folders", "email://message/{id}"};
    for (int i = 0; i < 3; i++) {
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
      reply(id, NULL, cJSON_CreateObject());
      return;
    }
    Result body = result_err("unknown resource");
    if (!strcmp(uri->valuestring, "email://inbox")) body = tool_list_emails(NULL);
    else if (!strcmp(uri->valuestring, "email://folders")) body = tool_list_folders(NULL);
    else if (!strncmp(uri->valuestring, "email://message/", 16)) {
      cJSON *args = cJSON_CreateObject();
      cJSON_AddStringToObject(args, "id", uri->valuestring + 16);
      body = tool_read_email(args);
      cJSON_Delete(args);
    }
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
    cJSON *msg = cJSON_Parse(line);
    if (msg) {
      handle(msg);
      cJSON_Delete(msg);
    }
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
