# Current task: V2 framing

## Goal
Implement 4-byte big-endian length-prefixed JSON framing.

## Required changes
- Add protocol/FramingCodec
- Add input buffer to Connection
- Read into buffer first, then parse frames
- Reject oversized frames safely

## Acceptance
- Half-packets can be reassembled
- Two concatenated frames can both be parsed
- Invalid length is rejected without crashinggit add .
git commit -m "checkpoint before codex v0"