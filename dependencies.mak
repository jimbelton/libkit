COM.dir := $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)), $(MAKEFILE_LIST))))
TOP.dir = $(COM.dir)/..

# List of the libraries in linker order.
# This is used by both the package GNUmakefiles and the top level GNUmakefile
#
remove_to        = $(if $(filter $(1),$(2)),$(call remove_to,$(1),$(wordlist 2,$(words $(2)),$(2))),$(2))
ALL_LIBRARIES    = sxe-jitson sxe-dict sxe-hash sxe-pool sxe-thread sxe-mmap sxe-list kit sxe-util sxe-log mock port
LIB_DEPENDENCIES = $(call remove_to,$(LIBRARIES),$(ALL_LIBRARIES))

MAK_VERSION ?= 2    # By default, use the libtap package; set this to 1 to use libtap built in to libsxe

# Convention opt-out list
CONVENTION_OPTOUT_LIST           = lib-kit/kit-queue.h
MAKE_ALLOW_SPACE_AFTER_ASTERISK  = 1    # Much of lib-sxe puts all declarations on separate lines, so it doesn't cuddle asterisks
MAKE_ALLOW_LOWERCASE_TYPEDEF     = 1
MAKE_ALLOW_LOWERCASE_HASH_DEFINE = 1    # Allow lower case defines for sxe-alloc.h wrappers

# Coverage opt-out list
COVERAGE_OPTOUT_LIST = lib-sxe-dict lib-sxe-hash lib-mock lib-port

include $(TOP.dir)/mak/mak-common.mak

LINK_FLAGS += -lresolv

ifneq ($(MAK_VERSION),1)    # Versions of mak > 1 use an external tap libary
	LINK_FLAGS += -ltap
else
	IFLAGS     += -I$(SXE.dir)/$(DST.dir)/include
	LINK_FLAGS += $(SXE.dir)/$(DST.dir)/libsxe$(EXT.lib)
endif

CFLAGS     += -D_FORTIFY_SOURCE=2 -pthread
LINK_FLAGS += -lrt -rdynamic -pthread -ldl -pie -z noexecstack

ifeq ($(OS_name), linux)
LINK_FLAGS    += -lbsd -ljemalloc
endif
