CC = arm-linux-gnueabi-gcc
CFLAGS=-I ../include
OBJS = test_ptree.o
TARGET = test

.SUFFIXES : .c .o

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

clean :
	rm -f $(TARGET)
	rm -f *.o
