CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2
TARGET = memoire
SOURCE = memoire.c
MANPAGE= memoire.1
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man/man1

.PHONY: all clean install uninstall

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -d $(BINDIR)
	install -m 755 $(TARGET) $(BINDIR)/$(TARGET)
	install -d $(MANDIR)
	install -m 644 $(MANPAGE) $(MANDIR)/$(MANPAGE)
	@echo "$(TARGET) installed to $(BINDIR)"
	@echo "You can now run 'memoire' from anywhere"
	@echo "Manual page installed to $(MANDIR)"
	@echo "Config will be stored in ~/.config/memoire/data.txt"

uninstall:
	rm -f $(BINDIR)/$(TARGET)
	rm -f $(MANDIR)/$(MANPAGE)
	@echo "$(TARGET) uninstalled from $(BINDIR)"
	@echo "Manual page removed from $(MANDIR)"
