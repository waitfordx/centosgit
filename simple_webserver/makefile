myhttpd : wrap.o myhttpd.o
	gcc wrap.o myhttpd.o -Wall -std=gnu99 -o myhttpd

myhttpd.o : myhttpd.c
	gcc -c myhttpd.c -Wall -o myhttpd.o
	
wrap.o : wrap.c
	gcc -c wrap.c -Wall -o wrap.o


.phony:

clean:
	rm -rf myhttpd.o wrap.o myhttpd
