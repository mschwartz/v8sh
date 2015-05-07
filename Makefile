# Makefile for commands

APP      = v8sh

SRCEXT   = cc
SRCDIR   = src
OBJDIR   = obj
BINDIR   = ./bin

SRCS    := $(shell find $(SRCDIR) -name '*.$(SRCEXT)')
SRCDIRS := $(shell find . -name '*.$(SRCEXT)' -exec dirname {} \; | uniq)
OBJS    := $(patsubst %.$(SRCEXT),$(OBJDIR)/%.o,$(SRCS))
DEPS    := $(OBJS:.o=.d)

DEBUG    = -g
INCLUDES = -I./inc -I/usr/local/include -I/usr/local
CFLAGS   = -std=c++11 -O3 -c $(DEBUG) $(INCLUDES)
LDFLAGS  = -stdlib=libstdc++ -pthread -L/usr/local/lib -lv8_base -lv8_libbase -lv8_snapshot -lv8_libplatform

ifneq ($(strip $(SRCDIRS)),)
  $(call,make-repo)
endif

ifeq ($(SRCEXT), cc)
CC       = clang++
else
CFLAGS  += -std=gnu99
endif

.PHONY: all clean distclean

all:  $(BINDIR)/$(APP)

# $(COMMON)/libcommon.a:
	# cd $(COMMON) && make

# .PHONY: common
# common: $(COMMON)/libcommon.a

$(BINDIR)/$(APP): .dep $(OBJS)
	@mkdir -p `dirname $@`
	@echo "Linking $@..."
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(OBJDIR)/%.o: %.$(SRCEXT)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -MMD -MP $< -o $@

clean:
	$(RM) -rf $(BINDIR)/$(APP) $(OBJDIR) .dep

distclean: clean
	$(RM) -r $(BINDIR)

.dep:
	@$(call make-repo)
	@touch .dep

define make-repo
   for dir in $(SRCDIRS); \
   do \
	mkdir -p $(OBJDIR)/$$dir; \
   done
endef


# usage: $(call make-depend,source-file,object-file,depend-file)
define make-depend
  $(CC) -MM       \
        -MF $3    \
        -MP       \
        -MT $2    \
        $(CFLAGS) \
        $1
endef

-include $(DEPS)
