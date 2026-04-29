CC = gcc
CFLAGS = -Wall -Wextra -std=c11
LIBS = -lcurl -lpthread

TARGET = scheduler_os
SRC = scheduler_os.c api.c scheduler.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)