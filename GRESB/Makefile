CC               = gcc
SOURCEDIR	 = ./ 
INCLUDEDIR       = ./
BUILDDIR         = ./
PATH            +=
CFLAGS          := -O2 -W -Wall -Wextra #-Wno-unused #-Werror -pedantic 
CPPFLAGS        := -I$(INCLUDEDIR) -I../FEE
LDFLAGS         := -lpthread
SOURCES         := $(wildcard *.c) ../FEE/rmap.c
OBJECTS         := $(patsubst %.c, $(BUILDDIR)/%.o, $(subst $(SOURCEDIR)/,, $(SOURCES)))
TARGET          := gresb_bridge 

DEBUG?=1
ifeq  "$(shell expr $(DEBUG) \> 1)" "1"
	    CFLAGS += -DDEBUGLEVEL=$(DEBUG)
else
	    CFLAGS += -DDEBUGLEVEL=1
endif


all: $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ -o $(TARGET)

clean:
	rm -f $(TARGET)
