# List of all the board related files.
BOARDSRC = $(CHIBIOS)/os/hal/boards/uSDX SMD/board.c

# Required include directories
BOARDINC = $(CHIBIOS)/os/hal/boards/uSDX SMD

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)
