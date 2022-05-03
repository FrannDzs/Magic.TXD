CC := g++
CCFLAGS := -std=c++17
srcdir := $(CURDIR)/../src
objdir := $(CURDIR)/../obj/linux
sources := $(shell find $(srcdir) -name "*.cpp")
headers := $(shell find $(srcdir) -name "*.h") $(shell find $(srcdir) -name "*.hxx")
objects := $(patsubst $(srcdir)/%,$(objdir)/%.o,$(sources))
LIBMAKE = ar
INCLUDE := \
    -I$(CURDIR)/../include \
    -I$(CURDIR)/../src \
    -I$(CURDIR)/../../eirrepo \
    -I$(CURDIR)/../../eirrepo/sdk
BUILD_DIR := \
    $(CURDIR)
PREPROC := 
VENDORS := 
LINKEDWITH :=

ifneq ($(wildcard $(CURDIR)/../../zlib), )
    VENDORS := $(VENDORS) zlib.vendor
    PREPROC := $(PREPROC) -D FILESYS_ENABLE_ZIP
    INCLUDE := $(INCLUDE) -I$(CURDIR)/../../zlib/include
    LINKEDWITH := $(LINKEDWITH) $(CURDIR)/../../zlib/lib/linux/libzlib.a
endif

ifneq ($(wildcard $(CURDIR)/../../lzo-2.08), )
    VENDORS := $(VENDORS) lzo-2.08.vendor
    PREPROC := $(PREPROC) FILESYS_ENABLE_LZO
    INCLUDE := $(INCLUDE) -I$(CURDIR)/../../lzo-2.08/include
    LINKEDWITH := $(LINKEDWITH) $(CURDIR)/../../lzo-2.08/lib/liblzo2.a
endif

main : $(objects) $(headers) $(VENDORS) ; \
    cd $(BUILD_DIR) ; \
    mkdir -p ../lib/linux/ ; \
    $(LIBMAKE) rcsT ../lib/linux/libfs.a $(objects) ; \
    $(LIBMAKE) rcT ../lib/linux/libfs.a $(LINKEDWITH) ;

$(objdir)/%.o : $(srcdir)/% ; \
    mkdir -p $(dir $@) ; \
    $(CC) $(CCFLAGS) -O3 -o $@ -c $< -Wno-invalid-offsetof $(INCLUDE) ;

lzo-2.08.vendor : ; \
    cd $(BUILD_DIR)/../../lzo-2.08/ ; \
    B/generic/build.sh ; \
    mv liblzo2.a lib/ ; \
    B/generic/clean.sh ; \

%.vendor : ; \
    cd $(BUILD_DIR)/../../$(patsubst %.vendor,%,$@)/build/ ; \
    make

clean : ; \
    rm -rf $(objdir)

%.vclean : ; \
    cd $(BUILD_DIR)/../vendor/$(patsubst %.vclean,%,$@)/build/ ; \
    make clean

print-%: ; @echo '$*=$($*)'