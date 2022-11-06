# Simple Makefile to build server

OUTPUT_FILE = server
SRC = main.c log.c server.c client.c

COMPILE_FLAGS = -Wall -Werror -Wformat-security -Wformat=2 \
                -Wtype-limits -Wuninitialized -Wpointer-arith \
                -Wno-format-nonliteral -Wextra -Wno-unused-parameter \
                -Wno-empty-body -Wconversion -fno-strict-aliasing

LINK_FLAGS = -lev

INCLUDE_DIRS = -I./include

release:
	gcc ${INCLUDE_DIRS} ${COMPILE_FLAGS} ${SRC} -o ${OUTPUT_FILE} ${LINK_FLAGS} -O3 -s

debug:
	gcc ${INCLUDE_DIRS} ${COMPILE_FLAGS} ${SRC} -o ${OUTPUT_FILE} ${LINK_FLAGS} -Og

clean:
	rm server