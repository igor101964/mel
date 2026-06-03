# mel — Terminal Editor · Art2Dec SoftLab · mshell Ecosystem
# GPL3 · v0.2.0 · Igor Lukyanov

CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -O2
LIBS    = -lcurl -ljson-c
TARGET  = mel
SRC     = mel.c

# macOS: Homebrew prefix (Intel: /usr/local, Apple Silicon: /opt/homebrew)
UNAME := $(shell uname)
ARCH  := $(shell uname -m)

ifeq ($(UNAME), Darwin)
    ifeq ($(ARCH), arm64)
        BREW_PREFIX = /opt/homebrew
    else
        BREW_PREFIX = /usr/local
    endif
    CFLAGS += -I$(BREW_PREFIX)/include
    LIBS   += -L$(BREW_PREFIX)/lib
endif

.PHONY: all debug install uninstall clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LIBS)

debug: $(SRC)
	$(CC) -std=c99 -Wall -Wextra -pedantic -g $(SRC) -o $(TARGET) $(LIBS)

install: $(TARGET)
	sudo cp $(TARGET) /usr/local/bin/
	sudo chmod +x /usr/local/bin/$(TARGET)
	@echo "mel installed to /usr/local/bin/mel"

uninstall:
	sudo rm -f /usr/local/bin/$(TARGET)
	sudo rm -f /usr/bin/$(TARGET)
	sudo rm -f /bin/$(TARGET)
	@echo "mel uninstalled"

clean:
	rm -f $(TARGET)
	@echo "cleaned"
