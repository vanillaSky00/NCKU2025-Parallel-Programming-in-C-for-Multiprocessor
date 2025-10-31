CC := mpic++
CFLAGS := -Wall -O2
TARGET := hw3b

all: $(TARGET)

$(TARGET): main.cpp
	$(CC) -o $@ $^ $(CFLAGS)

judge:
	chmod +x judge.sh
	bash judge.sh

clean:
	rm -f $(TARGET)
	rm -rf output
