#!/usr/bin/env python3
"""Run a Frida hook table against a game binary, unattended, with a verdict.

Adopted from the WSO fork's scripts/frida_orig.py (see docs/THIRD_PARTY.md):
the Trace/verdict shape, the crash guard and the hook-table wiring are his.
What is dropped on purpose:

  - His in-target load driver (cursor calibration, movie skip, the click
    driver).  Here the scenario is driven the house way: recorder.dll reads
    IWD2R_SCRIPT in the original, and our build runs the same scenario
    in-engine via RunScenario.  One driving mechanism, both sides.

  - His PROCESS-EXITED verdict (1).  A scenario ends with `quit`, so the
    process exiting after the world screen came up is what success looks
    like -- detaching is not a failure.  Dying before `loaded` still is:
    that is NOT-LOADED, same as his.

Verdicts (exit codes): CLEAN 0, CRASH 1, NOT-LOADED / NOT-EXERCISED /
OBSERVER-ERROR 2 -- the last three all mean *ran, proved nothing*, the same
discipline as smoke's --hit, so a green run cannot be a false green.

The status file explains itself: a script error (OBSERVER-ERROR) writes an
`ERROR script: <description> (<file>:<line>)` line there, not only into the
trace.  A hook site Frida refuses (hooks.py: {tag: 'unhooked'}, e.g. a body
too short for the 5-byte jump) writes `UNHOOKED <name> at <addr>: <error>`,
gets no HITS line (its counter never ran) and is not an observer error: the
other hooks were installed and prove what they prove; the refused row is
unmeasured (truth.frida_verdicts reads the event from the trace).  A refused
`loaded` observation never fires, so the run reads NOT-LOADED.

Two ways to find the target:

  orig.py --exe C:\\path\\to\\game.exe --env K=V ...      spawn it suspended,
                                                          hook, resume
  orig.py --attach IWD2.exe --hooks probe.json ...        poll for a running
                                                          process and hook it

The "loaded" signal is CScreenWorld::EngineActivated, observed as an ordinary
hook named `loaded` so a run only counts as loaded when the world screen
really came up.  The default site is the original's absolute address
(0x6869C0, ImageBase 0x400000, no ASLR); for our build pass --loaded-rva
instead (our addresses move with every rebuild) or --no-loaded to drop the
observation.  A table that already carries a `loaded` hook is left alone.

Fork session gotchas kept: no stdin; stdout redirection is unreliable, so
every status line is flushed + fsynced; a stuck driver blocks the next
attach; a silent no-fire is usually not the path -- 8 bytes are dumped at
every site on install (tag `sites`) so it can be diffed, not guessed at.
"""
import argparse
import json
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import hooks  # noqa: E402  (the renderer this file is staged with)

WORLD_ACTIVATED = 0x6869C0   # CScreenWorld::EngineActivated -- the "loaded" signal

# Lifted from the fork's frida_crash_guard.py via his frida_orig.py: same EBP
# walk, same 'system' pass-through so an OutputDebugString exception does not
# read as a fault, same `return false` so the crash stays faithful.
CRASH_JS = """
Process.setExceptionHandler(function (d) {
  if (d.type === 'system') return false;
  const ctx = d.context;
  send({ tag: 'crash', type: d.type, address: d.address.toString(),
         memory: d.memory ? { operation: d.memory.operation,
                              address: d.memory.address.toString() } : null,
         eip: ctx.eip.toString(), frames: guard(() => ebpwalk(ctx.ebp, 24)) });
  return false;
});
"""


def build_js(spec: dict, require_loaded: bool = True) -> str:
    """PRELUDE + module bases + crash guard + the table + self-checks + ready.
    require_loaded False (--no-loaded) installs no `loaded` hook at all: its
    default site is an address of the original's image, which in our exe
    would land inside our own code."""
    all_hooks = list(spec["hooks"])
    if require_loaded and not any(h.get("name") == "loaded" for h in all_hooks):
        all_hooks.append(loaded_hook())
    blocks = ["'use strict';", hooks.PRELUDE]
    bases = hooks.base_prelude(all_hooks)
    if bases:
        blocks.append(bases)
    blocks.append(CRASH_JS)
    blocks.append(hooks.hooks_js(all_hooks, bool(spec.get("count_only"))))
    blocks.append(hooks.install_dump_js(all_hooks))
    blocks.append(hooks.hit_counts_js(all_hooks))
    blocks.append("send({ tag: 'ready', hooks: %d });" % len(all_hooks))
    return "\n".join(blocks)


def loaded_hook() -> dict:
    """The world-activated observation, expressed in the table schema."""
    if LOADED_RVA is not None:
        return {"rva": LOADED_RVA, "module": LOADED_MODULE, "name": "loaded",
                "conv": "thiscall", "max": 1}
    return {"addr": hex(LOADED_ADDR), "name": "loaded", "conv": "thiscall", "max": 1}


LOADED_ADDR = WORLD_ACTIVATED
LOADED_RVA = None
LOADED_MODULE = "iwd2r.exe"


class Trace:
    def __init__(self, out: Path, status: Path):
        self.out = open(out, "w", encoding="utf-8", errors="replace")
        self.status = open(status, "w", encoding="utf-8", errors="replace")
        self.loaded = False
        self.loaded_detail = ""
        self.crash = None
        self.detached = False
        self.detach_reason = ""
        self.errors = []
        self.unhooked = {}       # hook name -> Frida's refusal text
        self.counts = {}
        self.final = False       # the final HITS message (sent at process exit) arrived
        self.script = None

    def _write(self, fp, line: str) -> None:
        if fp.closed:
            return
        fp.write(line + "\n")
        fp.flush()
        os.fsync(fp.fileno())   # a crash is the interesting case

    def record(self, payload) -> None:
        self._write(self.out, json.dumps(payload))

    def say(self, line: str) -> None:
        """A status line -- this is what the host polls, so it goes to both."""
        self._write(self.status, line)
        try:
            print(line, flush=True)
        except OSError:
            pass     # gotcha 2: stdout redirection can vanish under the wrapper

    def on_message(self, message, data) -> None:
        if message["type"] != "send":
            self.errors.append(message)
            self.record({"tag": "ERROR", "message": message})
            where = ""
            if message.get("fileName"):
                where = " (%s:%s)" % (message["fileName"], message.get("lineNumber", "?"))
            text = str(message.get("description") or message.get("stack")
                       or json.dumps(message, sort_keys=True))
            self.say("ERROR script: %s%s" % (text.splitlines()[0] if text else "?", where))
            return
        payload = message["payload"]
        tag = payload.get("tag")
        if tag == "HITS":
            if self.final:
                return
            self.counts = payload.get("counts", {})
            if payload.get("final"):
                self.final = True
                try:
                    self.script.post({"type": "HITSACK"})   # lets the exiting process go on
                except Exception:
                    pass
            return          # the timer fires every 2s; only the totals matter
        self.record(payload)
        if tag == "unhooked":
            name = str(payload.get("name"))
            self.unhooked[name] = str(payload.get("error", ""))
            self.say("UNHOOKED %s at %s: %s" % (name, payload.get("addr"),
                                               self.unhooked[name]))
        elif tag == "loaded" and not self.loaded:
            self.loaded = True
            self.loaded_detail = payload.get("detail", "")
            self.say("loaded: %s" % (self.loaded_detail or "world screen up"))
        elif tag == "crash" and self.crash is None:
            self.crash = payload

    def on_detached(self, reason, *rest) -> None:
        self.detached = True
        self.detach_reason = str(reason)

    def finish(self, hit: str, hit_min: int, require_loaded: bool = True) -> int:
        for name, n in sorted(self.counts.items()):
            if name not in self.unhooked:     # a refused hook counted nothing
                self.say("HITS %s %d" % (name, n))
        self.say("HITS-FINAL %s" % ("yes" if self.final else "no"))

        if self.crash is not None:
            c = self.crash
            self.say("EXCEPTION %s @ %s eip=%s" % (c.get("type"), c.get("address"),
                                                   c.get("eip")))
            for frame in c.get("frames") or []:
                self.say("  frame %s" % frame)
            verdict, rc = "CRASH", 1
        elif self.errors:
            verdict, rc = "OBSERVER-ERROR", 2
        elif require_loaded and not self.loaded:
            # Covers the process dying first too: detached before `loaded` is
            # still a run that never got where it was asked to go.
            verdict, rc = "NOT-LOADED", 2
        elif hit and self.counts.get(hit, 0) < hit_min:
            verdict, rc = "NOT-EXERCISED", 2
        else:
            verdict, rc = "CLEAN", 0

        note = ""
        if hit:
            note = "  (%s hit x%d)" % (hit, self.counts.get(hit, 0))
        elif verdict == "CLEAN" and not self.counts:
            note = "  (no hits -- nothing proved the path ran)"
        if self.detached and self.detach_reason:
            note += "  [detached: %s]" % self.detach_reason
        self.say("RESULT: %s%s" % (verdict, note))
        self.out.close()
        self.status.close()
        return rc


def attach_when_there(name: str, deadline: float):
    """Poll for the process -- the replay driver may take a while to boot it.

    Returns (session, pid): the status line wants the pid, and Session does
    not carry it.

    get_process_by_name raises InvalidArgumentError("process not found")
    on frida 17.24.1 -- measured on the smoke venv -- where older releases
    raised ProcessNotFoundError, so both mean "not there yet"."""
    import frida
    device = frida.get_local_device()
    while True:
        try:
            proc = device.get_process_by_name(name)
            return device.attach(proc.pid), proc.pid
        except (frida.InvalidArgumentError, frida.ProcessNotFoundError):
            if time.monotonic() >= deadline:
                raise
            time.sleep(0.5)


def build_parser() -> argparse.ArgumentParser:
    """The command line, importable without frida (probe.py's tests parse
    every generated sidecar command with it)."""
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--hooks", required=True, help="hook spec JSON (schema: hooks.py)")
    ap.add_argument("--out", default="trace.jsonl")
    ap.add_argument("--status", help="verdict file (default: <out>.status)")
    ap.add_argument("--exe", help="spawn this and hook it")
    ap.add_argument("--attach", metavar="NAME", help="hook a running process by name")
    ap.add_argument("--attach-timeout", type=float, default=90.0,
                    help="seconds to keep polling --attach for (default 90)")
    ap.add_argument("--env", action="append", default=[], metavar="K=V",
                    help="child environment override (spawn mode; repeatable)")
    ap.add_argument("--workdir", help="child working directory (spawn mode)")
    ap.add_argument("--timeout", type=float, default=180.0,
                    help="hard cap for the whole run, from arm time")
    ap.add_argument("--post-load", type=float, default=20.0,
                    help="seconds to keep tracing once `loaded` is seen")
    ap.add_argument("--hit", help="hook name that must fire, else NOT-EXERCISED")
    ap.add_argument("--hit-min", type=int, default=1)
    ap.add_argument("--loaded-addr", default=None,
                    help="absolute site of the loaded observation (default 0x%x)"
                         % WORLD_ACTIVATED)
    ap.add_argument("--loaded-rva", default=None,
                    help="site of the loaded observation as an RVA off --module")
    ap.add_argument("--module", default="iwd2r.exe",
                    help="module --loaded-rva resolves against (default iwd2r.exe)")
    ap.add_argument("--no-loaded", dest="require_loaded", action="store_false",
                    help="drop the loaded observation; CLEAN no longer requires it")
    return ap


def main() -> int:
    global LOADED_ADDR, LOADED_RVA, LOADED_MODULE
    import frida
    ap = build_parser()
    ns = ap.parse_args()

    if bool(ns.exe) == bool(ns.attach):
        ap.error("exactly one of --exe / --attach")
    if ns.loaded_rva is not None:
        LOADED_RVA = ns.loaded_rva
        LOADED_MODULE = ns.module
    elif ns.loaded_addr is not None:
        LOADED_ADDR = int(ns.loaded_addr, 0)

    spec = json.loads(Path(ns.hooks).read_text(encoding="utf-8"))
    out = Path(ns.out)
    status = Path(ns.status) if ns.status else Path(str(out) + ".status")
    trace = Trace(out, status)

    session = None
    spawned = None
    try:
        trace.say("=== %s %s (%d hooks) ==="
                  % ("spawning" if ns.exe else "attaching",
                     ns.exe or ns.attach, len(spec["hooks"])))
        if ns.exe:
            for kv in ns.env:
                key, _, value = kv.partition("=")
                if not key:
                    ap.error("--env expects K=V, got %r" % kv)
                os.environ[key] = value
            spawned = frida.spawn(ns.exe, cwd=ns.workdir)
            session = frida.attach(spawned)
            pid = spawned
        else:
            session, pid = attach_when_there(ns.attach,
                                             time.monotonic() + ns.attach_timeout)
        session.on("detached", trace.on_detached)
        script = session.create_script(build_js(spec, ns.require_loaded))
        trace.script = script
        script.on("message", trace.on_message)
        script.load()
        if spawned is not None:
            frida.resume(spawned)
        trace.say("ARMED pid=%s" % pid)

        deadline = time.monotonic() + ns.timeout
        grace = None
        while time.monotonic() < deadline:
            if trace.crash is not None or trace.detached:
                break
            if trace.loaded and grace is None:
                grace = time.monotonic() + ns.post_load
            if grace is not None and time.monotonic() >= grace:
                break
            time.sleep(0.5)

        return trace.finish(ns.hit, ns.hit_min, require_loaded=ns.require_loaded)
    except Exception as exc:   # spawn/attach/setup failure: a status line, not a traceback
        trace.say("SETUP-ERROR %s: %s" % (type(exc).__name__, exc))
        trace.errors.append({"kind": type(exc).__name__, "detail": str(exc)})
        return trace.finish(ns.hit, ns.hit_min, require_loaded=ns.require_loaded)
    finally:
        # Spawn mode owns its child; attach mode is a guest of the driver.
        if spawned is not None:
            try:
                frida.kill(spawned)
            except Exception:
                pass
        if session is not None:
            try:
                session.detach()
            except Exception:
                pass
        if not trace.out.closed:
            trace.out.close()
        if not trace.status.closed:
            trace.status.close()


if __name__ == "__main__":
    raise SystemExit(main())
