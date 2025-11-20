CC := mpic++
CFLAGS := -std=c++17 -O3 -march=skylake-avx512 -mtune=skylake-avx512 \
             -funroll-loops -ffast-math -flto -fno-signed-zeros \
             -fno-trapping-math -fassociative-math -freciprocal-math \
             -mprefer-vector-width=512 \
             -DOMPI_SKIP_MPICXX -DMPICH_SKIP_MPICXX -Wall
TARGET := hw3b

all: $(TARGET)

$(TARGET): counting_sort.cpp
	$(CC) -o $@ $^ $(CFLAGS)

judge:
	chmod +x judge.sh
	bash judge.sh

clean:
	rm -f $(TARGET)
	rm -rf output
