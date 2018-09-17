PROGRAM=websockchat
PROGRAM_AUTH=auth
SRC_DIR=src
CFLAGS=-Wall -pedantic -O0 -g -ggdb3 -lsea
OBJECTS=$(SRC_DIR)/main.o $(SRC_DIR)/users.o $(SRC_DIR)/commands.o

default: websockchat auth

websockchat: $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(shell pkg-config --cflags --libs openssl) -o $(PROGRAM)

auth.o: $(SRC_DIR)/auth.c
	$(CC) -c $(CFLAGS) $(SRC_DIR)/auth.c -o $(SRC_DIR)/$@

main.o: $(SRC_DIR)/main.c
	$(CC) -c $(CFLAGS) $(SRC_DIR)/main.c -o $(SRC_DIR)/$@

users.o: $(SRC_DIR)/users.c
	$(CC) -c $(CFLAGS) $(SRC_DIR)/users.c -o $(SRC_DIR)/$@

commands.o: $(SRC_DIR)/commands.o
	$(CC) -c $(CFLAGS) $(SRC_DIR)/commands.c -o $(SRC_DIR)/$@

auth:
	$(CC) $(CFLAGS) $(shell pkg-config --cflags --libs openssl) -pthread $(SRC_DIR)/auth_main.c -o $(PROGRAM_AUTH)

clean:
	-rm $(PROGRAM)
	-rm $(OBJECTS)
	-rm $(PROGRAM_AUTH)
