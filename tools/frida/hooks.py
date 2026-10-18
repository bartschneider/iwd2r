#!/usr/bin/env python3
"""Hook-table -> Frida JS renderer, shared by probe.py and orig.py.

Adopted from the WSO fork's scripts/frida_hooks.py (see docs/THIRD_PARTY.md);
the readers, the argument expressions, the prelude and the four render
functions are his.  One schema, two runtimes: probe.py generates a standalone
driver for a quick look, orig.py runs the same table unattended with a
verdict.  Keep new keys optional so a table written for one still renders
under the other.

Hook spec (JSON):
{
  "process": "IWD2.exe",              // probe only; orig uses --exe
  "log": "trace.jsonl",               // VM-side, relative to the run folder
  "hooks": [
    { "addr": "0x73F560", "name": "MTP", "conv": "thiscall",
      "args": ["s32", "s32", "s16"],
      "this_dump": [ {"off": "0x06", "type": "s16", "label": "posx"} ],
      "globals": [ {"chain": ["0x8CF6D8", "0x104C"], "type": "u32",
                    "label": "idLocalPlayer"} ],
      "bt": true,                     // 8-frame EBP walk on entry
      "ret": "s32",                   // log retval onLeave
      "max": 200 }                    // stop after N hits (default 500)
  ]
}

Types: u8 u16 u32 s16 s32 ptr f32 str wstr.  "str"/"wstr" deref, guarded.
conv: thiscall (this=ecx, stack args -> args[0..]) | cdecl | stdcall.
Hook function ENTRIES only (Frida's x86 inline trampoline rewrites ~5 bytes
and corrupts anything mid-function).

Addresses (ours, in addition to his): a hook may carry "rva" instead of
"addr" -- an offset from the module base, resolved in the target at install
time (Process.getModuleByName(...).base).  Tables for the original use
absolute addresses (ImageBase 0x400000, no ASLR); tables for our build use
"rva" because our addresses move with every rebuild.  "globals" chains stay
absolute in both.

"globals" is a deref chain: the first element is an absolute address, every
element after it is an offset applied after a readPointer().  A single-element
chain reads the global itself.  That is how you log engine state a hook's own
`this` does not reach -- e.g. g_pChitin(0x8CF6D8)->cNetwork.m_idLocalPlayer(+0x104C).
"""

import re

READERS = {   # NativePointer -> value
    "u8":   "{p}.readU8()",
    "u16":  "{p}.readU16()",
    "u32":  "{p}.readU32()",
    "s16":  "(({p}.readU32() << 16) >> 16)",
    "s32":  "{p}.readS32()",
    "f32":  "{p}.readFloat()",
    "ptr":  "{p}.readPointer().toString()",
    "str":  "{p}.readUtf8String(64)",
    "wstr": "{p}.readUtf16String(64)",
}
ARG_VALUE = {   # NativePointer arg -> logged value
    "u32": "{a}.toUInt32()",
    "s32": "{a}.toInt32()",
    "s16": "(({a}.toInt32() << 16) >> 16)",
    "u16": "({a}.toUInt32() & 0xffff)",
    "u8":  "({a}.toUInt32() & 0xff)",
    "ptr": "{a}.toString()",
    "f32": "{a}.toInt32()",          # raw bits; floats passed on stack: read via dump if needed
    "str": "rdstr({a})",
    "wstr": "rdwstr({a})",
}

# JS helpers every rendered hook may reference.  ebpwalk mirrors the one in
# the fork's crash guard: x86 frame chain, bounded by a plausible-stack window
# so a garbage EBP stops the walk instead of faulting.
PRELUDE = r"""function guard(f) { try { return f(); } catch (e) { return 'ERR:' + e; } }
function rdstr(p)  { try { return p.readUtf8String(64); } catch (e) { return 'ERR'; } }
function rdwstr(p) { try { return p.readUtf16String(64); } catch (e) { return 'ERR'; } }
function ebpwalk(fp, depth) {
  const out = [];
  const lo = fp, hi = fp.add(0x80000);
  for (let i = 0; i < depth && !fp.isNull(); i++) {
    if (fp.compare(lo) < 0 || fp.compare(hi) > 0) break;
    try {
      out.push(fp.add(4).readPointer().toString());
      fp = fp.readPointer();
    } catch (e) { break; }
  }
  return out;
}"""

DEFAULT_MODULE = 'iwd2r.exe'


def addr_expr(hook):
    """The JS expression of a hook's site: ptr(0x...) for an absolute
    address, HOOKBASE.add(0x...) for an rva off the hook's module."""
    if 'rva' in hook:
        return 'HOOKBASE%s.add(%s)' % (_base_suffix(_hook_module(hook)),
                                       hook['rva'])
    if 'addr' in hook:
        return 'ptr(%s)' % (hook['addr'],)
    raise ValueError('hook %r has neither addr nor rva' % hook.get('name'))


def _hook_module(hook):
    return hook.get('module', DEFAULT_MODULE)


def _base_suffix(module):
    if module == DEFAULT_MODULE:
        return ''
    return '_' + re.sub(r'[^A-Za-z0-9]', '_', module)


def base_prelude(hooks):
    """const HOOKBASE... lines for the modules rva hooks resolve against --
    empty when every hook carries an absolute address."""
    modules = sorted({_hook_module(h) for h in hooks if 'rva' in h})
    return '\n'.join("const HOOKBASE%s = Process.getModuleByName('%s').base;"
                     % (_base_suffix(m), m) for m in modules)


def _global_js(g):
    """{"chain": [...], "type": t} -> a guarded read expression."""
    chain = [c if isinstance(c, str) else hex(c) for c in g["chain"]]
    expr = f"ptr({chain[0]})"
    for off in chain[1:]:
        expr = f"{expr}.readPointer().add({off})"
    return READERS.get(g.get("type", "u32"), READERS["u32"]).format(p=expr)


def js_for_hook(h, idx):
    name = h["name"]
    conv = h.get("conv", "thiscall")
    args = h.get("args", [])
    maxn = int(h.get("max", 500))
    parts = []
    parts.append(f"let cnt{idx} = 0;")
    parts.append(f"Interceptor.attach({addr_expr(h)}, {{")
    parts.append("  onEnter(args) {")
    parts.append(f"    if (++cnt{idx} > {maxn}) return;")
    fields = [f"tag: '{name}'"]
    if conv == "thiscall":
        parts.append("    const thiz = this.context.ecx;")
        fields.append("this: thiz.toString()")
    for i, t in enumerate(args):
        expr = ARG_VALUE.get(t, ARG_VALUE["u32"]).format(a=f"args[{i}]")
        fields.append(f"a{i}_{t}: {expr}")
    for d in h.get("this_dump", []):
        off = d["off"] if isinstance(d["off"], str) else hex(d["off"])
        rd = READERS.get(d.get("type", "u32"), READERS["u32"]).format(p=f"thiz.add({off})")
        fields.append(f"{d.get('label', 'f_' + off)}: guard(() => {rd})")
    for g in h.get("globals", []):
        label = g.get("label", "g_" + str(g["chain"][0]))
        fields.append(f"{label}: guard(() => {_global_js(g)})")
    if h.get("bt"):
        fields.append("bt: guard(() => ebpwalk(this.context.ebp, 8))")
    parts.append("    send({ " + ", ".join(fields) + " });")
    if h.get("ret"):
        parts.append("  },")
        parts.append("  onLeave(retval) {")
        parts.append(f"    if (cnt{idx} > {maxn}) return;")
        rexpr = ARG_VALUE.get(h["ret"], ARG_VALUE["u32"]).format(a="retval")
        parts.append(f"    send({{ tag: '{name}_ret', ret: {rexpr} }});")
    parts.append("  }")
    parts.append("});")
    return "\n".join(parts)


def hooks_js(hooks):
    return "\n".join(js_for_hook(h, i) for i, h in enumerate(hooks))


def hit_counts_js(hooks, period_ms=2000):
    """Re-emit per-hook totals on a timer.

    Two reasons this is a timer and not a teardown handler: the wrapper kills
    the game rather than letting the driver unwind (same as the fork's crash
    guard), and the counters keep incrementing past a hook's "max", so the
    totals stay true even once logging has stopped."""
    counts = ", ".join("'%s': cnt%d" % (h["name"], i) for i, h in enumerate(hooks))
    return "setInterval(() => send({ tag: 'HITS', counts: { %s } }), %d);" % (counts, period_ms)


def install_dump_js(hooks):
    """Gotcha 5: dump 8 bytes at every hook site so a silent no-fire can be
    diffed against the bytes at that address instead of blamed on it."""
    entries = ", ".join(
        "{ name: '%s', addr: %s }" % (h["name"], addr_expr(h)) for h in hooks
    )
    return (
        "send({ tag: 'sites', sites: [%s].map(h => ({\n"
        "  name: h.name, addr: h.addr.toString(),\n"
        "  bytes: guard(() => Array.from(new Uint8Array(h.addr.readByteArray(8)))\n"
        "    .map(b => ('0' + b.toString(16)).slice(-2)).join('')) })) });" % entries
    )
