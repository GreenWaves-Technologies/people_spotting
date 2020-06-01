/*
 * Copyright (C) 2017 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include <stdio.h>

#include "vww.h"
#include "vwwKernels.h"
#include "ImgIO.h"

#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;



// Softmax always outputs Q15 short int even from 8 bit input
L2_MEM short int *ResOut;
typedef signed char IMAGE_IN_T;
L2_MEM IMAGE_IN_T *ImageIn;
unsigned char Input_1[AT_INPUT_SIZE];


#ifdef PERF
L2_MEM rt_perf_t *cluster_perf;
#endif

static void RunNetwork()
{
  printf("Running on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
  __PREFIX(CNN)(Input_1,ResOut);
  printf("Runner completed\n");

  printf("\n");
  
  //Checki Results
  if (ResOut[1] > ResOut[0]) {
    printf("person seen (%d, %d)\n", ResOut[0], ResOut[1]);
  } else {
    printf("no person seen (%d, %d)\n", ResOut[0], ResOut[1]);
  }
  printf("\n");
}


int start()
{
  char *ImageName = __XSTR(AT_IMAGE);
  struct pi_device cluster_dev;
  struct pi_cluster_task *task;
  struct pi_cluster_conf conf;
  //Input image size

  printf("Entering main controller\n");

  pi_cluster_conf_init(&conf);
  pi_open_from_conf(&cluster_dev, (void *)&conf);
  pi_cluster_open(&cluster_dev);
//  pi_freq_set(PI_FREQ_DOMAIN_CL,175000000);
  task = pmsis_l2_malloc(sizeof(struct pi_cluster_task));
  memset(task, 0, sizeof(struct pi_cluster_task));
  task->entry = &RunNetwork;
  task->stack_size = STACK_SIZE;
  task->slave_stack_size = SLAVE_STACK_SIZE;
  task->arg = NULL;

  printf("Constructor\n");

  // IMPORTANT - MUST BE CALLED AFTER THE CLUSTER IS SWITCHED ON!!!!
  if (__PREFIX(CNN_Construct)())
  {
    printf("Graph constructor exited with an error\n");
    return 1;
  }

#ifndef FROM_CAMERA
  printf("Reading image\n");
  //Reading Image from Bridge
  if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH, AT_INPUT_HEIGHT, AT_INPUT_COLORS, (char *)Input_1, AT_INPUT_SIZE*sizeof(IMAGE_IN_T), IMGIO_OUTPUT_CHAR, 0)) {
    printf("Failed to load image %s\n", ImageName);
    return 1;
  }
  printf("Finished reading image\n");
#else

#endif
#ifdef PRINT_IMAGE
  for (int i=0; i<H; i++) {
    for (int j=0; j<W; j++) {
      printf("%03d, ", ImageInChar[W*i + j]);
    }
    printf("\n");
  }
#endif
  ResOut = (short int *) AT_L2_ALLOC(0, 2*sizeof(short int));

  if (ResOut==0) {
    printf("Failed to allocate Memory for Result (%ld bytes)\n", 2*sizeof(short int));
    return 1;
  }

  printf("Call cluster\n");
  // Execute the function "RunNetwork" on the cluster.
#ifdef __EMUL__
  RunNetwork(NULL);
#else
  pi_cluster_send_task_to_cl(&cluster_dev, task);
#endif
  
  __PREFIX(CNN_Destruct)();

#ifdef PERF
  {
    unsigned int TotalCycles = 0, TotalOper = 0;
    printf("\n");
    for (int i=0; i<(sizeof(AT_GraphPerf)/sizeof(unsigned int)); i++) {
      printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", AT_GraphNodeNames[i],
             AT_GraphPerf[i], AT_GraphOperInfosNames[i], ((float) AT_GraphOperInfosNames[i])/ AT_GraphPerf[i]);
      TotalCycles += AT_GraphPerf[i]; TotalOper += AT_GraphOperInfosNames[i];
    }
    printf("\n");
    printf("%45s: Cycles: %10d, Operations: %10d, Operations/Cycle: %f\n", "Total", TotalCycles, TotalOper, ((float) TotalOper)/ TotalCycles);
    printf("\n");
  }
#endif

#ifdef __EMUL__
  dt_close_dump_file();
#else
  pmsis_exit(0);
#endif

  printf("Ended\n");
  return 0;
}

#ifndef __EMUL__
int main(void)
{
  return pmsis_kickoff((void *) start);
}
#endif
