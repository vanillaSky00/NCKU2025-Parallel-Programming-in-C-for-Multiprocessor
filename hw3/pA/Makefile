CC := mpic++
CFLAGS := -Wall -O3 -ftree-vectorize -march=native -lm
TARGET := hw3a

all: $(TARGET)

$(TARGET): main.cpp
	$(CC) -o $@ $^ $(CFLAGS)

judge:
	chmod +x judge.sh
	bash judge.sh

clean:
	rm -f $(TARGET)
	rm -rf output
