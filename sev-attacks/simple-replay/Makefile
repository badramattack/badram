
CFLAGS= -O3 -std=gnu11 -Wall -Wextra -Wpedantic -Werror

OBJ_DIR = ./build/obj
BIN_DIR = ./build/binaries

INCS = $(wildcard *.h $(foreach fd, $(SUBDIR), $(fd)/*.h))
SRCS = $(wildcard *.c $(foreach fd, $(SUBDIR), $(fd)/*.c))

#libcommon.a
LIBCOMMON= ../../common-code/
#lib kmod_read_alias.a
LIBKRA = ../../alias-reversing/modules/read_alias/


INCLUDES=  -I$(LIBCOMMON)/include -I$(LIBKRA)/include LIBS = -L$(LIBCOMMON)/build/libs -L$(LIBKRA)

all: setup-dirs $(BIN_DIR)/badram-vm-victim $(BIN_DIR)/badram-sev-replay
.PHONY: clean setup-dirs

#create output directores for build stuff
setup-dirs:
	printf "\n###\nCreating Build dirs\n###\n"
	mkdir -p $(OBJ_DIR)
	mkdir -p $(BIN_DIR)

$(LIBCOMMON)/build/libs/libcommon.a:
	printf "\n###\nBuilding libcommon.a\n###\n"
	cd $(LIBCOMMON) && make all

$(LIBKRA)/libkmodreadalias.a:
	printf "\n###\nBuilding libkmodreadalias.a\n###\n"
	cd $(LIBKRA) && make all

#build all objects files in this folder
$(OBJ_DIR)/%.o: %.c $(INCS)
	gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<

$(BIN_DIR)/badram-sev-replay: $(OBJ_DIR)/badram_sev_replay.o $(OBJ_DIR)/qemu_gpa2hpa.o $(LIBCOMMON)/build/libs/libcommon.a $(LIBKRA)/libkmodreadalias.a
	printf "\n###\nBuilding badram-sev-replay\n###\n"
	gcc $(INCLUDES) $(LIBS) $(CFLAGS) -o $(BIN_DIR)/badram-sev-replay $^ -lcommon -lkmodreadalias

$(BIN_DIR)/badram-vm-victim: $(OBJ_DIR)/badram_vm_victim.o $(LIBCOMMON)/build/libs/libcommon.a 
	printf "\n###\nBuilding badram-vm-victim\n###\n"
	gcc $(INCLUDES) -L$(LIBCOMMON)/build/libs $(CFLAGS) -o $(BIN_DIR)/badram-vm-victim $^ -lcommon


clean:
	rm -rf ./build
	cd $(LIBCOMMON) && make clean
	cd $(LIBKRA) && make clean
