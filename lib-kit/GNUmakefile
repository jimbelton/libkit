LIBRARIES = kit

ifdef ZEUS_TOP.dir
# Use the ZEUS libsxe
ifdef LIBSXE_DEV_ROOT
SXE.dir = $(LIBSXE_DEV_ROOT)
else
SXE.dir = $(ZEUS_TOP.dir)/libsxe
endif
else
SXE.dir = $(TOP.dir)/libsxe
endif

include ../dependencies.mak

ifdef ZEUS_TOP.dir
IFLAGS_TEST   := $(filter-out -I$(TOP.dir)/libsxe/lib-tap/$(DST.dir), $(IFLAGS_TEST))
endif

MAKE_ALLOW_LOWERCASE_TYPEDEF = 1

LINK_FLAGS    += $(TOP.dir)/libkit/lib-kit/$(DST.dir)/kit$(EXT.lib)
CFLAGS_WFORMAT = -Wformat=2
CFLAGS_MARCH   =
