#!/usr/bin/env python3
"""token_audit.summarize -- where a session's context actually went.

Adopted from WillScarlettOhara/iwd2-re@e8dd16a scripts/token_audit.py:
his accounting rules are kept -- count usage only on assistant events
that carry a ``usage`` dict, attribute each tool_result to its tool by
joining the tool_use block's id, and judge result weight by characters
(1 token ~ 4 chars).  His CLI printed a watchlist against his own
habits (py heredocs, raw ghidra-bridge decompiles, grep chains); ours
returns a plain dict so the report itself is the caller's.

``summarize(transcript_jsonl)`` returns::

    {"usage":   {input_tokens, output_tokens, cache_creation_input_tokens,
                 cache_read_input_tokens, messages},
     "tools":   {name: {"calls", "result_chars", "result_tokens"}},
     "largest": [{"tool", "chars", "result_tokens"}] x <=10, chars desc}

Defensive by design: a transcript can end mid-line (crashed session), so
unparseable lines are skipped, tool_results whose tool_use id never
appeared are bucketed as ``?``, and content may be a string, a block
list, or missing.  Read-only.

Usage::

    python3 tools/token_audit.py SESSION.jsonl [MORE.jsonl ...]
"""
from __future__ import annotations

import argparse
import json
import os
import sys

USAGE_KEYS = ("input_tokens", "output_tokens",
              "cache_creation_input_tokens", "cache_read_input_tokens")
LARGEST_CAP = 10
CHARS_PER_TOKEN = 4


def _content_len(content) -> int:
    """Characters of a message content: a string, a block list, or None."""
    if isinstance(content, str):
        return len(content)
    if isinstance(content, list):
        return sum(len(b.get("text", "") or "") for b in content
                   if isinstance(b, dict))
    return 0


def summarize(transcript_jsonl: str) -> dict:
    """Token totals, per-tool result weight, and the largest single
    tool results, from one Claude Code session transcript (.jsonl)."""
    usage = {k: 0 for k in USAGE_KEYS}
    usage["messages"] = 0
    tools: dict = {}
    id2tool: dict = {}
    results = []

    with open(transcript_jsonl, encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                event = json.loads(line)
            except ValueError:
                continue
            if not isinstance(event, dict):
                continue
            msg = event.get("message")
            if not isinstance(msg, dict):
                msg = {}
            etype = event.get("type")

            if etype == "assistant":
                u = msg.get("usage")
                if isinstance(u, dict):
                    usage["messages"] += 1
                    for k in USAGE_KEYS:
                        usage[k] += u.get(k) or 0
                for block in msg.get("content") or []:
                    if not isinstance(block, dict):
                        continue
                    if block.get("type") == "tool_use":
                        name = block.get("name") or "?"
                        id2tool[block.get("id")] = name
                        tools.setdefault(
                            name, {"calls": 0, "result_chars": 0,
                                   "result_tokens": 0})["calls"] += 1

            elif etype == "user":
                content = msg.get("content")
                if not isinstance(content, list):
                    continue
                for block in content:
                    if not isinstance(block, dict):
                        continue
                    if block.get("type") == "tool_result":
                        name = id2tool.get(block.get("tool_use_id"), "?")
                        chars = _content_len(block.get("content"))
                        row = tools.setdefault(
                            name, {"calls": 0, "result_chars": 0,
                                   "result_tokens": 0})
                        row["result_chars"] += chars
                        row["result_tokens"] = row["result_chars"] // CHARS_PER_TOKEN
                        results.append((chars, name))

    results.sort(key=lambda r: -r[0])
    largest = [{"tool": name, "chars": chars,
                "result_tokens": chars // CHARS_PER_TOKEN}
               for chars, name in results[:LARGEST_CAP]]
    return {"usage": usage, "tools": tools, "largest": largest}


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("transcripts", nargs="+", help="session .jsonl file(s)")
    args = ap.parse_args(argv)
    merged = {"usage": {}, "tools": {}, "largest": []}
    all_results = []
    for path in args.transcripts:
        s = summarize(path)
        print(f"== {os.path.basename(path)}")
        u = s["usage"]
        n = u["messages"] or 1
        print(f"  messages with usage: {u['messages']}  "
              f"input {u['input_tokens']}  output {u['output_tokens']}  "
              f"cache write {u['cache_creation_input_tokens']}  "
              f"cache read {u['cache_read_input_tokens']}  "
              f"(~{u['cache_read_input_tokens'] // n} read/message)")
        for name, row in sorted(s["tools"].items(),
                                key=lambda kv: -kv[1]["result_tokens"]):
            print(f"  {name:<22} calls {row['calls']:<6} "
                  f"results {row['result_chars'] / 1e6:.1f}M chars "
                  f"(~{row['result_tokens'] // 1000}K tokens)")
        for r in s["largest"][:10]:
            print(f"    {r['chars']:>9,} chars  {r['tool']}")
        for k in USAGE_KEYS:
            merged["usage"][k] = merged["usage"].get(k, 0) + u[k]
        merged["usage"]["messages"] = merged["usage"].get("messages", 0) + u["messages"]
        for name, row in s["tools"].items():
            dst = merged["tools"].setdefault(
                name, {"calls": 0, "result_chars": 0, "result_tokens": 0})
            dst["calls"] += row["calls"]
            dst["result_chars"] += row["result_chars"]
            dst["result_tokens"] = dst["result_chars"] // CHARS_PER_TOKEN
        all_results.extend((r["chars"], r["tool"]) for r in s["largest"])
    if len(args.transcripts) > 1:
        all_results.sort(key=lambda r: -r[0])
        merged["largest"] = [{"tool": name, "chars": c,
                              "result_tokens": c // CHARS_PER_TOKEN}
                             for c, name in all_results[:LARGEST_CAP]]
        print("== merged")
        for r in merged["largest"]:
            print(f"    {r['chars']:>9,} chars  {r['tool']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
