CC := gcc
srcdir := $(CURDIR)/../src
objdir := $(CURDIR)/../obj/linux
sources := $(shell find $(srcdir) -name "*.c") $(shell find $(srcdir) -name "*.cxx")
headers := $(shell find $(srcdir) -name "*.h")
objects := $(patsubst $(srcdir)/%,$(objdir)/%.o,$(sources))
LIBMAKE = ar
INCLUDE := \
    -I$(CURDIR)/../include
    
main : $(objects) $(headers) ; mkdir -p ../lib/linux/ ; \
    $(LIBMAKE) rcs ../lib/linux/libsquish.a $(objects)
    
$(objdir)/%.o : $(srcdir)/% ; \
    mkdir -p $(dir $@) ; \
    $(CC) -O3 -o $@ -c $< $(INCLUDE) ;

clean : ; \
    rm -rf $(objdir)

print-%: ; @echo '$*=$($*)'