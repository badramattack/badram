
CFLAGS= -O3 -std=gnu11 -Wall -Wextra -Wpedantic -Werror

OBJ_DIR = ./build/obj
BIN_DIR = ./build/binaries

INCS = $(wildcard *.h $(foreach fd, $(SUBDIR), $(fd)/*.h))
SRCS = $(wildcard *.c $(foreach fd, $(SUBDIR), $(fd)/*.c))

#libcommon.a
LIBCOMMON= ../../../common-code/
#lib kmod_read_alias.a
LIBKRA = ../../modules/read_alias/


INCLUDES=  -I../../../common-code/include -I$(LIBKRA)/include
LIBS = -L$(LIBCOMMON)/build/libs -L$(LIBKRA)


all: setup-dirs $(BIN_DIR)/test-aliases
.PHONY: clean setup-dirs

#create output directores for build stuff
setup-dirs:
	mkdir -p $(OBJ_DIR)
	mkdir -p $(BIN_DIR)

$(LIBCOMMON)/build/libs/libcommon.a:
	echo "Building libcommon.a"
	cd $(LIBCOMMON) && make all

$(LIBKRA)/libkmodreadalias.a:
	echo "Building libkmodreadalias.a"
	cd $(LIBKRA) && make all

#build all objects files in this folder
$(OBJ_DIR)/%.o: %.c $(INCS)
	gcc $(CFLAGS) $(INCLUDES) -o $@ -c $<


$(BIN_DIR)/test-aliases : $(OBJ_DIR)/test_aliases.o $(LIBCOMMON)/build/libs/libcommon.a $(LIBKRA)/libkmodreadalias.a
	echo "Building test-aliases"
	gcc $(INCLUDES) $(LIBS) $(CFLAGS) -o $(BIN_DIR)/test-aliases $^ -lcommon -lkmodreadalias


clean:
	rm -rf ./build
	cd $(LIBCOMMON) && make clean
	cd $(LIBKRA) && make clean
