#pragma once
#include "common.h"

Result tool_list_emails(const cJSON *args);
Result tool_search_emails(const cJSON *args);
Result tool_get_unread_count(const cJSON *args);
Result tool_list_starred(const cJSON *args);
Result tool_read_email(const cJSON *args);
Result tool_read_email_html(const cJSON *args);
Result tool_read_email_raw(const cJSON *args);
Result tool_render_email(const cJSON *args);
Result tool_flag_email(const cJSON *args);
Result tool_move_email(const cJSON *args);
Result tool_list_folders(const cJSON *args);
Result tool_create_folder(const cJSON *args);
Result tool_delete_folder(const cJSON *args);
Result tool_compose_email(const cJSON *args);
Result tool_export_to_markdown(const cJSON *args);
Result tool_create_action_item(const cJSON *args);
Result tool_draft_reply(const cJSON *args);
Result tool_send_email(const cJSON *args);
Result tool_save_draft(const cJSON *args);
Result tool_copy_to_clipboard(const cJSON *args);
Result tool_list_attachments(const cJSON *args);
Result tool_download_attachment(const cJSON *args);
Result tool_extract_calendar_event(const cJSON *args);
Result tool_create_calendar_event(const cJSON *args);
Result tool_list_threads(const cJSON *args);
Result tool_read_thread(const cJSON *args);
Result tool_snooze_email(const cJSON *args);
Result tool_list_snoozed_emails(const cJSON *args);
Result tool_create_reminder(const cJSON *args);
Result tool_health_check(const cJSON *args);
