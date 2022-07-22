
#ifndef __VWW_H__
#define __VWW_H__

#define __PREFIX(x) vww ## x

#include "Gap.h"
#include "gaplib/ImgIO.h"

#ifdef __EMUL__
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/param.h>
#include <string.h>
#endif

#include "../../power_meas_utils/measurments_utils.h"
extern AT_HYPERFLASH_FS_EXT_ADDR_TYPE __PREFIX(_L3_Flash);

#endif
