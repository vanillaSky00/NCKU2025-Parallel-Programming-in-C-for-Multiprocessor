CC = gcc
CXX = g++
MPICC = mpicc
MPICXX = mpic++

EXEC = main
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

.PHONY: all clean judge

all: $(EXEC)

$(EXEC): $(SRC)
	$(COMPILER) -o $(EXEC) $(SRC)

judge: $(EXEC)
	@python3 score.py

clean:
	rm -f $(EXEC)

