#!/usr/bin/env bash
# Create and remove the worktree of a parallel work stream.
#
#   tools/worktree.sh new <stream>    worktree $BASE/p3-<stream> on a new
#                                     branch p3/<stream> from first's HEAD;
#                                     prints the path
#   tools/worktree.sh done <stream>   removes that worktree and deletes the
#                                     branch, only when the branch is merged
#                                     into first; otherwise refuses
#
# BASE is $IWD2R_WORKTREES, default /home/thier/iwd2-re-worktrees. Run it
# from any checkout of the repository. A worktree's Windows build folder is
# C:\Users\thier\iwd2r-build-p3-<stream> (tools/play.py win_checkout).
set -euo pipefail

BASE=${IWD2R_WORKTREES:-/home/thier/iwd2-re-worktrees}
TRUNK=first

usage() {
    echo "usage: tools/worktree.sh new|done <stream>" >&2
    exit 2
}

[ $# -eq 2 ] || usage
cmd=$1
stream=$2
if ! [[ $stream =~ ^[A-Za-z0-9_-]+$ ]]; then
    echo "worktree: stream name '$stream' must be letters, digits, _ or -" >&2
    exit 2
fi
branch=p3/$stream
path=$BASE/p3-$stream

case $cmd in
new)
    if [ -e "$path" ]; then
        echo "worktree: $path already exists" >&2
        exit 1
    fi
    if git show-ref --verify --quiet "refs/heads/$branch"; then
        echo "worktree: branch $branch already exists" >&2
        exit 1
    fi
    mkdir -p "$BASE"
    git worktree add --quiet -b "$branch" "$path" "$TRUNK" >&2
    echo "$path"
    ;;
done)
    if ! git show-ref --verify --quiet "refs/heads/$branch"; then
        echo "worktree: no branch $branch" >&2
        exit 1
    fi
    if ! git merge-base --is-ancestor "$branch" "$TRUNK"; then
        echo "worktree: $branch is not merged into $TRUNK; refusing" >&2
        exit 1
    fi
    if [ -e "$path" ]; then
        git worktree remove "$path"
    fi
    # Merged into first (checked above), whatever this checkout's HEAD is.
    git branch --quiet -D "$branch"
    echo "removed $path and $branch"
    ;;
*)
    usage
    ;;
esac
