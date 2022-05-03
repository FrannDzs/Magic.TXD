CC := gcc
srcdir := $(CURDIR)/../src
headerdir := $(CURDIR)/../libtiff
objdir := $(CURDIR)/../obj/linux
sources := $(shell find $(srcdir) -name "*.c")
headers := \
    $(shell find $(srcdir) -name "*.h") $(shell find $(srcdir) -name "*.hxx") \
    $(shell find $(headerdir) -name "*.h") $(shell find $(headerdir) -name "*.hxx")
objects := $(patsubst $(srcdir)/%,$(objdir)/%.o,$(sources))
LIBMAKE = ar
INCLUDE := \
    -I$(headerdir)

main : $(objects) $(headers) ; mkdir -p ../lib/linux/ ; \
    $(LIBMAKE) rcs ../lib/linux/libtiff.a $(objects)

$(objdir)/%.o : $(srcdir)/% ; \
    mkdir -p $(dir $@) ; \
    $(CC) -O3 -o $@ -c $< $(INCLUDE) ;

clean : ; \
    rm -rf $(objdir)

print-%: ; @echo '$*=$($*)'