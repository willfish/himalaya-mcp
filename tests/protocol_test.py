#!/usr/bin/env python3
"""Offline protocol test. Uses a fake himalaya binary."""

import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    binary = Path(sys.argv[1]).resolve()
    root = Path(tempfile.mkdtemp(prefix="himalaya-mcp-test-"))
    fake = root / "himalaya"
    log = root / "argv.log"
    fake.write_text(
        "#!/bin/sh\n"
        f"printf '%s\\n' \"$*\" >> {log}\n"
        "case \"$*\" in\n"
        "  *'envelope list'*) printf '%s\\n' "
        "'[{\"id\":\"7\",\"flags\":[],\"subject\":\"Hello\",\"from\":{\"name\":\"A\",\"addr\":\"a@example.com\"},\"to\":{\"name\":\"\",\"addr\":\"me@example.com\"},\"date\":\"2026-09-25\",\"has_attachment\":false}]'\n"
        "    ;;\n"
        "  *'template send'*) exit 0 ;;\n"
        "  *'account list'*) printf '%s\\n' '[{\"name\":\"gmail\",\"default\":true}]' ;;\n"
        "  *'folder list'*) printf '%s\\n' '[{\"name\":\"INBOX\"}]' ;;\n"
        "  *) printf '%s\\n' '{}' ;;\n"
        "esac\n"
    )
    fake.chmod(0o755)
    env = os.environ.copy()
    env["HIMALAYA_BINARY"] = str(fake)
    env["XDG_STATE_HOME"] = str(root / "state")
    proc = subprocess.Popen(
        [str(binary)],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        text=True,
        env=env,
    )

    def call(payload):
        proc.stdin.write(json.dumps(payload) + "\n")
        proc.stdin.flush()
        return json.loads(proc.stdout.readline())

    init = call({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}})
    assert init["result"]["serverInfo"]["name"] == "himalaya-mcp"
    listed = call({"jsonrpc": "2.0", "id": 2, "method": "tools/list"})
    names = [tool["name"] for tool in listed["result"]["tools"]]
    assert len(names) == 29, len(names)
    prompts = call({"jsonrpc": "2.0", "id": 3, "method": "prompts/list"})
    assert len(prompts["result"]["prompts"]) == 7
    resources = call({"jsonrpc": "2.0", "id": 4, "method": "resources/list"})
    assert len(resources["result"]["resources"]) == 3
    emails = call({
        "jsonrpc": "2.0", "id": 5, "method": "tools/call",
        "params": {"name": "list_emails", "arguments": {"account": "gmail"}},
    })
    assert "Hello" in emails["result"]["content"][0]["text"]
    preview = call({
        "jsonrpc": "2.0", "id": 6, "method": "tools/call",
        "params": {"name": "send_email", "arguments": {"template": "To: a@example.com\n\nHi\n"}},
    })
    assert "PREVIEW" in preview["result"]["content"][0]["text"]
    assert "template send" not in log.read_text()
    sent = call({
        "jsonrpc": "2.0", "id": 7, "method": "tools/call",
        "params": {"name": "send_email", "arguments": {
            "template": "To: a@example.com\n\nHi\n", "confirm": True,
        }},
    })
    assert sent["result"]["content"][0]["text"].strip() == "sent"
    assert "template send" in log.read_text()
    proc.stdin.close()
    proc.wait(timeout=5)
    print("ok", len(names), "tools")


if __name__ == "__main__":
    main()
