"""An exclusive lock on a file shared by several worktrees.

    with filelock.hold('/path/to/x.lock', 'ghidra'):
        ...

hold() takes fcntl.flock(LOCK_EX) on the file (created if missing), waiting
for any other holder. After WAIT_NOTICE seconds of waiting it prints
"<what>: waiting for <path>" to stderr once. With a timeout it raises
TimeoutError when the lock is still held elsewhere after that many seconds.
The lock is released when the block ends or the process dies.
"""

import contextlib
import fcntl
import os
import sys
import time

WAIT_NOTICE = 5.0  # seconds before the waiting message
POLL = 0.05  # seconds between attempts


@contextlib.contextmanager
def hold(path, what, timeout=None):
    """Hold an exclusive flock on path for the duration of the block."""
    path = os.path.normpath(path)
    os.makedirs(os.path.dirname(path) or '.', exist_ok=True)
    fd = os.open(path, os.O_RDWR | os.O_CREAT, 0o666)
    try:
        start = time.monotonic()
        noticed = False
        while True:
            try:
                fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
                break
            except BlockingIOError:
                waited = time.monotonic() - start
                if timeout is not None and waited >= timeout:
                    raise TimeoutError(f'{what}: {path} is still held after '
                                       f'{timeout:g} s')
                if not noticed and waited >= WAIT_NOTICE:
                    print(f'{what}: waiting for {path}', file=sys.stderr,
                          flush=True)
                    noticed = True
                time.sleep(POLL)
        try:
            yield
        finally:
            fcntl.flock(fd, fcntl.LOCK_UN)
    finally:
        os.close(fd)
