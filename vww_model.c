/*
 * Copyright (C) 2017 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include <stdio.h>

#ifndef __EMUL__
  /* PMSIS includes. */
  #include "pmsis.h"
#endif
#include "vww.h"
#include "vwwKernels.h"
#include "vwwInfo.h"
#include "gaplib/ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)
#define NUM_CLASSES 2

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;
int max_class;
int max_value;

// Softmax always outputs Q15 short int even from 8 bit input
L2_MEM short int *ResOut;
typedef unsigned char IMAGE_IN_T;
char *ImageName;
#ifdef __EMUL__
unsigned char * __restrict__ Input_1;
#endif

static void RunNetwork()
{
  printf("Running on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif

#ifdef __EMUL__
  __PREFIX(CNN)(Input_1, ResOut);
#else
    GPIO_HIGH();
  __PREFIX(CNN)(ResOut);
    GPIO_LOW();
#endif
  printf("Runner completed\n");
}

int start()
{
  #ifdef __EMUL__
    Input_1 = AT_L2_ALLOC(0, AT_INPUT_SIZE*sizeof(IMAGE_IN_T));
    if (Input_1==0){
      printf("error allocating %ld \n", AT_INPUT_SIZE*sizeof(IMAGE_IN_T));
      return 1;
    }
  #else
    OPEN_GPIO_MEAS();
    ImageName = __XSTR(AT_IMAGE);
    struct pi_device cluster_dev;
    struct pi_cluster_conf conf;
    pi_cluster_conf_init(&conf);
    conf.cc_stack_size = CLUSTER_STACK_SIZE;
    pi_open_from_conf(&cluster_dev, (void *)&conf);
    pi_cluster_open(&cluster_dev);

    pi_freq_set(PI_FREQ_DOMAIN_FC, FREQ_FC*1000*1000);
    pi_freq_set(PI_FREQ_DOMAIN_CL, FREQ_CL*1000*1000);
    pi_freq_set(PI_FREQ_DOMAIN_PERIPH, FREQ_PE*1000*1000);
    printf("Set FC Frequency = %d MHz, CL Frequency = %d MHz, PERIIPH Frequency = %d MHz\n",
        pi_freq_get(PI_FREQ_DOMAIN_FC), pi_freq_get(PI_FREQ_DOMAIN_CL), pi_freq_get(PI_FREQ_DOMAIN_PERIPH));
    #ifdef VOLTAGE
    pi_pmu_voltage_set(PI_PMU_VOLTAGE_DOMAIN_CHIP, VOLTAGE);
    pi_pmu_voltage_set(PI_PMU_VOLTAGE_DOMAIN_CHIP, VOLTAGE);
    printf("Voltage: %dmV\n", VOLTAGE);
    #endif

    struct pi_cluster_task *task = pi_l2_malloc(sizeof(struct pi_cluster_task));
    if(task==NULL) {
      printf("pi_cluster_task alloc Error!\n");
      pmsis_exit(-1);
    }
    pi_cluster_task(task, (void (*)(void *))&RunNetwork, NULL);
    pi_cluster_task_stacks(task, NULL, CLUSTER_SLAVE_STACK_SIZE);
    #if defined(__GAP8__)
    task->entry = &RunNetwork;
    task->stack_size = CLUSTER_STACK_SIZE;
    task->slave_stack_size = CLUSTER_SLAVE_STACK_SIZE;
    #endif
    printf("Stack sizes: %d %d\n", CLUSTER_STACK_SIZE, CLUSTER_SLAVE_STACK_SIZE);
  #endif

  ResOut = (short int *) AT_L2_ALLOC(0, NUM_CLASSES*sizeof(short int));
  if (ResOut==0) {
    printf("Failed to allocate Memory for Result (%ld bytes)\n", NUM_CLASSES*sizeof(short int));
    return 1;
  }

  printf("Constructor\n");
  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX(CNN_Construct)())
  {
    printf("Graph constructor exited with error\n");
    return 1;
  }

  printf("Reading image\n");
  if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH, AT_INPUT_HEIGHT, AT_INPUT_COLORS,
                        Input_1, AT_INPUT_SIZE*sizeof(IMAGE_IN_T), IMGIO_OUTPUT_CHAR, 0)) {
    printf("Failed to load image %s\n", ImageName);
    return 1;
  }
  #ifdef IMAGE_SUB_128
  for (int i=0; i<AT_INPUT_SIZE; i++) Input_1[i] -= 128;
  #endif
  printf("Finished reading image\n");

  #ifdef __EMUL__
    RunNetwork();
  #else
    pi_cluster_send_task_to_cl(&cluster_dev, task);
  #endif
    float person_not_seen = FIX2FP(ResOut[0] * vww_Output_1_OUT_QSCALE, vww_Output_1_OUT_QNORM);
    float person_seen = FIX2FP(ResOut[1] * vww_Output_1_OUT_QSCALE, vww_Output_1_OUT_QNORM);

    if (person_seen > person_not_seen) {
        printf("person seen! confidence %f\n", person_seen);
    } else {
        printf("no person seen %f\n", person_not_seen);
    }
  
  __PREFIX(CNN_Destruct)();

  #ifdef PERF
    {
      unsigned int TotalCycles = 0, TotalOper = 0;
      printf("\n");
      for (unsigned int i=0; i<(sizeof(AT_GraphPerf)/sizeof(unsigned int)); i++) {
        TotalCycles += AT_GraphPerf[i]; TotalOper += AT_GraphOperInfosNames[i];
      }
      for (unsigned int i=0; i<(sizeof(AT_GraphPerf)/sizeof(unsigned int)); i++) {
        printf("%45s: Cycles: %10u, Cyc%%: %5.1f%%, Operations: %10u, Op%%: %5.1f%%, Operations/Cycle: %f\n", AT_GraphNodeNames[i], AT_GraphPerf[i], 100*((float) (AT_GraphPerf[i]) / TotalCycles), AT_GraphOperInfosNames[i], 100*((float) (AT_GraphOperInfosNames[i]) / TotalOper), ((float) AT_GraphOperInfosNames[i])/ AT_GraphPerf[i]);
      }
      printf("\n");
      printf("%45s: Cycles: %10u, Cyc%%: 100.0%%, Operations: %10u, Op%%: 100.0%%, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
      printf("\n");
    }
  #endif

  printf("Ended\n");
  #ifndef __EMUL__
    //Checks for jenkins:
    if(FIX2FP(ResOut[1] * vww_Output_1_OUT_QSCALE, vww_Output_1_OUT_QNORM)>0.89) { printf("Correct Results!\n");pmsis_exit(0);}
    else { printf("Wrong Results!\n");pmsis_exit(-1);}
    pi_cluster_close(&cluster_dev);
    pmsis_exit(0);
  #endif
  return 0;
}

int main(int argc, char *argv[])
{
    printf("\n\n\t *** NNTOOL VWW MIT Example ***\n\n");
    #ifdef __EMUL__
    if (argc < 2)
    {
      printf("Usage: VWW MIT [image_file]\n");
      exit(-1);
    }
    ImageName = argv[1];
    start();
    #else
    ImageName = __XSTR(AT_IMAGE);
    return pmsis_kickoff((void *) start);
    #endif
    return 0;
}

