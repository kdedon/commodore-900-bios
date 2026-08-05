# The June 1985 Mark Williams Z8001 system, as this ROM is built with

Everything the host build of the ROM needs, other than the emulator that runs
it: the C preprocessor and the three compiler passes, the assembler, the 32-bit
linkage editor, two object utilities, the C library, the C runtime startoff and
the three header trees the system's own sources include.  `mk/1985root.sh`
copies this into `build/1985root`, and `c900 --exec` reroots the guest's
absolute paths there, so `-I/usr/sys/z8001/h` and `/lib/libc.a` mean on a host
what they mean on the machine.

Recovered from a Commodore 900 hard-disk image.  Dated June 1985.

## The tools

| file | bytes | sha256 |
|---|---|---|
| bin/as | 38006 | 877a6ef07bc025c4 |
| bin/nld | 20922 | 580a26a44215abac |
| bin/nm | 9764 | c4d31a38467f9cbd |
| bin/size | 7350 | 051d50306adbe92a |
| lib/cpp | 17972 | 094a0449fc3c5eca |
| lib/cc0 | 46374 | 2a7622043f16844d |
| lib/cc1 | 59292 | 7a45bffaf20d836b |
| lib/cc2 | 32342 | d3561efb8c0f7f1a |

`nld`, not `ld`: `cc2` emits the 32-bit object format (`l_flag` carries
`LF_32`) and `nld` is the loader for it.  There is no assembly step for C --
`cc2` writes a COHERENT `l.out` object directly -- so `as` is called only for
the ROM's hand-written `.s` files.  The original driver `/bin/ccx`
fork/exec/waits each pass, which a one-process runner cannot do, so it is not
here: `mk/mwc1985/cc` does that sequencing on the host.

`VARIANT`, beside them, is the variant word these passes must be run with, by
kind of artifact, with the evidence for each written next to it.  It travels
with the binaries because the wrong word silently produces objects that are not
the ones the machine shipped, with no diagnostic anywhere.

## What they compile and link against

| path | what | sha256 |
|---|---|---|
| lib/libc.a | /lib/libc.a, 76062 bytes | 18ae9fda3d418608 |
| lib/crts0.o | /lib/crts0.o, 255 bytes | 4680e1f8a41dd318 |
| usr/include | /usr/include, 46 headers + sys/, 13 | |
| usr/sys/h | /usr/sys/h, 31 headers | |
| usr/sys/z8001/h | /usr/sys/z8001/h, 24 headers | |

The ROM is a SYSTEM artifact: it compiles with `-I/usr/sys/z8001/h` and
includes `<../../h/...>` across into `/usr/sys/h`, so those two trees are
present or the ROM does not compile.

The forward-ported COHERENT headers are NOT a substitute -- they are a later
system, and compiling against them does not give the 1985 objects.  Neither is
COHERENT 0.8's tree: its headers differ in `ktty.h`, `machine.h`, `mdata.h` and
`alloc.h`, and its `slibc.a` is half the size.

Setting `C900_COH_IMAGE` stages the library and the headers out of a stock
COHERENT 0.7.3 hard-disk image instead of out of this tree, which is how these
copies are checked against a real disk.  That image is large, private and in no
repository; this tree is the path that works anywhere.

## Binaries, in a repository that otherwise holds none

Deliberate, and the test is REGENERABILITY.  The no-binaries rule keeps build
output out of a source tree; nothing here can reproduce these, because the
compiler that emitted them exists only as these files.  Losing them would be
permanent, and they came within one disk of being lost already.  `libc.a` and
`crts0.o` pass the same test for the same reason: the sources they were built
from are in no holding, and the compiler that built them is the one above.

They are here rather than fetched because this repository reproduces a 1985 ROM
byte for byte, and only the 1985 compiler does that.  A build that had to
acquire its compiler from elsewhere could not be checked out and run.

The same binaries are also in `commodore-900-toolchain/vendor/mwc-1985`, which
is that compiler's lineage: ours is MWC C for the Z8001 ported forward, and the
1985 originals are the other end of the same line.  Two copies is the cost of
each repository standing alone; they are recovered artifacts and do not change,
so the copies cannot drift except by one of them being replaced.

## `usr/sys/h/con.h` and Windows

`con` is a reserved DOS device name, so git cannot check this repository out on
Windows: the checkout fails outright with "invalid path".  It is not worked
around.  The file is named by `#include <con.h>` in sources this repository
reproduces byte for byte, CI is Linux, and no Windows binary is published.  A
Windows user builds this in WSL or a container.

## Licence

Mark Williams Company work, (c) 1977-1995 Robert Swartz, released under BSD
3-Clause.  Condition 2 applies to these specifically: a binary redistribution
must reproduce the copyright notice and disclaimer in its documentation.
