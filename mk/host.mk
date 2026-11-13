# mk/host.mk -- build the ROM on a host, with the 1985 tools under the emulator.
#
#	make -f mk/host.mk deps		place what DEPS says this repo consumes
#	make -f mk/host.mk stage	build the guest root, from vendor/mwc-1985
#	make -f mk/host.mk rom		compile and link
#	make -f mk/host.mk split	also produce rom.H and rom.L
#	make -f mk/host.mk check	split, then md5 both halves against V1.0
#	make -f mk/host.mk boot-check	split, then boot what was built
#	make -f mk/host.mk reloc-check	relink with the code shifted, then boot it
#	make -f mk/host.mk abi-check	check the service table, write build/romabi.h
#	make -f mk/host.mk clean
#
# Every target but `deps' and `clean' recurses into the real Makefile with the
# tool macros pointed at mk/mwc1985/.  There is deliberately no second copy of
# the build rules: a host build and a native build must be the same build, or
# byte-exactness on one says nothing about the other.
#
# GNU make reads this file; the C900's own make reads ./Makefile.  That is the
# only asymmetry -- this file has no build knowledge in it beyond which tools
# to use and where the reference images are.

MK      := $(dir $(lastword $(MAKEFILE_LIST)))
ROOT    := $(patsubst %/,%,$(dir $(patsubst %/,%,$(MK))))
MWC     := $(ROOT)/mk/mwc1985

C900_1985ROOT ?= $(ROOT)/build/1985root
export C900_1985ROOT

# Where the two reference EPROM images are, for `check'.  They are dumps of a
# physical part and are not in this repository; point this at the directory
# holding boot-L_V_1.0.bin and boot-H_V_1.0.bin.
C900_ROMREF ?=

TOOLS = CCBIN='sh $(MWC)/cc' ASBIN='sh $(MWC)/as' LDBIN='sh $(MWC)/ld' \
	CPPBIN='sh $(MWC)/cpp' RUN='sh $(MWC)/run'

.PHONY: rom split check boot-check reloc-check abi-check stage deps clean

rom split clean:
	$(MAKE) -C $(ROOT) $(TOOLS) $@

stage:
	sh $(ROOT)/mk/1985root.sh

deps:
	sh $(ROOT)/mk/deps-fetch.sh $(DEP)

# The acceptance test the README states.  It can only pass for an unmodified
# V1.0 and is retired the moment this repository changes the ROM's behaviour;
# what replaces it is a functional gate -- boot the built ROM in the emulator
# and require it to reach the same point the reference ROM reaches.
check: split
	@test -n "$(C900_ROMREF)" || { \
	  echo "*** check: C900_ROMREF is unset."; \
	  echo "*** It must name the directory holding the two reference dumps"; \
	  echo "*** boot-L_V_1.0.bin and boot-H_V_1.0.bin, which are dumps of a"; \
	  echo "*** physical EPROM pair and are not in this repository."; \
	  exit 2; }
	@cmp $(ROOT)/rom.H $(C900_ROMREF)/boot-H_V_1.0.bin && echo "rom.H == boot-H_V_1.0.bin"
	@cmp $(ROOT)/rom.L $(C900_ROMREF)/boot-L_V_1.0.bin && echo "rom.L == boot-L_V_1.0.bin"

# The gate that replaces `check' once the ROM is deliberately changed: boot
# what was built and require it to reach the point the reference reaches.
boot-check: split
	sh $(ROOT)/mk/bootcheck.sh


# Shift the code past crt.o and require the ROM to still work.
reloc-check: split
	sh $(ROOT)/mk/reloccheck.sh

# The service table must stay where callers expect it.
abi-check: rom
	sh $(ROOT)/mk/abicheck.sh
