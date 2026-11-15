#!/bin/sh
# mk/version.sh -- write version.h, the banner's version and date, from git.
#
# At a v* tag the version is the tag's: v2.0 -> 2.0.  Anywhere else it is
# VERSION and the commit, 2.0.d13c4dc, with a + when the tree has changes.
# The date is the commit's, so one commit always builds the same ROM.
set -e
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
if v=$(git describe --tags --exact-match --match 'v[0-9]*' HEAD 2>/dev/null); then
	v=${v#v}
else
	v=$(cat VERSION).$(git rev-parse --short=7 HEAD)
fi
git diff --quiet HEAD -- . ':!version.h' || v=$v+
d=$(git log -1 --format=%cd --date=format:'%-m/%-d/%y')
new=$(printf '#define VERSION "%s"\n#define VDATE "%s"\n' "$v" "$d")
[ -f version.h ] && [ "$(cat version.h)" = "$new" ] || printf '%s\n' "$new" > version.h
