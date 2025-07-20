DLLIBRARIES = libkit
MAKE_DEBS   = 1

DST.lib+=	$(DST.dir)/libkit$(EXT.lib)

libsources  = $(wildcard $(1)/*.c) $(subst $(OS_class)/,,$(wildcard $(1)/$(OS_class)/*.c))
libobjects  = $(foreach SOURCE,$(call libsources,$(1)),$(patsubst $(1)/%.c,$(1)/$(DST.dir)/%$(EXT.obj),$(SOURCE)))
DST.obj     = $(foreach PACKAGE,$(filter-out tzcode,$(ALL_LIBRARIES)),$(call libobjects,$(COM.dir)/lib-$(PACKAGE)))
TZCODE.dir  = $(COM.dir)/lib-tzcode
DST.obj    += $(TZCODE.dir)/asctime.o $(TZCODE.dir)/difftime.o $(TZCODE.dir)/localtime.o $(TZCODE.dir)/strftime.o

libheaders=	$(wildcard $(1)/*.h) $(wildcard $(1)/$(DST.dir)/*.h) $(wildcard $(1)/$(OS_class)/*.h)
SRC.inc.lib=	$(filter-out %/private.h %-private.h,$(foreach PACKAGE,$(ALL_LIBRARIES),$(call libheaders,$(COM.dir)/lib-$(PACKAGE))))
DST.inc.lib=	$(filter-out %/private.h %-private.h,$(foreach PACKAGE,$(ALL_LIBRARIES),$(subst /$(OS_class),,$(subst ./lib-$(PACKAGE),$(DST.dir)/include,$(call libheaders,$(COM.dir)/lib-$(PACKAGE))))))
SRC.inc=	$(SRC.inc.lib)
DST.inc=	$(DST.inc.lib)

DEB_BUILD_NUMBER=	$(shell echo -$${BUILD_NUMBER-dev})

include dependencies.mak

ifdef MAKE_DEBUG
$(info make: debug: SRC.inc           : $(SRC.inc))
endif

include:
	@$(MAKE_PERL_ECHO_BOLD) "make[$(MAKELEVEL)]: updating: $(DST.dir)/include"
	-$(MAKE_RUN)$(MKDIR) $(call OSPATH,$(DST.dir)/include) $(TO_NUL) $(FAKE_PASS)
	$(COPYFILES2DIR) $(SRC.inc.lib) $(DST.dir)/include

clean::         $(DEP.dirs)
	rm -rf target
	rm -rf output

realclean::     $(DEP.dirs)
	rm -rf target
	rm -rf output

package:
	if [ ! -e build-linux-64-release/libkit.a ]; then $(MAKE) release; fi
	mkdir -p target/include
	cp -rp build-linux-64-release/include/*.h target/include
	cp build-linux-64-release/libkit.a target
	cp build-linux-64-release/libkit.so.$(DEB.ver) target
	cp -p bin/kit-alloc-analyze target
	ln -sf libkit.so.$(DEB.ver) target/libkit.so.$(DEB.ver.maj)
	ln -sf libkit.so.$(DEB.ver.maj) target/libkit.so
	cp -rp debian target
	@err=$$(dpkg-parsechangelog -l target/debian/changelog 2>&1 >/dev/null); [ -z "$$err" ] || { echo "$$err"; false; }
	perl -nale '$$_ =~ s/libkit \(([0-9.-]+)\) (\w+); urgency=(\w+)/libkit ($$1$(DEB_BUILD_NUMBER)) $$2; urgency=$$3/g;print' <debian/changelog >target/debian/changelog
	cd target && dpkg-buildpackage -b -rfakeroot -uc -us
	mkdir -p output
	mv libkit_*_amd64.deb libkit-dev_*_amd64.deb libkit_*_amd64.changes libkit_*_amd64.buildinfo output
