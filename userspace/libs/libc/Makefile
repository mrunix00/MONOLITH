LIB_NAME := libc

LIB_OPTFLAGS := -O3

include ../common-lib.mk

CRT_SRC_DIR := src/crt
CRT_OBJECTS := $(LIBS_OUTPUT_DIR)/crt0.o $(LIBS_OUTPUT_DIR)/crt1.o \
	$(LIBS_OUTPUT_DIR)/crti.o $(LIBS_OUTPUT_DIR)/crtn.o
EXTRA_CLEAN += $(CRT_OBJECTS)

all: $(CRT_OBJECTS)

$(LIBS_OUTPUT_DIR)/crt0.o $(LIBS_OUTPUT_DIR)/crt1.o: $(CRT_SRC_DIR)/crt0.c | $(LIBS_OUTPUT_DIR)
	$(CC) $(LIB_CFLAGS) -c $< -o $@

$(LIBS_OUTPUT_DIR)/crti.o: $(CRT_SRC_DIR)/crti.asm | $(LIBS_OUTPUT_DIR)
	nasm -f elf64 $< -o $@

$(LIBS_OUTPUT_DIR)/crtn.o: $(CRT_SRC_DIR)/crtn.asm | $(LIBS_OUTPUT_DIR)
	nasm -f elf64 $< -o $@

$(LIBS_OUTPUT_DIR):
	@mkdir -p $(LIBS_OUTPUT_DIR)
