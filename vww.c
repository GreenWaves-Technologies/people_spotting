/*
 * Copyright (C) 2017 GreenWaves Technologies
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 *
 */

#include <stdio.h>
#include "pmsis.h"
#include "pmsis.h"
#include "bsp/bsp.h"
#include "bsp/camera.h"
#include "bsp/camera/gc0308.h"
#include "bsp/display/ili9341.h"
#include "vww.h"
#include "vwwKernels.h"
#include "gaplib/ImgIO.h"


#define __XSTR(__s) __STR(__s)
#define __STR(__s) #__s

#define AT_INPUT_SIZE (AT_INPUT_WIDTH*AT_INPUT_HEIGHT*AT_INPUT_COLORS)
#define PIXEL_SIZE 2

AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash) = 0;

struct pi_device ili;
struct pi_device device;
static pi_buffer_t buffer;

static struct pi_device camera;

// Softmax always outputs Q15 short int even from 8 bit input
PI_L2 short int *ResOut;
uint8_t *Input_1;

static int open_camera(struct pi_device *device)
{
    printf("Opening GC0308 camera\n");
    struct pi_gc0308_conf cam_conf;
    pi_gc0308_conf_init(&cam_conf);

    cam_conf.format = PI_CAMERA_QVGA;

    pi_open_from_conf(device, &cam_conf);
    if (pi_camera_open(device))
        return -1;

    return 0;
}

static int open_display(struct pi_device *device)
{
  struct pi_ili9341_conf ili_conf;

  pi_ili9341_conf_init(&ili_conf);

  pi_open_from_conf(device, &ili_conf);

  if (pi_display_open(device))
    return -1;

  if (pi_display_ioctl(device, PI_ILI_IOCTL_ORIENTATION, (void *)PI_ILI_ORIENTATION_90))
    return -1;

  return 0;
}

void draw_text(struct pi_device *display, const char *str, unsigned posX, unsigned posY, unsigned fontsize)
{
    writeFillRect(display, 0, posY, 320, fontsize*8, 0xFFFF);
    setCursor(display, posX, posY);
    writeText(display, str, fontsize);
}


static void RunNetwork()
{
  //printf("Running on cluster\n");
#ifdef PERF
  printf("Start timer\n");
  gap_cl_starttimer();
  gap_cl_resethwtimer();
#endif
  __PREFIX(CNN)((unsigned short*)Input_1,ResOut);
  //printf("Runner completed\n");

  //printf("\n");

}


int start()
{
    char *ImageName = __XSTR(AT_IMAGE);
    struct pi_device cluster_dev;
    struct pi_cluster_task *task;
    struct pi_cluster_conf conf;
    char result_out[30];
    unsigned int W = 238, H = 208;
    unsigned int Wcam=238, Hcam=208;
    
    //Input image size
    printf("Entering main controller\n");
    //pi_freq_set(PI_FREQ_DOMAIN_FC,50000000);
    //Allocating output
    ResOut = (short int *) pmsis_l2_malloc( 2*sizeof(short int));
    if (ResOut==0) {
        printf("Failed to allocate Memory for Result (%ld bytes)\n", 2*sizeof(short int));
        pmsis_exit(-1);
    }

    //allocating input
    Input_1 = (uint8_t*)pmsis_l2_malloc(AT_INPUT_WIDTH*AT_INPUT_HEIGHT*PIXEL_SIZE);

#ifndef FROM_CAMERA
    printf("Reading image\n");
    //Reading Image from Bridge
     img_io_out_t type = IMGIO_OUTPUT_RGB565;
    if (ReadImageFromFile(ImageName, AT_INPUT_WIDTH, AT_INPUT_HEIGHT, AT_INPUT_COLORS, Input_1, AT_INPUT_SIZE*PIXEL_SIZE, type, 0)) {
        printf("Failed to load image %s\n", ImageName);
        pmsis_exit(-1);
    }
    printf("Finished reading image\n");
#else

    if (open_display(&ili))
    {
        printf("Failed to open display\n");
        pmsis_exit(-1);
    }

    writeFillRect(&ili, 0, 0, 320, 240, 0xFFFF);
    writeText(&ili, "  GreenWaves Technologies", 2);

    buffer.data = Input_1;
    buffer.stride = 0;

    // WIth Himax, propertly configure the buffer to skip boarder pixels
    pi_buffer_init(&buffer, PI_BUFFER_TYPE_L2, Input_1);
    pi_buffer_set_stride(&buffer, 0);
    pi_buffer_set_format(&buffer, AT_INPUT_WIDTH, AT_INPUT_HEIGHT, 2, PI_BUFFER_FORMAT_RGB565);

    if (open_camera(&camera))
    {
        printf("Failed to open camera\n");
        pmsis_exit(-1);
    }
    pi_camera_set_crop(&camera,160,120,AT_INPUT_WIDTH,AT_INPUT_HEIGHT);

    
#endif

    pi_cluster_conf_init(&conf);
    pi_open_from_conf(&cluster_dev, (void *)&conf);
    if (pi_cluster_open(&cluster_dev))
    {
        printf("Cluster open failed !\n");
        pmsis_exit(-7);
    }
    
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
        pmsis_exit(-1);
    }

    pi_freq_set(PI_FREQ_DOMAIN_CL,175000000);

    printf("Application main cycle\n");
    int iter=1;
    while(iter){

        #ifndef FROM_CAMERA
        iter=0;
        #else

            pi_task_t task_1;
            pi_task_t task_2;
            pi_camera_capture_async(&camera, Input_1, AT_INPUT_WIDTH*AT_INPUT_HEIGHT,pi_task_block(&task_1));
            pi_camera_capture_async(&camera, Input_1+AT_INPUT_WIDTH*AT_INPUT_HEIGHT, AT_INPUT_WIDTH*AT_INPUT_HEIGHT,pi_task_block(&task_2));
            pi_camera_control(&camera, PI_CAMERA_CMD_START, 0);
            pi_task_wait_on(&task_1);
            pi_task_wait_on(&task_2);
            pi_camera_control(&camera, PI_CAMERA_CMD_STOP, 0);

        #endif

        // Execute the function "RunNetwork" on the cluster.
        pi_cluster_send_task_to_cl(&cluster_dev, task);

        #ifndef FROM_CAMERA
        
        if (ResOut[1] > ResOut[0]) {
            printf("person seen (%f, %f)\n", FIX2FP(ResOut[0],15), FIX2FP(ResOut[1],15));
        } else {
            printf("no person seen (%f, %f)\n", FIX2FP(ResOut[0],15), FIX2FP(ResOut[1],15));
        }

        #else
        pi_display_write(&ili, &buffer, 41,20, AT_INPUT_WIDTH, AT_INPUT_HEIGHT);
        if (ResOut[1] > ResOut[0]) 
            sprintf(result_out,"Person seen (%f)",FIX2FP(ResOut[1],15));
            draw_text(&ili, result_out, 40, 218, 2);
        else
            sprintf(result_out,"Person seen (%f)",FIX2FP(ResOut[0],15));
            draw_text(&ili, result_out, 40, 218, 2);
        #endif
    }

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

    pmsis_l2_malloc_free(ResOut, 2*sizeof(short int));
    pmsis_l2_malloc_free(Input_1,AT_INPUT_WIDTH*AT_INPUT_HEIGHT*PIXEL_SIZE);
    printf("Ended\n");
    pmsis_exit(0);
    return 0;
}

int main(void)
{
    return pmsis_kickoff((void *) start);
}
