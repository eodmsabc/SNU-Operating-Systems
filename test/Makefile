CC = arm-linux-gnueabi-gcc
CFLAGS=-I ../include
OBJS = rotd.o
TARGET = rotd

.SUFFIXES : .c .o

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS)

clean :
	rm -f $(TARGET)
	rm -f *.o
