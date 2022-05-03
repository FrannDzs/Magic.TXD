CC := gcc
srcdir := $(CURDIR)/../src
headerdir := $(CURDIR)/../include
objdir := $(CURDIR)/../obj/linux
sources := $(shell find $(srcdir) -name "*.c")
headers := $(shell find $(srcdir) -name "*.h")
objects := $(patsubst $(srcdir)/%,$(objdir)/%.o,$(sources))
LIBMAKE = ar
INCLUDE := \
    -I$(CURDIR)/../include
    
main : $(objects) $(headers) ; mkdir -p ../lib/linux/ ; \
    $(LIBMAKE) rcs ../lib/linux/libimagequant.a $(objects)
    
$(objdir)/%.o : $(srcdir)/% ; \
    mkdir -p $(dir $@) ; \
    $(CC) -O3 -o $@ -c $< $(INCLUDE) ;

clean : ; \
    rm -rf $(objdir)

print-%: ; @echo '$*=$($*)'