CC= gcc
CFLAGS= -g -Wall
LDLIBS= -lpthread
INCLUDE= -I ./utils/
MAIN= farm
FILE= generafile

#eseguibile da compilare
all:
	$(CC) $(CFLAGS) $(INCLUDE) boundedqueue.c $(MAIN).c -o $(MAIN) $(LDLIBS) 
	$(CC) $(CFLAGS) $(FILE).c -o $(FILE) 

farm:
	$(CC) $(INCLUDE) boundedqueue.c $(MAIN).c -o $(MAIN) $(LDLIBS)

genera:
	$(CC) $(FILE).c -o $(FILE)

# target che cancella eseguibili e file oggetto
rm:
	rm farm >/dev/null
	rm generafile >/dev/null
s:
	rm farm.sck
	
#genera file txt
file1: $(FILE)
	./$(FILE) file1.dat 10
file2: $(FILE)
	./$(FILE) file2.dat 5
file3: $(FILE)
	./$(FILE) file3.dat 8

test:
	./test.sh

test1: $(MAIN)
	./$(MAIN) -n ciao file1.dat file2.dat file3.dat

test2: $(MAIN)
	./$(MAIN) -n 5 -q 5 -t 5 file100.dat file2.dat file1.dat

test3: $(MAIN)
	./$(MAIN) -n 2 -q 1 file100.dat file2.dat file1.dat file150.dat file116.dat

