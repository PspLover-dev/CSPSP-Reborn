PSPDEV ?= /Users/Camille/pspdev
export PSPDEV
export PATH := $(PSPDEV)/bin:$(PATH)

.PHONY: all clean rebuild maps

all:
	mkdir -p build
	cd build && psp-cmake -DCMAKE_BUILD_TYPE=Release .. && $(MAKE)

maps:
	python3 tools/generate_maps.py

clean:
	rm -rf build

rebuild: clean all
