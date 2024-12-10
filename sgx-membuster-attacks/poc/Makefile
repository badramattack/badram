ifndef LIBSGXSTEP_DIR
$(error LIBSGXSTEP_DIR is not set)
endif

LIBSGXSTEP           = $(LIBSGXSTEP_DIR)/libsgxstep
-include $(LIBSGXSTEP)/Makefile.config

OBJ_DIR = ./build/obj
BIN_DIR = ./build/binaries

#libcommon.a
LIBCOMMON= ../../common-code/
#lib kmod_read_alias.a
LIBKRA = ../../alias-reversing/modules/read_alias/

ifeq ($(SGX_SDK),)
    SGX_SDK          = /opt/intel/sgxsdk
endif
export SGX_SDK
ifneq ($(SGX_SDK), /opt/intel/sgxsdk)
    URTS_LD_LIBRARY_PATH = LD_LIBRARY_PATH=$(LIBSGXSTEP_DIR)/linux-sgx/psw/urts/linux
endif

ENCLAVE              = Enclave
SUBDIRS              = $(ENCLAVE) $(LIBSGXSTEP)

CC                   = gcc
AS                   = gcc
LD                   = gcc

ifeq ($(M32), 1)
	ASFLAGS  = -m32 -DM32=$(M32)
	CFLAGS   = -m32 -DM32=$(M32)
	LDFLAGS  = -m32
else
	LIB_SUFX = 64
endif

CFLAGS              += -fPIC -fno-stack-protector -fno-builtin -fno-jump-tables \
                       -fno-common -Wno-attributes -g -D_GNU_SOURCE -O0
INCLUDE              = -I$(SGX_SDK)/include/ -I$(LIBSGXSTEP_DIR) \
											 -I$(LIBCOMMON)/include -I$(LIBKRA)/include
LDFLAGS             += -lsgx-step -lencl_proxy -lsgx_urts \
                       -lsgx_uae_service -lcommon -lkmodreadalias \
											 -pthread $(SUBDIRS:%=-L %) \
											 -L$(SGX_SDK)/lib$(LIB_SUFX)/ \
                       -L$(LIBSGXSTEP_DIR)/linux-sgx/psw/urts/linux \
											 -L$(LIBCOMMON)/build/libs -L$(LIBKRA)


SOURCES              = $(shell ls *.c)
OBJECTS              = $(SOURCES:.c=.o)
OUTPUT               = $(BIN_DIR)/app

BUILDDIRS            = $(SUBDIRS:%=build-%)
CLEANDIRS            = $(SUBDIRS:%=clean-%)

CFLAGS              += -DATTACK_SCENARIO=$(ATTACK) -DNUM_RUNS=$(NUM)

MAKEFLAGS           += --silent

all: setup-dirs $(LIBCOMMON)/build/libs/libcommon.a $(LIBKRA)/libkmodreadalias.a $(OUTPUT)

setup-dirs:
	echo "$(INDENT)[===] $(@:build-%=%) [===]"
	mkdir -p $(OBJ_DIR)
	mkdir -p $(BIN_DIR)

$(LIBCOMMON)/build/libs/libcommon.a:
	echo "$(INDENT)[===] $(@:build-%=%) [===]"
	cd $(LIBCOMMON) && make all

$(LIBKRA)/libkmodreadalias.a:
	echo "$(INDENT)[===] $(@:build-%=%) [===]"
	cd $(LIBKRA) && make all

rmmod:
	echo "Removing kernel module"
	sudo rmmod kmod_readalias &> /dev/null || true

insmod:
	echo "Inserting kernel module"
	sudo insmod $(LIBKRA)/kmod_readalias.ko
	
run: rmmod clean all insmod
	sudo $(URTS_LD_LIBRARY_PATH) ./build/binaries/app ./aliases.csv > out.txt
	cat out.txt

$(OUTPUT): $(BUILDDIRS) $(OBJECTS)
	echo "$(INDENT)[LD]" $(OBJECTS) $(LIBS) -o $(OUTPUT) 
	$(LD) $(OBJECTS) $(LDFLAGS) -o $(OUTPUT) 

%.o : %.c
	echo "$(INDENT)[CC] " $<
	$(CC) $(CFLAGS) $(INCLUDE) -c $<

%.o : %.S
	echo "$(INDENT)[AS] " $<
	$(AS) $(ASFLAGS) $(INCLUDE) -c $< -o $@

clean: $(CLEANDIRS)
	echo "$(INDENT)[RM]" $(OBJECTS) $(OUTPUT)
	rm -f $(OBJECTS) $(OUTPUT)
	cd $(LIBCOMMON) && make clean
	cd $(LIBKRA) && make clean

$(BUILDDIRS):
	echo "$(INDENT)[===] $(@:build-%=%) [===]"
	$(MAKE) -C $(@:build-%=%) INDENT+="$(INDENT_STEP)" M32=$(M32) curr-dir=$(curr-dir)/$(@:build-%=%)

$(CLEANDIRS):
	echo "$(INDENT)[===] $(@:clean-%=%) [===]"
	$(MAKE) clean -C $(@:clean-%=%) INDENT+="$(INDENT_STEP)" curr-dir=$(curr-dir)/$(@:build-%=%)
