CC=gcc
CFLAGS=-Wall -Wextra -Iinclude -g

SRC_DIR=src
BUILD_DIR=build

SOURCES= $(wildcard $(SRC_DIR)/*.c)
OBJECTS= $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
TARGET= $(BUILD_DIR)/zyphie

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

	$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	    if not exist $(BUILD_DIR); mkdir $(BUILD_DIR)
			$(CC) $(CFLAGS) -c $< -o $@ 

clean:
    rmdir /S /Q $(BUILD_DIR)