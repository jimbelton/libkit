COM.dir := $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)), $(MAKEFILE_LIST))))
TOP.dir = $(COM.dir)/..

# List of the libraries in linker order.
# This is used by both the package GNUmakefiles and the top level GNUmakefile
#
remove_to = $(if $(filter $(1),$(2)),$(call remove_to,$(1),$(wordlist 2,$(words $(2)),$(2))),$(2))
ALL_LIBRARIES = sxe-jitson kit sxe-dict sxe-cdb sxe-hash sxe-pool sxe-thread sxe-mmap sxe-list kit-alloc sxe-md5 sxe-util \
		sxe-log kit-mock sxe-test tzcode
LIB_DEPENDENCIES = $(call remove_to,$(LIBRARIES),$(ALL_LIBRARIES))

MAK_VERSION ?= 2    # By default, use the libtap package; set this to 1 to use libtap built in to libsxe
CONVENTION_OPTOUT_LIST = lib-kit/kit-queue.h
MAKE_ALLOW_LOWERCASE_TYPEDEF     = 1
MAKE_ALLOW_LOWERCASE_HASH_DEFINE = 1
MAKE_ALLOW_SPACE_AFTER_ASTERISK  = 1

COVERAGE_OPTOUT_LIST = lib-tzcode

include $(TOP.dir)/mak/mak-common.mak

CFLAGS     += -D_GNU_SOURCE=1 -D_FORTIFY_SOURCE=2 -pthread
LINK_DLLS  += -lxxhash -lresolv -lrt -rdynamic -pthread -ldl
LINK_FLAGS += -pie -z noexecstack

ifeq ($(OS_name), linux)
LINK_DLLS  += -lbsd -ljemalloc
endif
