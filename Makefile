CXX      = gcc
#CPPFLAGS =
CXXFLAGS = -Wall -Werror
#LDFLAGS  =
#LDLIBS1  =
#LDLIBS2  =

EXEC = mod-v6

SRCS  = $(wildcard *.c)
OBJS := $(patsubst %.c,%.o,$(SRCS))
DEP  := $(patsubst %.c,%.d,$(SRCS))


.PHONY: all clean


all: $(EXEC)


clean:
	rm -f $(OBJS) $(DEP) $(EXEC)


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
