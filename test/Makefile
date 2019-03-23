CC = arm-linux-gnueabi-gcc
OBJS = test_ptree.c
TARGET = test.o

.SUFFIXES : .c .o

all : $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -I ../include $(OBJS) -o $(TARGET)

clean :
	rm -f $(TARGET)
