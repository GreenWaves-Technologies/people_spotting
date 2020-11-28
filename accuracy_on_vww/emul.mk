# Copyright (C) 2020 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

include ../common.mk

QUANT_BITS = 8
MODEL_SQ8=1
MODEL_SUFFIX=_$(QUANT_BITS)BIT_EMUL

$(info Building emulation mode with 8 bit quantization)

# The training of the model is slightly different depending on
# the quantization. This is because in 8 bit mode we used signed
# 8 bit so the input to the model needs to be shifted 1 bit

NNTOOL_SCRIPT=../model/nntool_script
TRAINED_TFLITE_MODEL=../model/visual_wake_quant.tflite
NNTOOL_EXTRA_FLAGS= -q

include ../model_decl.mk
IMAGES=../images/

MAIN=$(MODEL_PREFIX)_emul.c

MODEL_GEN_EXTRA_FLAGS= -f $(MODEL_BUILD)
CC = gcc
OPTIMIZATION?=-O3
CFLAGS += -g -m32 $(OPTIMIZATION) -D__EMUL__ -DAT_MODEL_PREFIX=$(MODEL_PREFIX) $(MODEL_SIZE_CFLAGS) -DPERF
INCLUDES +=   -I../ -I$(TILER_EMU_INC) -I$(TILER_INC)  # -I$(TILER_CNN_GENERATOR_PATH) -I$(TILER_CNN_KERNEL_PATH) -I$(TILER_CNN_KERNEL_PATH_SQ8)
INCLUDES += $(CNN_LIB_INCLUDE) -I$(MODEL_COMMON_INC) -I$../$(MODEL_BUILD) #-I../$(MODEL_HEADERS) $(CNN_GEN_INCLUDE) -I/home/manuele/gap_sdk/libs/gap_lib/include 
LFLAGS =
LIBS =
SRCS = $(MAIN) $(MODEL_COMMON_SRCS) $(MODEL_SRCS) $(CNN_LIB) $(MODEL_GEN_C)

$(info CNN_LIB++ $(CNN_LIB))
$(info SRCS++ $(SRCS))

#CFLAGS += -DAT_CONSTRUCT=$(AT_CONSTRUCT) -DAT_DESTRUCT=$(AT_DESTRUCT) -DAT_CNN=$(AT_CNN) -DAT_L3_ADDR=$(AT_L3_ADDR)
#INCLUDES = -I../ -I$(TILER_EMU_INC) -I$(TILER_INC) 
#LFLAGS =
#LIBS =
#SRCS += $(MAIN) $(MODEL_GEN_C) $(MODEL_COMMON_SRCS) $(CNN_LIB)

BUILD_DIR=BUILD_EMUL
OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))
EXE=$(MODEL_PREFIX)_emul

# Here we set the memory allocation for the generated kernels
# REMEMBER THAT THE L1 MEMORY ALLOCATION MUST INCLUDE SPACE
# FOR ALLOCATED STACKS!
MODEL_L1_MEMORY=52000
MODEL_L2_MEMORY=307200
MODEL_L3_MEMORY=8388608
# hram - HyperBus RAM
# qspiram - Quad SPI RAM
MODEL_L3_EXEC=hram
# hflash - HyperBus Flash
# qpsiflash - Quad SPI Flash
MODEL_L3_CONST=hflash

all: model $(EXE)

$(OBJS) : $(BUILD_DIR)/%.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< $(INCLUDES) -MD -MF $(basename $@).d -o $@

$(EXE): $(OBJS)
	$(CC) $(CFLAGS) -MMD -MP $(CFLAGS) $(INCLUDES) -o $(EXE) $(OBJS) $(LFLAGS) $(LIBS)

clean: clean_model
	$(RM) -r $(BUILD_DIR)
	$(RM) $(EXE)

.PHONY: depend clean

include ../model_rules.mk
