# Tracing rand() calls (IWD2R_RAND_TRACE)

`globals.ai_rng_state` in a replay dump says *that* the two engines drew a
different number of `rand()` calls, not *where*. With `IWD2R_RAND_TRACE=1` in
the environment both sides log every call:

```sh
IWD2R_RAND_TRACE=1 python3 tools/oracle.py replay compare scenarios/pending/world_entry.txt --seed 1 --dump-every 1
```

- **The original** (`oracle/replay/recorder.cpp`, `HookRand`): `rand` (0x7E8160)
  begins with a 5-byte call to `_getptd`; the recorder turns it into a jmp to a
  trampoline that logs and makes the call itself. `replay/rand.txt` gets one
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
