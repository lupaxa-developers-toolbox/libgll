# Static library build. The c skill syntax-checks and runs cppcheck.
# It does not compile or link.

override CFLAGS += -std=c11 -Wall -Wextra -Wpedantic -Werror -Iinclude
override CPPCHECK_FLAGS += -Iinclude --std=c11

GLL_BUILD := build
GLL_LIB := $(GLL_BUILD)/libgll.a
GLL_OBJ := $(GLL_BUILD)/gll.o
GLL_TEST := $(GLL_BUILD)/test_gll

.PHONY: lib test check clean

lib: $(GLL_LIB)

$(GLL_OBJ): src/gll.c include/gll.h
	mkdir -p $(GLL_BUILD)
	$(CC) $(CFLAGS) -c -o $@ src/gll.c

$(GLL_LIB): $(GLL_OBJ)
	$(AR) rcs $@ $(GLL_OBJ)

$(GLL_TEST): tests/test_gll.c include/gll.h $(GLL_LIB)
	$(CC) $(CFLAGS) -o $@ tests/test_gll.c $(GLL_LIB) $(LDFLAGS)

test: $(GLL_TEST)
	./$(GLL_TEST)

check: $(if $(filter c,$(SKILLS)),c-check) test

clean:
	rm -rf $(GLL_BUILD)
