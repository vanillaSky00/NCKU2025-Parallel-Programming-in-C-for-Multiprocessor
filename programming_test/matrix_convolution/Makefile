MPICC = mpicc
MPICXX = mpic++
CFLAGS = -Wall -O1 -Wextra -Wshadow -lm

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

