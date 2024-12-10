all: setup-dirs libcommon.a
.PHONY: clean setup-dirs

CFLAGS= -O3 -std=gnu11 -Wall -Wextra  -Werror

OBJ_DIR = ./build/obj
BIN_DIR = ./build/binaries
LIB_DIR = ./build/libs

INCS = $(wildcard *.h $(foreach fd, $(SUBDIR), $(fd)/*.h))
SRCS = $(wildcard *.c $(foreach fd, $(SUBDIR), $(fd)/*.c))

INCLUDES = -I ../alias-reversing/modules/read_alias/include -I$(KERNEL_PATH_UAPI)/include/

LIBCOMMON_OBJS=$(OBJ_DIR)/helpers.o  $(OBJ_DIR)/mem_range_repo.o $(OBJ_DIR)/proc_iomem_parser.o $(OBJ_DIR)/parse_pagemap.o 
ifndef KERNEL_PATH_UAPI 
$(info "KERNEL_PATH_UAPI env var not defined. Not building GPA2HPA functionality. Point this env var to the uapi headers exported while building the kvm module with the gpa2hpa patches")
else
$(info "Building with GPA2HPA features")
	LIBCOMMON_OBJS += $(OBJ_DIR)/kvm_ioctls.o
endif

#create output directores for build stuff
setup-dirs:
	mkdir -p $(OBJ_DIR)
	mkdir -p $(BIN_DIR)
	mkdir -p $(LIB_DIR)

#build all objects files
$(OBJ_DIR)/%.o: %.c $(INCS)
	gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<


libcommon.a : $(LIBCOMMON_OBJS)
	ar rcs $(LIB_DIR)/libcommon.a $^


clean:
	rm -rf ./build
