# List of all the board related files.
BOARDSRC = $(CHIBIOS)/os/hal/boards/BlackPill_F401/board.c

# Required include directories
BOARDINC = $(CHIBIOS)/os/hal/boards/BlackPill_F401

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)
