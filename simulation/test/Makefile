SRC=../src

all: build plot

build:
	g++ test.cpp hilbert.cpp -ggdb -o filt.out
	./filt.out
	
plot:
	gnuplot plot_settings

float:
	g++ test2.cpp ../ftoa.cpp -ggdb -o float.out
