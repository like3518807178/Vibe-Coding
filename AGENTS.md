# TinyIM agent instructions

## Project goal
This repo implements TinyIM step by step.

## Coding constraints
- Use C++17.
- Keep changes minimal and focused.
- Do not rewrite unrelated modules.
- Keep net / protocol / service / storage responsibilities separated.
- Prefer small, reviewable edits.

## Output requirements
After each task, explain:
1. changed files
2. design decisions
3. how to test
4. remaining risks

## Current protocol baseline
- 4-byte big-endian length prefix + JSON body
- Never assume one read() == one message
- For nonblocking sockets, read/write until EAGAIN or EWOULDBLOCK