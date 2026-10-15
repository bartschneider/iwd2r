# Tracing rand() calls (IWD2R_RAND_TRACE)

`globals.ai_rng_state` in a replay dump says *that* the two engines drew a
different number of `rand()` calls, not *where*. With `IWD2R_RAND_TRACE=1` in
the environment both sides log every call:

```sh
IWD2R_RAND_TRACE=1 python3 tools/oracle.py replay compare scenarios/pending/world_entry.txt --seed 1 --dump-every 1
```

- **The original** (`oracle/replay/recorder.cpp`, `HookRand`): `rand` (0x7E8160)
  begins with a 5-byte call to `_getptd`; the recorder turns it into a jmp to a
  trampoline that logs and makes the call itself. ``runs/<stamp>/original/rand.txt` (after `replay compare`; the record output folder after `replay record`)` gets one
  line per call, `frame thread caller`: the frame number, the Windows thread
  id and the caller's return address (hex, a vanilla address; the function is
  the nearest `// 0xADDR` comment at or below it in `src/`, or
  `tools/oracle.py evidence` on its start).
- **Ours** (`compat/msvc_rand.cpp`): `ours/rand.txt` (in `IWD2R_STATE_DUMP`,
  else the working folder) gets `frame thread-state state caller`: the frame,
  the address of the calling thread's generator state (one per thread), the
  state after the call, and the caller's offset in the executable
  (`addr2line -f -C -e build/linux-gcc/iwd2r 0x<offset>`; on MSVC the absolute
  return address).

The AI thread is the one whose state passes through a dump's `ai_rng_state`
(`awk '$3 == <ai_rng_state>' ours/rand.txt`); on the original's side it is the
thread with the most calls in a world frame. Both logs number frames the same
way (the replay frame), so a frame's calls can be compared site by site; the
first call whose function differs is where the streams part.

Off by default: the trace costs a file write per call.

With `IWD2R_RAND_TRACE=2` the original's lines get nine more columns, for
calls made through a helper (most go through `CUtil::UtilRandInt` 0x780C80,
which is useless as a caller): the stack dword just above the return address
(for a caller that calls `rand` first, its own return address), then the
first eight dwords further up the stack (scanning up to 512) that lie in the
exe's code range 0x401000-0x7FFFFF, each as `address@slot` (the slot in
dwords up from the return address; `00000000@0` when fewer are found). Those eight
are a heuristic: a stack slot can hold a stale return address from an
earlier call, so check that the instruction before a candidate is a call to
the function below it before trusting it. Our side's lines are unchanged.

Notes: the recorder flushes its buffered calls at the start of each frame, so the calls of the last frame before the original exits are not written. On our side, with the trace on outside replay mode (no `IWD2R_STATE_DUMP`), `rand.txt` is written to the working folder, which is the game folder once the engine has changed into it.

## The state probe (IWD2R_PROBE)

The trace shows where the streams part; the probe shows which state made
them part. With `IWD2R_PROBE=FIRST-LAST` (or one frame number) both engines
append the same text block to `probe.txt` at each of those frames, at the
frame's start (before its AI update, where the dumps are taken), so

```sh
IWD2R_PROBE=1492-1499 IWD2R_RAND_TRACE=1 python3 tools/oracle.py replay compare scenarios/pending/walk_and_talk.txt --seed 1
diff runs/<stamp>/original/probe.txt runs/<stamp>/ours/probe.txt
```

names the first state difference. The block (format in
`platform/replay_probe.h`, which formats both sides):

- `game`: the party count, the area on view, the party slots
  (`m_characters`, what Player1..6 resolve through) and the portraits;
- `area`: the area on view's resref, character count, viewport and its
  visibility map's size, range and character ids;
- `obj`: every live object of the object array, by index: id, type,
  position, whether it is in the area on view, list, `m_canBeSeen`, AI speed
  and inhibitor; a door's or sprite's script name; for a sprite also
  `m_lastCharacterCount`, the active flags, `m_bVisibleMonster`,
  `field_294` (hidden), the general state, the current action, the sequence,
  the action queue (length and head), the pending triggers, the script
  timers and the seven script levels (resref and block count);
- `vis`: the visibility map, one hex row per map row.

The recorder reads these through `fields.json` (schema.json's `x-fields`,
`tools/gen_fields.py`); `src/StateDumpSources.cpp` reads them from our
classes. Off by default; a frame costs a few tens of KB. To add a value,
add its x-field, a member of `replay_probe::Object` (or `Frame`), and the
two reads.

At the time of writing the positions of static objects (type 17) and of
doors (type 33) differ between the engines (doors by one pixel, statics by
up to about 80; not investigated yet); filter them with
`grep -v 'type 17 \|type 33 '` when they are not the subject.
