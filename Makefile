.c.o:
	gcc -g -c $?

all: proxy

proxy: proxy.o confutils.o
	gcc -g -o proxy proxy.o confutils.o -lpthread

clean:
	rm -f *.o proxy
