# Copyright (C) 2020 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

$(info Building emulation mode with 8 bit quantization)

include common.mk
include model_decl.mk
NNTOOL_SCRIPT := $(NNTOOL_SCRIPT)_emul
MODEL_SUFFIX := $(MODEL_SUFFIX)_EMUL

MODEL_GEN_EXTRA_FLAGS = -f $(MODEL_BUILD)
CC = gcc
CFLAGS += -g -O0 -D__EMUL__ -DAT_MODEL_PREFIX=$(MODEL_PREFIX) $(MODEL_SIZE_CFLAGS) 
INCLUDES = -I. -I$(TILER_EMU_INC) -I$(TILER_INC) -I$(TILER_CNN_GENERATOR_PATH) -I$(TILER_CNN_KERNEL_PATH) -I$(MODEL_BUILD) -I$(MODEL_COMMON_INC) $(CNN_LIB_INCLUDE)
LFLAGS =
LIBS = -lm
SRCS = vww_model.c $(MODEL_COMMON_SRCS) $(MODEL_GEN_C) $(CNN_LIB)

BUILD_DIR = BUILD_EMUL

OBJS = $(patsubst %.c, $(BUILD_DIR)/%.o, $(SRCS))

MAIN = $(MODEL_PREFIX)_emul

all: model $(MAIN)

$(OBJS) : $(BUILD_DIR)/%.o : %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< $(INCLUDES) -MD -MF $(basename $@).d -o $@

$(MAIN): $(OBJS)
	$(CC) $(CFLAGS) -MMD -MP $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

clean: 
	rm -rf $(BUILD_DIR)
	$(RM) $(MAIN)

.PHONY: depend clean

include model_rules.mk
