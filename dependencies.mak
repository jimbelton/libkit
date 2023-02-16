COM.dir := $(patsubst %/,%,$(dir $(word $(words $(MAKEFILE_LIST)), $(MAKEFILE_LIST))))
TOP.dir = $(COM.dir)/..

# List of the libraries in linker order.
# This is used by both the package GNUmakefiles and the top level GNUmakefile
#
remove_to = $(if $(filter $(1),$(2)),$(call remove_to,$(1),$(wordlist 2,$(words $(2)),$(2))),$(2))
ALL_LIBRARIES = kit
LIB_DEPENDENCIES = $(call remove_to,$(LIBRARIES),$(ALL_LIBRARIES))

MAK_VERSION ?= 2    # By default, use the libtap package; set this to 1 to use libtap built in to libsxe
CONVENTION_OPTOUT_LIST = lib-kit/kit-queue.h
MAKE_ALLOW_LOWERCASE_TYPEDEF = 1

include $(TOP.dir)/mak/mak-common.mak

LINK_FLAGS += -lresolv

ifneq ($(MAK_VERSION),1)    # Versions of mak > 1 use an external tap libary
	LINK_FLAGS += -ltap
endif

IFLAGS      += -I$(SXE.dir)/$(DST.dir)/include
CFLAGS      += -D_GNU_SOURCE=1 -D_FORTIFY_SOURCE=2 -pthread
LINK_FLAGS  += $(SXE.dir)/$(DST.dir)/libsxe$(EXT.lib) -lrt -rdynamic -pthread -ldl -pie -z noexecstack

ifeq ($(OS_name), linux)
LINK_FLAGS    += -lbsd -ljemalloc
endif
