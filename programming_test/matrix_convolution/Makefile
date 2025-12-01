MPICC = mpicc
MPICXX = mpic++
CFLAGS := -std=c++17 -O3 -march=skylake-avx512 -mtune=skylake-avx512 \
             -funroll-loops -ffast-math -flto -fno-signed-zeros \
             -fno-trapping-math -fassociative-math -freciprocal-math \
             -mprefer-vector-width=512 \
             -DOMPI_SKIP_MPICXX -DMPICH_SKIP_MPICXX -Wall

EXEC = pA
SRC_C = main.c
SRC_CXX = main.cpp

ifneq ($(wildcard $(SRC_CXX)),)
	SRC = $(SRC_CXX)
	COMPILER = $(MPICXX)
	EXT = cpp
else ifneq ($(wildcard $(SRC_C)),)
	SRC = $(SRC_C)
	COMPILER = $(MPICC)
	EXT = c
else
	$(error No main.c or main.cpp found)
endif

.PHONY: all clean

all: $(EXEC)

$(EXEC): $(SRC)
	$(COMPILER) -o $(EXEC) $(SRC) $(CFLAGS)

clean:
	rm -f $(EXEC)

