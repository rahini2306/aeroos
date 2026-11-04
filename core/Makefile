CC = clang
CFLAGS = -std=c17 -O2 -Wall -Wextra -Wpedantic -Iinclude
SOURCES = $(wildcard src/*.c)
all: aeroos
aeroos: $(SOURCES) include/aero.h
	$(CC) $(CFLAGS) $(SOURCES) -pthread -o $@
clean:
	rm -f aeroos
test: aeroos
	$(CC) $(CFLAGS) ../tests/test_resources.c src/resource_manager.c src/flight.c src/json.c -o /tmp/aeroos-test-resources
	/tmp/aeroos-test-resources
sanitize:
	$(CC) -std=c17 -g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(SOURCES) -pthread -o aeroos-sanitize
.PHONY: all clean test sanitize
