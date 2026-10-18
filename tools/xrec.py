"""Cross-recovery comparison: per C++ function, in THEIRS vs OURS for src/*.cpp."""
import re, subprocess, sys, collections
BASE, THEIRS, OURS = sys.argv[1:4]
def files(ref):
    out = subprocess.run(['git','ls-tree','-r','--name-only',ref,'src'],capture_output=True,text=True).stdout.split()
    return {f for f in out if f.endswith('.cpp')}
def show(ref, path):
    r = subprocess.run(['git','show',f'{ref}:{path}'],capture_output=True,text=True,errors='replace')
    return r.stdout if r.returncode==0 else ''
sig = re.compile(r'^(?:[A-Za-z_][\w:<>,*&\s]*?\s+)?([A-Za-z_]\w*(?:::[~A-Za-z_]\w*)+|[A-Za-z_]\w*::operator\S*)\s*\([^;{]*\)\s*(?:const)?\s*\{', re.M)
def funcs(text):
    """name -> normalized body (brace-matched, whitespace/comments stripped). Rough but symmetric."""
    text = re.sub(r'//[^\n]*','',text); text = re.sub(r'/\*.*?\*/','',text,flags=re.S)
    out = {}
    for m in sig.finditer(text):
        i = m.end()-1; depth = 0; j = i
        while j < len(text):
            c = text[j]
            if c=='{': depth+=1
            elif c=='}':
                depth-=1
                if depth==0: break
            j+=1
        body = re.sub(r'\s+','',text[i:j+1])
        out[m.group(1)] = body
    return out
def stubby(body):  # TODO-only / empty bodies
    return len(body) < 40 or 'TODO' in body or 'NotImplemented' in body or 'assert(false' in body.replace(' ','')
tf, of, bf = files(THEIRS), files(OURS), files(BASE)
shared = sorted(tf & of)
tot = collections.Counter(); per = []
for p in shared:
    T, O, B = funcs(show(THEIRS,p)), funcs(show(OURS,p)), funcs(show(BASE,p))
    c = collections.Counter()
    for n in set(T)|set(O):
        t, o, b = T.get(n), O.get(n), B.get(n)
        if t is None: c['only_ours']+=1
        elif o is None: c['only_theirs']+=1
        elif t == o: c['same']+=1 if not stubby(t) else 0; c['same_stub']+= 1 if stubby(t) else 0
        else:
            ts, os_ = stubby(t), stubby(o)
            if ts and not os_: c['ours_real_theirs_stub']+=1
            elif os_ and not ts: c['theirs_real_ours_stub']+=1
            elif ts and os_: c['both_stub_diff']+=1
            elif t == b: c['ours_changed_theirs_base']+=1
            elif o == b: c['theirs_changed_ours_base']+=1
            else: c['both_changed_differ']+=1
    tot.update(c); per.append((p,c))
print("shared .cpp files:", len(shared), "| theirs-only files:", len(tf-of), "| ours-only files:", len(of-tf))
for k in ['same','same_stub','only_theirs','only_ours','theirs_real_ours_stub','ours_real_theirs_stub','theirs_changed_ours_base','ours_changed_theirs_base','both_changed_differ','both_stub_diff']:
    print(f"  {k:26} {tot[k]:6}")
print("\nfiles with most 'both_changed_differ' (true cross-check candidates):")
for p,c in sorted(per,key=lambda x:-x[1]['both_changed_differ'])[:10]: print(f"  {c['both_changed_differ']:4}  {p}  (theirs-real/ours-stub {c['theirs_real_ours_stub']}, ours-real/theirs-stub {c['ours_real_theirs_stub']})")
print("\ntheirs-only files: functions (real bodies) they recovered that we don't have at all:")
n=0
for p in sorted(tf-of):
    T=funcs(show(THEIRS,p)); n+=sum(1 for b in T.values() if not stubby(b))
print("  ", n, "functions across", len(tf-of), "files")
