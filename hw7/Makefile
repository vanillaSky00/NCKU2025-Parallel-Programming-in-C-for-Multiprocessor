CC = gcc
CXX = g++
CFLAGS := -std=c++17 -O3 -Wall -march=native -Wextra -Wshadow -lm -pthread \
             -funroll-loops -ffast-math -flto -fno-signed-zeros \
             -fno-trapping-math -fassociative-math -freciprocal-math \
             -mprefer-vector-width=512 \
             -DOMPI_SKIP_MPICXX -DMPICH_SKIP_MPICXX  

EXEC = hw7
SRC_C = main.c
SRC_CXX = main.cpp

ifneq ($(wildcard $(SRC_CXX)),)
	SRC = $(SRC_CXX)
	COMPILER = $(CXX)
	EXT = cpp
else ifneq ($(wildcard $(SRC_C)),)
	SRC = $(SRC_C)
	COMPILER = $(CC)
	EXT = c
else
	$(error No main.c or main.cpp found)
endif

.PHONY: all clean compile run

all: compile

compile: $(EXEC)

run: $(EXEC)
	./$(EXEC)

$(EXEC): $(SRC)
	$(COMPILER) -o $(EXEC) $(SRC) $(CFLAGS)

clean:
	rm -f $(EXEC)