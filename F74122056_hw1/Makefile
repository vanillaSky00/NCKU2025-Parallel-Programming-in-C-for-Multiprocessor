CC := gcc
CFLAGS := -Wall
TARGET := hw1
STUDENT_ID := F74122056
v := 1

all: $(TARGET)

$(TARGET): $(STUDENT_ID)_$(TARGET).c
	$(CC) -o $@ $^

judge: all
	@judge -v ${v} || printf "or \`make judge v=1\`"

clean:
	rm -f $(TARGET)
	rm -rf output
