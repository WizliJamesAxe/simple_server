# Simple Makefile to build server

OUTPUT_FILE = server
SRC = main.c log.c server.c

BUILD_FLAGS = -Wall -Werror -Wformat-security -Wformat=2 \
              -Wtype-limits -Wuninitialized -Wpointer-arith \
              -Wno-format-nonliteral -Wextra -Wno-unused-parameter \
              -Wno-empty-body -Wconversion

release:
	gcc ${BUILD_FLAGS} ${SRC} -o ${OUTPUT_FILE} -O3 -s

debug:
	gcc ${BUILD_FLAGS} ${SRC} -o ${OUTPUT_FILE} -Og

clean:
	rm server