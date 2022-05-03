# Required constexpr-if support, which came at least in GCC version 7.
CC := g++
CCFLAGS := -std=c++17
srcdir := $(CURDIR)/../src
objdir := $(CURDIR)/../obj/linux
sources := $(shell find $(srcdir) -name "*.cpp")
headers := $(shell find $(srcdir) -name "*.h") $(shell find $(srcdir) -name "*.hxx")
objects := $(patsubst $(srcdir)/%,$(objdir)/%.o,$(sources))
LIBMAKE = ar
INCLUDE := \
    -I$(CURDIR)/../../eirrepo/ \
    -I$(CURDIR)/../../eirrepo/sdk/ \
    -I$(CURDIR)/../../FileSystem/include/ \
    -I$(CURDIR)/../include/ \

main : $(objects) $(headers) ; mkdir -p ../lib/linux/ ; \
    $(LIBMAKE) rcs ../lib/linux/libgtaconfig.a $(objects)

$(objdir)/%.o : $(srcdir)/% ; \
    mkdir -p $(dir $@) ; \
    $(CC) $(CCFLAGS) -O3 -o $@ -c $< $(INCLUDE) ;

clean : ; \
    rm -rf $(objdir)

print-%: ; @echo '$*=$($*)'