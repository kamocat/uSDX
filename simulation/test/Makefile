SRC=../src

all: build run plot

build:
	g++ test.cpp metrology.cpp hilbert.cpp -DHILTEST -ggdb -o filt.out

peak:
	g++ test.cpp metrology.cpp -DPEAK -ggdb -o peak.out

phase:
	g++ test.cpp metrology.cpp -DPHASE -ggdb -o filt.out

run:
	./filt.out
	
plot:
	gnuplot plot_settings
