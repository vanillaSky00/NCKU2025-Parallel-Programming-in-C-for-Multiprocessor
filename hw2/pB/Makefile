CXX = mpic++
CXXFLAGS = -std=c++17 -O2 -Wall
TARGET = main_mpi
SOURCE = main.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)


