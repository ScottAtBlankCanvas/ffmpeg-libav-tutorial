#include <stdio.h>
#include <stdlib.h>
#include "libavfilter/avfilter.h"

// save a frame into a .pgm file
void save_pgm_file(char *filename, unsigned char *buf, int wrap, int xsize, int ysize);
