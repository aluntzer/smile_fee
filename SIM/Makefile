CC               = gcc
SOURCEDIR	 = ./
INCLUDES         = -I../FEE -I./ -I../GRESB
BUILDDIR         = ./
PATH            +=
CFLAGS          += -O2 -W -Wall -Wextra #-Wno-unused #-Werror -pedantic
CPPFLAGS        := $(INCLUDES) -DFEE_SIM
LDFLAGS         := -lpthread -lm -lcfitsio
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
