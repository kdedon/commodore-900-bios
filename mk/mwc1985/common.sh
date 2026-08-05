# mk/mwc1985/common.sh -- where the June 1985 Mark Williams tools live, and how
# to run one.  Sourced by the cc/cpp/as/ld wrappers beside it; not executable.
#
# Every tool in this flavour is a Z8001 COHERENT binary, so "running" one means
# handing it to the emulator's process runner (`c900 --exec'), which services
# its system calls against the host filesystem.  There is exactly one guest
# process per invocation: the wrappers, and make above them, do the sequencing
# that the original driver `ccx' did with fork/exec/wait -- which the runner
# cannot provide, and says so rather than faking it.
#
# THE TOOLS LIVE IN THE STAGED ROOT: mk/1985root.sh puts cpp, cc0/cc1/cc2, as,
# nld, nm, size and VARIANT there beside the /lib and /usr trees they read, all
# of it out of vendor/mwc-1985.  So one variable says where everything is, and
# a tool and the headers it compiles against cannot come from different
# vintages.
#
# Override in the environment: C900_EMU, C900_1985ROOT.

set -e

# The repository root: this file is at <root>/mk/mwc1985/common.sh.
mwc_here=$(cd "$(dirname "$0")" && pwd)
: "${C900_ROOT:=$(cd "$mwc_here/../.." && pwd)}"

# The emulator: resolved by mk/emulator.sh, the same search mk/deps.sh uses, so
# shell and make agree.
. "$C900_ROOT/mk/emulator.sh"

# The staged guest root: the filesystem the 1985 tools see through $N2ROOT,
# and where the tools themselves are.
# /lib/libc.a and the <sys/...> headers are named by ABSOLUTE guest paths in
# the Makefile -- exactly as they are on a real machine -- and $N2ROOT is what
# makes those paths mean something on a host.
: "${C900_1985ROOT:=$C900_ROOT/build/1985root}"

mwc_die() { echo "mwc1985: $*" >&2; exit 1; }

# mwc_need <path> <what it is needed for>: a missing tool is named, once, with
# the reason -- not left to surface as the emulator's "cannot open".
mwc_need() {
	emu_need "run the 1985 tools, which are Z8001 binaries"
	[ -f "$1" ] || mwc_die "$2: $1 is missing"
}

# mwc_variant <kind>: the variant word to compile a <kind> artifact with.
#
# The words are a property of THESE binaries and travel with them.
# It sits beside the binaries in vendor/mwc-1985, with the evidence for each
# kind written next to it, so it cannot be separated from what it describes;
# mk/1985root.sh stages it into the root.  Nothing here decides a word; a
# caller that must override one sets MWC1985_VARIANT in the environment.
#
# The ROM is a `system' artifact -- the kind the kernel, the drivers and the
# libraries are -- and this repository builds nothing else.
mwc_variant() {
	[ -n "${MWC1985_VARIANT:-}" ] && { echo "$MWC1985_VARIANT"; return 0; }
	mwc_vt=$C900_1985ROOT/VARIANT
	[ -f "$mwc_vt" ] || mwc_die "$mwc_vt is missing: it is where the variant words are written down"
	mwc_vw=$(sed -n "s/^$1[ 	][ 	]*\([0-9a-fA-F][0-9a-fA-F]*\).*/\1/p" "$mwc_vt")
	[ -n "$mwc_vw" ] || mwc_die "no variant word for kind \`$1' in $mwc_vt"
	echo "$mwc_vw"
}

# mwc_need_root: the staged guest root, which every compile reads headers from
# and every link reads libc.a from.  Refuse naming the variable and the maker.
mwc_need_root() {
	[ -f "$C900_1985ROOT/lib/libc.a" ] && return 0
	{
		echo "mwc1985: no staged 1985 root at C900_1985ROOT=$C900_1985ROOT"
		echo "  It holds the 1985 tools, /lib/libc.a, /lib/crts0.o and the"
		echo "  /usr/sys/z8001/h, /usr/sys/h and /usr/include header trees,"
		echo "  staged from vendor/mwc-1985."
		echo "  Run:  sh mk/1985root.sh"
		echo "  or set C900_1985ROOT to a directory already holding them."
	} >&2
	exit 2
}

# mwc_run <tool> [args...]: one guest process.  $N2ROOT makes the guest's
# ABSOLUTE paths (/lib/libc.a, /usr/sys/z8001/h) resolve into the staged root;
# relative paths still mean the host's current directory, which is how the
# Makefile's own sources and objects are found.  The tool is named by a HOST
# path and is not rerooted.
mwc_run() {
	t=$1; shift
	N2ROOT=$C900_1985ROOT "$C900_EMU" --exec "$t" "$@"
}

# mwc_cpp_check <preprocessed-file> <original-source>: fail if /lib/cpp put a
# diagnostic in its OUTPUT.  The 1985 preprocessor writes every message to
# fd 1, in among the C it emits.  An error also exits 1, so `set -e' stops the
# compile; a warning exits 0 and the message becomes a line of C, so the next
# pass reports a syntax error near it -- or, worse, does not.  Match on the
# `NN: Warning: ' severity prefix; real preprocessed C cannot contain it.
mwc_cpp_check() {
	if grep -n '[0-9]: Warning: ' "$1" >&2; then
		mwc_die "$2: /lib/cpp warned, and its warning is now part of the preprocessed source (above)"
	fi
	return 0
}
