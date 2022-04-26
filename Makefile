CXX       = gcc
#CPPFLAGS =
CXXFLAGS  = -Wall -Werror
#LDFLAGS  =
LDLIBS    = -lm

EXEC = mod-v6
TAR  = proj-2-2.tar

SRCS  = $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))
DEP  := $(patsubst %.c,%.d,$(SRCS))


.PHONY: all clean tarball


all: $(EXEC)


clean:
	rm -f $(OBJS) $(DEP) $(EXEC)

tarball:
	rm -f $(TAR); \
	tar cfv $(TAR) $(SRCS) Makefile README.md


%.d:%.c
	@echo Creating $@
	@set -e; rm -f $@; \
	$(CXX) -MM $(CPPFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$


$(EXEC): $(OBJS)
	$(CXX) $(LDFLAGS) -o $(EXEC) $(OBJS) $(LDLIBS)


Makefile: $(SRCS:.c=.d)


-include $(SRCS:.c=.d)
