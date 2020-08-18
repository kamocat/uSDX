# List of all the board related files.
BOARDSRC = $(CHIBIOS)/os/hal/boards/uSDX/board.c

# Required include directories
BOARDINC = $(CHIBIOS)/os/hal/boards/uSDX

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)
