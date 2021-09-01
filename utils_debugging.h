#include <stdio.h>
#include <stdlib.h>
#include "libavfilter/avfilter.h"
#include "libavcodec/avcodec.h"

void debug_avframe_info(char* hint, AVFrame *pFrame);
void debug_avframe_num(int num, AVFrame *pFrame);
void debug_avframe_audio_info(const char* hint, AVFrame *pFrame);

void debug_avpacket_info(char* hint, AVPacket *pPacket);

void debug_video_avcodeccontext_info(char* hint, AVCodecContext *video_avcc);
