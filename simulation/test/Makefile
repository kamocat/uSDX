SRC=../src

all: build run plot

build:
	g++ test.cpp metrology.cpp hilbert.cpp -ggdb -o filt.out

run:
	./filt.out
	
plot:
	gnuplot plot_settings
