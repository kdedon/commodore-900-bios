#!/bin/sh
# deps.sh -- resolve the one thing a HOST build consumes from another
# repository, and refuse by name when it is missing.
#
#   sh mk/deps.sh <dep>              print the resolved path, or nothing
#   sh mk/deps.sh -k <dep>           print the SHAPE the path resolved to, or
#                                    nothing: `checkout', or `release X.Y.Z'
#   sh mk/deps.sh -n <dep> [value]   print nothing; refuse and exit 2 if
#                                    <value> (or, empty, the search) does not
#                                    resolve
#
# The two modes exist because the edges are wanted at different times.  The
# search runs when a makefile is read, so a variable can be assigned from it;
# the REFUSAL belongs in the recipe that wanted the thing, because a NATIVE
# build on a booted C900 needs none of these and must not be blocked by a
# missing emulator.  -n takes the make variable's current value so that a
# `make C900_EMU=/wrong' is refused as what the user asked for rather than
# silently re-searched.
#
#   dep         variable          what it names
#   emu         C900_EMU          the emulator CHECKOUT (bin/c900 inside it)
#
# One edge, because the 1985 compiler is not an edge: it is in this repository,
# at vendor/mwc-1985.  What is left is a RUNNER -- those tools are Z8001
# binaries, and `c900 --exec' is how one runs on a host.
#
# This file is what DEPS and `make deps' talk to: the name column there is a
# name here, which is what lets the fetcher ask "is this edge already
# satisfied?" before placing anything, and what keeps `make deps' honest --
# it may only put things where these lists already look.
#
# mk/emulator.sh is the per-consumer resolver the build itself uses for the
# emulator; this file searches the same paths in the same order and exists so
# that ONE query answers for both.
#
# Search order: the variable wins; then the pinned release unpacked under
# deps/, then a c900 on $PATH; then a sibling emulator checkout, walking
# outward AT MOST THREE PARENTS, then one inside a `repos/' directory beside
# this repository.  Three parents is what reaches the enclosing workspace from
# a repository staged at <workspace>/repos/<repo>; further out is not a
# sibling, it is a coincidence -- an unbounded walk finds another job's
# checkout on a CI runner and reports a false success.

root=$(cd "$(dirname "$0")/.." && pwd)

# The sibling search list for a repository name: three parents, then repos/.
siblings() {
	_d=$root
	_n=0
	while [ $_n -lt 3 ] && [ "$_d" != / ]; do
		_d=$(cd "$_d/.." && pwd)
		echo "$_d/$1"
		_n=$((_n + 1))
	done
	echo "$root/repos/$1"
}

# Per dep: VAR names the variable, WANT what is being looked for, LIST the
# candidate paths, ok() the test that a candidate is the real thing, fixup()
# the value a caller's own spelling maps to, and shape() what KIND of thing the
# resolved path turned out to be.  The edge has two shapes -- an unpacked
# release or a tree somebody built -- and a build that did not report which it
# used would leave "what ran this" to be worked out from the filesystem.
shape() { echo checkout; }
case "$1" in
-n) mode=need; dep=$2; given=$3 ;;
-k) mode=kind; dep=$2; given= ;;
*)  mode=find; dep=$1; given= ;;
esac

case "$dep" in
emu)
	VAR="C900_EMU"
	WANT="the Commodore 900 emulator"
	LIST="$root/deps/commodore-900-emulator"
	p=$(command -v c900 2>/dev/null) &&
		LIST="$LIST $(dirname "$(dirname "$p")")"
	LIST="$LIST $(siblings commodore-900-emulator)"
	[ -n "$given" ] || given=${C900_EMU:-${EMU:-${EMUBIN:-}}}
	# A file names bin/c900; a directory names the checkout.  Both are
	# accepted, and the checkout is what is printed: rom/ sits beside bin/
	# in it, which is how $C900_EMU_ROM falls out with no second variable.
	fixup() {
		case "$1" in
		*/bin/c900) dirname "$(dirname "$1")" ;;
		*) echo "$1" ;;
		esac
	}
	ok() { [ -x "$1/bin/c900" ]; }
	shape() {
		if [ -f "$1/src/main.c" ] || [ -d "$1/src" ]; then
			echo checkout
		elif [ -f "$1/VERSION" ]; then
			echo "release $(sed -n 1p "$1/VERSION")"
		else
			echo unknown
		fi
	}
	HOW="  Clone https://github.com/MichalPleban/commodore-900-emulator
  and \`make' it, to one of the paths above -- or put its c900 on \$PATH,
  or set C900_EMU to the checkout or to its bin/c900.
  Only a HOST build needs it: every 1985 tool is a Z8001 binary and
  \`c900 --exec' is what runs one.  A native build on a booted C900 does not.
  \`make deps DEP=emu' unpacks the release DEPS pins."
	;;
*)
	echo "deps.sh: unknown dependency \`$dep' (emu)" >&2
	exit 2
	;;
esac

found=
if [ -n "$given" ]; then
	given=$(fixup "$given")
	ok "$given" && found=$given
else
	for c in $LIST; do
		c=$(fixup "$c")
		ok "$c" && { found=$c; break; }
	done
fi

if [ -n "$found" ]; then
	case "$mode" in
	find) echo "$found" ;;
	kind) shape "$found" ;;
	esac
	exit 0
fi

case "$mode" in find|kind) exit 0 ;; esac

{
	if [ -n "$given" ]; then
		echo "*** $WANT: nothing usable at $VAR=$given."
		echo "*** That is $VAR's own value, so nothing else was tried."
		echo "*** Unset it to search these instead:"
	else
		echo "*** $WANT: none found, and this target needs one."
		echo "*** $VAR is unset; the paths tried were:"
	fi
	for c in $LIST; do echo "***     $c"; done
	echo "$HOW" | sed 's/^/*** /'
} >&2
exit 2
