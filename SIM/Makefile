CC               = gcc
SOURCEDIR	 = ./ 
INCLUDEDIR       = ../FEE
BUILDDIR         = ./
PATH            +=
CFLAGS          += -O2 -W -Wall -Wextra #-Wno-unused #-Werror -pedantic
CPPFLAGS        := -I$(INCLUDEDIR) -I../GRESB #-DDEBUG
LDFLAGS         := -lpthread
SOURCES         := $(wildcard *.c) ../GRESB/gresb.c ../FEE/*.c
OBJECTS         := $(patsubst %.c, $(BUILDDIR)/%.o, $(subst $(SOURCEDIR)/,, $(SOURCES)))
TARGET          := fee_sim

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
