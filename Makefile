CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -Isrc
LDFLAGS =
TARGET  = visor.exe

SRCDIR  = src
SOURCES = $(SRCDIR)/main.c     \
          $(SRCDIR)/serial.c   \
          $(SRCDIR)/sensor_bar.c \
          $(SRCDIR)/diagnostic.c \
          $(SRCDIR)/ui.c       \
          $(SRCDIR)/config.c   \
          $(SRCDIR)/utils.c    \
          $(SRCDIR)/boards.c   \
          $(SRCDIR)/upload.c

OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
