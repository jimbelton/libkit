LIBRARIES = kit

include ../dependencies.mak

# List of the libraries in linker order.
# This is used by both the package GNUmakefiles and the top level GNUmakefile
#
#remove_to = $(if $(filter $(1),$(2)),$(call remove_to,$(1),$(wordlist 2,$(words $(2)),$(2))),$(2))
#ALL_LIBRARIES = lib
#LIB_DEPENDENCIES = $(call remove_to,$(LIBRARIES),$(ALL_LIBRARIES))
LIB_DEPENDENCIES = jemalloc

MAKE_ALLOW_LOWERCASE_TYPEDEF = 1

IFLAGS        += $(if $(findstring port,$(LIB_DEPENDENCIES)),$(CC_INC)$(COM.dir)/lib-port/$(OS_class),) \
                 -I$(TOP.dir)/libsxe/$(DST.dir)/include -I$(TOP.dir)/libjemalloc/$(DST.dir)
LINK_FLAGS    += $(TOP.dir)/libkit/lib-kit/$(DST.dir)/kit$(EXT.lib)
LINK_FLAGS    += $(TOP.dir)/libjemalloc/$(DST.dir)/jemalloc$(EXT.lib)
LINK_FLAGS    += $(TOP.dir)/libsxe/$(DST.dir)/libsxe$(EXT.lib)
LINK_FLAGS    += -lrt -rdynamic -pthread
CFLAGS        += -D_GNU_SOURCE=1 -D_FORTIFY_SOURCE=2    # man 7 feature_test_macros
CFLAGS        += -pthread
CFLAGS_WFORMAT = -Wformat=2
CFLAGS_MARCH   =

LINK_FLAGS    += -lpcap -lm -lz -lcrypto -ldl -pie -z noexecstack
