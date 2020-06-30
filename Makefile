# Copyright (C) 2017 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

ifndef GAP_SDK_HOME
  $(error Source sourceme in gap_sdk first)
endif

include common.mk

IMAGE=$(CURDIR)/images/COCO_val2014_000000174838_1.ppm

## Mute printf in source code
SILENT=1

## Enable image grub from camera and disaply output to lcd
FROM_CAMERA=1

io=host

QUANT_BITS=8
BUILD_DIR=BUILD
MODEL_SQ8=1

$(info Building GAP8 mode with $(QUANT_BITS) bit quantization)

NNTOOL_SCRIPT=model/nntool_script
MODEL_SUFFIX = _SQ8BIT
TRAINED_TFLITE_MODEL=model/visual_wake_quant.tflite
#LOAD A TFLITE QUANTIZED GRAPH
NNTOOL_EXTRA_FLAGS= -q


include model_decl.mk

# Here we set the memory allocation for the generated kernels
# REMEMBER THAT THE L1 MEMORY ALLOCATION MUST INCLUDE SPACE
# FOR ALLOCATED STACKS!
#MAIN_STACK_SIZE=2048

CLUSTER_STACK_SIZE=4096
CLUSTER_SLAVE_STACK_SIZE=1024
#TOTAL_STACK_SIZE=$(shell expr $(CLUSTER_STACK_SIZE) \+ $(CLUSTER_SLAVE_STACK_SIZE) \* 7)
#MODEL_L1_MEMORY= $(shell expr 60000 \- $(TOTAL_STACK_SIZE))
MODEL_L1_MEMORY= 48000
MODEL_L2_MEMORY=200000
MODEL_L3_MEMORY=4194304
# hram - HyperBus RAM
# qspiram - Quad SPI RAM
MODEL_L3_EXEC=hram
# hflash - HyperBus Flash
# qpsiflash - Quad SPI Flash
MODEL_L3_CONST=hflash 

APP = vww
APP_SRCS += $(MODEL_PREFIX).c $(MODEL_GEN_C) $(MODEL_COMMON_SRCS) $(CNN_LIB)

APP_CFLAGS += -O3 -s -mno-memcpy -fno-tree-loop-distribute-patterns 
APP_CFLAGS += -I. -I$(MODEL_COMMON_INC) -I$(TILER_EMU_INC) -I$(TILER_INC) -I$(TILER_CNN_KERNEL_PATH) -I$(MODEL_BUILD)
APP_CFLAGS += -DAT_MODEL_PREFIX=$(MODEL_PREFIX) $(MODEL_SIZE_CFLAGS)
APP_CFLAGS += -DSTACK_SIZE=$(CLUSTER_STACK_SIZE) -DSLAVE_STACK_SIZE=$(CLUSTER_SLAVE_STACK_SIZE)
APP_CFLAGS += -I$(GAP_SDK_HOME)/libs/gap_lib/include/gaplib -I$(NNTOOL_KERNEL_PATH)
APP_CFLAGS += -DAT_IMAGE=$(IMAGE)
ifeq ($(SILENT),1)
  APP_CFLAGS += -DSILENT=1
endif
ifeq ($(FROM_CAMERA),1)
  APP_CFLAGS += -DFROM_CAMERA=1
endif

APP_LDFLAGS += -lgaplib

READFS_FILES=$(abspath $(MODEL_TENSORS))
PLPBRIDGE_FLAGS += -f

# all depends on the model
all:: model

clean:: clean_model

include model_rules.mk
include $(RULES_DIR)/pmsis_rules.mk

