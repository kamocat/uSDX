board=arduino:avr:nano:cpu=atmega328old
sketch=code
port=/dev/ttyACM0

all: build

build:
	arduino-cli compile -b $(board) $(sketch) --build-properties compiler.cpp.extra_flags=-Os 

.PHONY: docs
docs:
	doxygen $(sketch)/Doxyfile

clean:
	arduino-cli cache clean
	rm $(sketch)/*.bin
	rm $(sketch)/*.elf
