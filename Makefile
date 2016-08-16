#The target's filename
TARGET=lirc-itest

#Directories of source files
SRC_DIRS = . led hsm msm nim sim lib mem cpu

#Directories of header files
INC_DIRS = -I. -I led -I hsm -I msm -I nim -I sim -I lib -I mem -I cpu

#Compile flags: directories of header files, warning option...
CFLAGS = -Wall -O $(INC_DIRS) #-g -DDEBUG

#Link flags: directories of libraries, ...
#LDFLAGS = -L.

#The list of libraries to link with
LDLIBS = -pthread


################################################################
SOURCE = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))
OBJS = $(patsubst %.c,%.o,$(SOURCE))
DEPS = $(patsubst %.o,%.d,$(OBJS))
MISSING_DEPS = $(filter-out $(wildcard $(DEPS)),$(DEPS))
MISSING_DEPS_SOURCES = $(wildcard $(patsubst %.d,%.c,$(MISSING_DEPS)))
CPPFLAGS += -MMD # Generate depends files


.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	@$(RM) $(TARGET) $(OBJS) $(DEPS) *~


ifneq ($(MISSING_DEPS),)
$(MISSING_DEPS):
	@$(RM) $(patsubst %.d,%.o,$@)
endif

-include $(DEPS)
