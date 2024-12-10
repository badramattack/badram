all: setup-dirs fai
.PHONY: clean setup-dirs deploy

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


fai : $(OBJ_DIR)/find_alias_individual.o $(LIBCOMMON)/build/libs/libcommon.a $(LIBKRA)/libkmodreadalias.a
	echo "Building fai"
	gcc $(INCLUDES) $(LIBS) $(CFLAGS) -o $(BIN_DIR)/fai $^ -lcommon -lkmodreadalias

deploy:
	./deploy.sh


clean:
	rm -rf ./build
	cd $(LIBCOMMON) && make clean
	cd $(LIBKRA) && make clean
