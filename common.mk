# Copyright (C) 2020 GreenWaves Technologies
# All rights reserved.

# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

MODEL_PREFIX=vww
AT_INPUT_WIDTH=238
AT_INPUT_HEIGHT=208
AT_INPUT_COLORS=3

BUILD_DIR=BUILD
MODEL_SQ8=1

NNTOOL_SCRIPT?=model/nntool_script
MODEL_SUFFIX = _SQ8
TRAINED_MODEL=model/visual_wake_quant.tflite
#LOAD A TFLITE QUANTIZED GRAPH
NNTOOL_EXTRA_FLAGS= -q
MODEL_EXPRESSIONS =BUILD_MODEL_SQ8/Expression_Kernels.c

# Here we set the memory allocation for the generated kernels
# REMEMBER THAT THE L1 MEMORY ALLOCATION MUST INCLUDE SPACE
# FOR ALLOCATED STACKS!
# FC stack size:
MAIN_STACK_SIZE=2048
# Cluster PE0 stack size:
CLUSTER_STACK_SIZE=4096
# Cluster PE1-PE7 stack size:
CLUSTER_SLAVE_STACK_SIZE=1024

ifeq '$(TARGET_CHIP_FAMILY)' 'GAP9'
  FREQ_CL?=50
  FREQ_FC?=50
  FREQ_PE?=50
  TARGET_L1_SIZE=128000
  TARGET_L2_SIZE=1300000
  TARGET_L3_SIZE=8388608
else
  ifeq '$(TARGET_CHIP)' 'GAP8_V3'
    FREQ_CL?=175
  else
    FREQ_CL?=50
  endif
  FREQ_FC?=250
  FREQ_PE?=250
  TARGET_L1_SIZE=64000
  TARGET_L2_SIZE=220000
  TARGET_L3_SIZE=8388608
endif

ifeq ($(MODEL_NE16), 1)
  NNTOOL_SCRIPT = model/nntool_script_ne16
  MODEL_SUFFIX = _NE16
endif
ifeq ($(MODEL_HWC), 1)
  NNTOOL_SCRIPT = model/nntool_script_hwc
  MODEL_SUFFIX = _HWC
  APP_CFLAGS += -DIMAGE_SUB_128
  CFLAGS += -DIMAGE_SUB_128
endif
