ifeq ($(MIX_APP_PATH),)
calling_from_make:
	mix compile
endif

ifeq ($(CROSSCOMPILE),)
    ifeq ($(shell uname -s),Linux)
        DEPS_HOME ?= ./extra
        LIB_EXT    = -lpthread -ldl
        TFL_GEN    = linux_x86_64
    else
        DEPS_HOME ?= C:/msys64/home/work
        LIB_EXT    = -lmman
        TFL_GEN    = windows_x86_64
        INC_EXT    = -I$(DEPS_HOME)/CImg-2.9.2
    endif
else
    DEPS_HOME ?= ./extra
    TFL_GEN    = nerves_armv6
    INC_EXT    = -I$(DEPS_HOME)/usr/include -I$(DEPS_HOME)/usr/include/arm-linux-gnueabi
    LDFLAGS    = -L$(DEPS_HOME)/usr/lib/arm-linux-gnueabi -lpthread -ldl -latomic
endif

INCLUDE   = -I./src \
            -I$(DEPS_HOME)/tensorflow_src \
            -I$(DEPS_HOME)/tensorflow_src/tensorflow/lite/tools/make/downloads/flatbuffers/include \
            $(INC_EXT)
DEFINES   = #-D__LITTLE_ENDIAN__ -DTFLITE_WITHOUT_XNNPACK
CXXFLAGS += -O3 -DNDEBUG -fPIC --std=c++11 -fext-numeric-literals $(INCLUDE) $(DEFINES)
LDFLAGS  += $(LIB_EXT) -ljpeg

LIB_TFL = $(DEPS_HOME)/tensorflow_src/tensorflow/lite/tools/make/gen/$(TFL_GEN)/lib/libtensorflow-lite.a

SRC=$(wildcard src/*.cc)
OBJ=$(SRC:src/%.cc=obj/%.o)



all: obj priv install

install: priv/tfl_interp

obj/%.o: src/%.cc
	$(CXX) -c $(CXXFLAGS) -o $@ $<

priv/tfl_interp: $(OBJ)
	$(CXX) $^ $(LIB_TFL) $(LDFLAGS) -o $@

clean:
	rm -f priv/tfl_interp $(OBJ)

priv obj:
	mkdir -p $@

print-vars:
	@$(foreach v,$(.VARIABLES),$(info $v=$($v)))

.PHONY: all clean calling_from_make install print-vars
