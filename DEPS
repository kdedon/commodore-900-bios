# DEPS -- what this repository consumes from other repositories.
#
#	name  kind  url  [ref]  [asset]  [dest]
#
# Read by `make deps' (mk/deps-fetch.sh) and by CI.  It is a statement of what
# is consumed, not a resolver: nothing in the build reads this file.  mk/deps.sh
# still does the finding, and a named variable still wins.  The name column is
# the dep name that resolver answers to, so `make deps' can ask it whether an
# edge is already satisfied before placing anything.
#
# kind release = a BINARY, PINNED by tag and unpacked into deps/, which is
# gitignored.  Nothing about a binary is recoverable from our own history, so
# "which one ran this" has to be a number chosen in advance.  <dest> names the
# directory under deps/ and defaults to the url's basename.
#
# ONE EDGE, and this repository is otherwise self-contained.  It reproduces a
# 1985 ROM byte for byte, which is possible only with the 1985 compiler; that
# compiler, its assembler, its linkage editor, its C library and the three
# header trees the ROM includes are IN this repository, at vendor/mwc-1985,
# because they exist as recovered Z8001 executables and as nothing else.  There
# is no source edge to have, and nothing is fetched in order to build the ROM.
#
# What the edge is wanted for:
#
#   emu        THE RUNNER.  Every 1985 tool in this build is a Z8001 COHERENT
#              binary, so `c900 --exec' -- a one-process runner -- is how any
#              of them runs on a host at all.  The same emulator boots the ROM
#              that was built, which is what `boot-check' tests.  A NATIVE
#              build on a booted C900 needs none of this: there the tools are
#              just /bin/cc and /bin/as, and the Makefile's defaults name them.
#
# Nothing else is needed.  Setting $C900_COH_IMAGE stages the C library and the
# header trees out of a stock COHERENT 0.7.3 hard-disk image instead of out of
# vendor/, which is how the vendored copies are checked against a real disk.
# The image is large, private and in no repository; vendor/ is the path that
# works anywhere.

emu      release  https://github.com/kdedon/commodore-900-emulator   v0.4        c900-@REF@-@HOST@
