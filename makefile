all: muzeum bank firma
clean:
	rm bank.o muzeum.o firma.o bank firma muzeum
muzeum.o: muzeum.c komunikacja.h
	gcc -Wall -std=gnu99 -pthread muzeum.c -c -o muzeum.o
bank.o: bank.c komunikacja.h
	gcc -Wall -std=gnu99 -pthread bank.c -c -o bank.o
firma.o: firma.c komunikacja.h
	gcc -Wall -std=gnu99 -pthread muzeum.c -c -o firma.o

muzeum: muzeum.o
	gcc -Wall -std=gnu99 -pthread muzeum.o -o muzeum
bank: bank.o
	gcc -Wall -std=gnu99 -pthread bank.c -o bank
firma: firma.o
	gcc -Wall -std=gnu99 -pthread firma.o -o firma

