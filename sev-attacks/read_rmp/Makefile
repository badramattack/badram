
CFLAGS= -O3 -std=gnu11 -Wall -Wextra -Wpedantic -Werror

OBJ_DIR = ./build/obj
BIN_DIR = ./build/binaries

INCS = $(wildcard *.h $(foreach fd, $(SUBDIR), $(fd)/*.h))
SRCS = $(wildcard *.c $(foreach fd, $(SUBDIR), $(fd)/*.c))

#libcommon.a
LIBCOMMON= ../../common-code/
#lib kmod_read_alias.a
LIBKRA = ../../alias-reversing/modules/read_alias/

INCLUDES=  -I$(LIBCOMMON)/include -I$(LIBKRA)/include
LIBS = -L$(LIBCOMMON)/build/libs -L$(LIBKRA)

DEPLOY_URI ?=

all: setup-dirs $(BIN_DIR)/read_rmp $(BIN_DIR)/badram-gpa-swap-victim $(BIN_DIR)/swap_attack
.PHONY: clean setup-dirs deploy

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

$(BIN_DIR)/read_rmp: $(OBJ_DIR)/read_rmp_main.o $(OBJ_DIR)/rmp.o $(LIBCOMMON)/build/libs/libcommon.a $(LIBKRA)/libkmodreadalias.a
	printf "\n\nn###\nBuilding read_rmp\n###\n\n"
	gcc $(INCLUDES) $(LIBS) $(CFLAGS) -o $(BIN_DIR)/read_rmp $^ -lcommon -lkmodreadalias

$(BIN_DIR)/badram-gpa-swap-victim: $(OBJ_DIR)/badram_gpa_swap_victim.o  $(LIBCOMMON)/build/libs/libcommon.a 
	printf "\n\n###\n Building badram-gpa-swap-victim\n###\n\n"
	gcc $(INCLUDES) $(LIBS) $(CFLAGS) -o $(BIN_DIR)/badram-gpa-swap-victim $^ -lcommon 

$(BIN_DIR)/swap_attack: $(OBJ_DIR)/swap_attack_main.o $(OBJ_DIR)/rmp.o $(LIBCOMMON)/build/libs/libcommon.a $(LIBKRA)/libkmodreadalias.a
	printf "\n\nn###\nBuilding swap_attack\n###\n\n"
	gcc $(INCLUDES) $(LIBS) $(CFLAGS) -o $(BIN_DIR)/swap_attack $^ -lcommon -lkmodreadalias
deploy:
	./deploy.sh

clean:
	rm -rf ./build
	cd $(LIBCOMMON) && make clean
	cd $(LIBKRA) && make clean
