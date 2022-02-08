CC               = gcc
SOURCEDIR	 = ./ 
INCLUDEDIR       = ./
BUILDDIR         = ./
PATH            +=
CFLAGS          := -O2 -W -Wall -Wextra #-Wno-unused #-Werror -pedantic 
CPPFLAGS        := -I$(INCLUDEDIR) -I../FEE -DDEBUG
LDFLAGS         := -lpthread
SOURCES         := $(wildcard *.c) ../FEE/rmap.c
OBJECTS         := $(patsubst %.c, $(BUILDDIR)/%.o, $(subst $(SOURCEDIR)/,, $(SOURCES)))
TARGET          := gresb_bridge 


all: $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $(TARGET)

clean:
	rm -f $(TARGET)
