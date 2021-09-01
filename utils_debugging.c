#include "utils_debugging.h"

void debug_avframe_info(char* hint, AVFrame *pFrame) {
  printf("%s [%c] kf:%d fl:0x%x size:%d w:%d h:%d format:%d pts:%ld dts:%ld cpnum:%d dpnum:%d interl:%d\n",
        hint?hint:"Frame",
        av_get_picture_type_char(pFrame->pict_type),
        pFrame->key_frame,
        pFrame->flags,
        pFrame->pkt_size,
        pFrame->width,
        pFrame->height,
        pFrame->format,
        pFrame->pts,
        pFrame->pkt_dts,
        pFrame->coded_picture_number,
        pFrame->display_picture_number,
        pFrame->interlaced_frame
    );
}

void debug_avframe_num(int num, AVFrame *pFrame) {
  char hint[1024];
  sprintf(hint, "Frame #%03d", num);
  debug_avframe_info(hint, pFrame);
}

void debug_avframe_audio_info(const char* hint, AVFrame *pFrame) {
  printf("[%s] channels:%d ch_layout:%ld rate:%d nb_sample:%d format:%d  fl:0x%x size:%d  pts:%ld pkt_pts:%ld pkt_dur:%ld  dec_error:%ld \n",
        hint?hint:"Frame",
        pFrame->channels,
        pFrame->channel_layout,
        pFrame->sample_rate,
        pFrame->nb_samples,
        pFrame->format,
        pFrame->flags,
        pFrame->pkt_size,
        pFrame->pts,
        pFrame->pkt_dts,
        pFrame->pkt_duration,
        pFrame->decode_error_flags
    );
}



void debug_avpacket_info(char* hint, AVPacket *pPacket) {
  printf("%s  idx:%d flags:0x%x size:%d pts:%ld dts:%ld dur:%ld side_data:%d\n",
        hint?hint:"Packet",
        pPacket->stream_index,
        pPacket->flags,
        pPacket->size,
        pPacket->pts,
        pPacket->dts,
        pPacket->duration,
        pPacket->side_data_elems
    );
}

void debug_video_avcodeccontext_info(char* hint, AVCodecContext *video_avcc) {
  printf("%s  w:%d h:%d pix_fmt:%d tb:%d/%d \n",
        hint?hint:"VCodec",
        video_avcc->width,
        video_avcc->height,
        video_avcc->pix_fmt,
        video_avcc->time_base.num,
        video_avcc->time_base.den
    );
}
