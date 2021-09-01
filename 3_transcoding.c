#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <libavutil/opt.h>
#include <string.h>
#include <inttypes.h>
#include "video_debugging.h"
#include "utils_debugging.h"

typedef struct StreamingParams {
  char copy_video;
  char copy_audio;
  char *output_extension;
  char *muxer_opt_key;
  char *muxer_opt_value;
  char *video_codec;
  char *audio_codec;
  char *codec_priv_key;
  char *codec_priv_value;
} StreamingParams;

typedef struct StreamingContext {
  AVFormatContext *avfc;
  AVCodec *video_avc;
  AVCodec *audio_avc;
  AVStream *video_avs;
  AVStream *audio_avs;
  AVCodecContext *video_avcc;
  AVCodecContext *audio_avcc;
  int video_index;
  int audio_index;
  char *filename;
} StreamingContext;

int count = 0;

int fill_stream_info(AVStream *avs, AVCodec **avc, AVCodecContext **avcc) {
  printf("> fill_stream_info codec_id:%d \n", avs->codecpar->codec_id);


  *avc = avcodec_find_decoder(avs->codecpar->codec_id);
  if (!*avc) {logging("failed to find the codec"); return -1;}
  printf("- avcodec_find_decoder codeec:%p \n", *avc);

  *avcc = avcodec_alloc_context3(*avc);
  if (!*avcc) {logging("failed to alloc memory for codec context"); return -1;}
  printf("- avcodec_alloc_context3 avcc:%p \n", *avcc);

  if (avcodec_parameters_to_context(*avcc, avs->codecpar) < 0) {logging("failed to fill codec context"); return -1;}
  printf("- avcodec_parameters_to_context (ctx <- avs) \n");

  if (avcodec_open2(*avcc, *avc, NULL) < 0) {logging("failed to open codec"); return -1;}
  printf("- avcodec_open2\n");

  printf("< fill_stream_info\n");

  return 0;
}

int open_media(const char *in_filename, AVFormatContext **avfc) {
  printf("> open_media\n");

  *avfc = avformat_alloc_context();
  if (!*avfc) {logging("failed to alloc memory for format"); return -1;}
  printf("- avformat_alloc_context avfc:%p \n", *avfc);

  if (avformat_open_input(avfc, in_filename, NULL, NULL) != 0) {logging("failed to open input file %s", in_filename); return -1;}
  printf("- avformat_open_input in_filename:%s \n", in_filename);

  if (avformat_find_stream_info(*avfc, NULL) < 0) {logging("failed to get stream info"); return -1;}
  printf("- avformat_find_stream_info\n");
  av_dump_format(*avfc, 0, "", 0);

  printf("< open_media\n");
  return 0;
}

int prepare_decoder(StreamingContext *sc) {
//  printf("> prepare_decoder\n");

  for (int i = 0; i < sc->avfc->nb_streams; i++) {
    if (sc->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      sc->video_avs = sc->avfc->streams[i];
      sc->video_index = i;
//      printf("- prepare_decoder video_index :%d \n", i);

      if (fill_stream_info(sc->video_avs, &sc->video_avc, &sc->video_avcc)) {return -1;}
    } else if (sc->avfc->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
      sc->audio_avs = sc->avfc->streams[i];
      sc->audio_index = i;
//      printf("- prepare_decoder audio_index :%d \n", i);

      if (fill_stream_info(sc->audio_avs, &sc->audio_avc, &sc->audio_avcc)) {return -1;}
    } else {
      logging("skipping streams other than audio and video");
    }
  }

//  printf("< prepare_decoder\n");


  return 0;
}

int prepare_video_encoder(StreamingContext *sc, AVCodecContext *decoder_ctx, AVRational input_framerate, StreamingParams sp) {
  printf("> prepare_video_encoder\n");

  sc->video_avs = avformat_new_stream(sc->avfc, NULL);
  printf("- avformat_new_stream stream:%p \n", sc->video_avs);

  sc->video_avc = avcodec_find_encoder_by_name(sp.video_codec);
  if (!sc->video_avc) {logging("could not find the proper codec"); return -1;}
  printf("- avcodec_find_encoder_by_name %s codec:%p \n", sp.video_codec, sc->video_avc);

  sc->video_avcc = avcodec_alloc_context3(sc->video_avc);
  if (!sc->video_avcc) {logging("could not allocated memory for codec context"); return -1;}
  printf("- avcodec_alloc_context3 video_avcc:%p \n", sc->video_avcc);

  av_opt_set(sc->video_avcc->priv_data, "preset", "fast", 0);
  if (sp.codec_priv_key && sp.codec_priv_value)
    av_opt_set(sc->video_avcc->priv_data, sp.codec_priv_key, sp.codec_priv_value, 0);

  printf("- av_opt_set \n");


  sc->video_avcc->height = decoder_ctx->height;
  sc->video_avcc->width = decoder_ctx->width;
  // printf("height:0x%x\n", sc->video_avcc->height);
  // printf("width:0x%x\n", sc->video_avcc->width);

  sc->video_avcc->sample_aspect_ratio = decoder_ctx->sample_aspect_ratio;
//  printf("sample_aspect_ratio:0x%x\n", sc->video_avcc->sample_aspect_ratio);
  if (sc->video_avc->pix_fmts)
    sc->video_avcc->pix_fmt = sc->video_avc->pix_fmts[0];
  else
    sc->video_avcc->pix_fmt = decoder_ctx->pix_fmt;
//  printf("pix_fmt:0x%x\n", sc->video_avcc->pix_fmt);

  sc->video_avcc->bit_rate = 2 * 1000 * 1000;
  sc->video_avcc->rc_buffer_size = 4 * 1000 * 1000;
  sc->video_avcc->rc_max_rate = 2 * 1000 * 1000;
  sc->video_avcc->rc_min_rate = 2.5 * 1000 * 1000;

  sc->video_avcc->time_base = av_inv_q(input_framerate);
  sc->video_avs->time_base = sc->video_avcc->time_base;
  printf("time_base:%f\n", av_q2d(sc->video_avcc->time_base));

  if (avcodec_open2(sc->video_avcc, sc->video_avc, NULL) < 0) {logging("could not open the codec"); return -1;}
  printf("- avcodec_open2 \n");

  avcodec_parameters_from_context(sc->video_avs->codecpar, sc->video_avcc);
  printf("- avcodec_parameters_from_context str <-- video_ctx \n");

  printf("< prepare_video_encoder\n");
  return 0;
}

int prepare_audio_encoder(StreamingContext *sc, int sample_rate, StreamingParams sp){
  printf("> prepare_audio_encoder\n");

  sc->audio_avs = avformat_new_stream(sc->avfc, NULL);
  printf("- avformat_new_stream stream:%p \n", sc->audio_avs);

printf("> prepare_audio_encoder sample_rate=%d\n", sample_rate);

  sc->audio_avc = avcodec_find_encoder_by_name(sp.audio_codec);
  if (!sc->audio_avc) {logging("could not find the proper codec"); return -1;}
  printf("- avcodec_find_encoder_by_name %s codec:%p \n", sp.audio_codec, sc->audio_avc);

  sc->audio_avcc = avcodec_alloc_context3(sc->audio_avc);
  if (!sc->audio_avcc) {logging("could not allocated memory for codec context"); return -1;}

  int OUTPUT_CHANNELS = 2;
  int OUTPUT_BIT_RATE = 196000;
  sc->audio_avcc->channels       = OUTPUT_CHANNELS;
  sc->audio_avcc->channel_layout = av_get_default_channel_layout(OUTPUT_CHANNELS);
  sc->audio_avcc->sample_rate    = sample_rate;
  sc->audio_avcc->sample_fmt     = sc->audio_avc->sample_fmts[0];
  sc->audio_avcc->bit_rate       = OUTPUT_BIT_RATE;
  sc->audio_avcc->time_base      = (AVRational){1, sample_rate};

  sc->audio_avcc->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;

  sc->audio_avs->time_base = sc->audio_avcc->time_base;

  if (avcodec_open2(sc->audio_avcc, sc->audio_avc, NULL) < 0) {logging("could not open the codec"); return -1;}
  printf("- avcodec_open2 \n");

  avcodec_parameters_from_context(sc->audio_avs->codecpar, sc->audio_avcc);
  printf("- avcodec_parameters_from_context str <-- audio_ctx \n");

  printf("< prepare_audio_encoder\n");

  return 0;
}

int prepare_copy(AVFormatContext *avfc, AVStream **avs, AVCodecParameters *decoder_par) {
  *avs = avformat_new_stream(avfc, NULL);
  avcodec_parameters_copy((*avs)->codecpar, decoder_par);
  return 0;
}

int remux(AVPacket **pkt, AVFormatContext **avfc, AVRational decoder_tb, AVRational encoder_tb) {
  av_packet_rescale_ts(*pkt, decoder_tb, encoder_tb);
  if (av_interleaved_write_frame(*avfc, *pkt) < 0) { logging("error while copying stream packet"); return -1; }
  return 0;
}


int encode_video(StreamingContext *decoder, StreamingContext *encoder, AVFrame *input_frame) {
//  if (count < 3) printf("> encode_video\n");

  if (input_frame) input_frame->pict_type = AV_PICTURE_TYPE_NONE;
//  if (count < 3) printf("- pict_type = AV_PICTURE_TYPE_NONE\n");

  AVPacket *output_packet = av_packet_alloc();
  if (!output_packet) {logging("could not allocate memory for output packet"); return -1;}
//  if (count < 3) printf("- av_packet_alloc\n");

//  AVCodecContext *avctx = encoder->video_avcc;
//  printf(".encode_video avctx->codec_type:0x%x\n", avctx->codec_type);

// printf(".encode_video frame->width:0x%x\n", input_frame->width);
// printf(".encode_video frame->height:0x%x\n", input_frame->height);
// printf(".encode_video frame->format:0x%x\n", input_frame->format);
// printf(".encode_video frame->pict_type:0x%x\n", input_frame->pict_type);

//av_log_set_level(AV_LOG_TRACE);

  int response = avcodec_send_frame(encoder->video_avcc, input_frame);
//  if (count < 3) printf("- avcodec_send_frame\n");

  while (response >= 0) {
    response = avcodec_receive_packet(encoder->video_avcc, output_packet);
//    if (count < 3) printf("- avcodec_receive_packet\n");

    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      logging("Error while receiving packet from encoder: %s", av_err2str(response));
      return -1;
    }
    //debug_avpacket_info("Packet (initil)", output_packet);



    output_packet->stream_index = decoder->video_index;
    // printf(". dur1: %d \n", encoder->video_avs->time_base.den);
    // printf(". dur2: %d \n", encoder->video_avs->time_base.num);
    // printf(". dur3: %d \n", decoder->video_avs->avg_frame_rate.num);
    // printf(". dur4: %d \n", decoder->video_avs->avg_frame_rate.den);

    output_packet->duration = encoder->video_avs->time_base.den / encoder->video_avs->time_base.num / decoder->video_avs->avg_frame_rate.num * decoder->video_avs->avg_frame_rate.den;
//    if (count < 3) printf("- output_packet->duration =\n");

    // printf(". tb1: %f \n", av_q2d(decoder->video_avs->time_base));
    // printf(". tb2: %f \n", av_q2d(encoder->video_avs->time_base));

    av_packet_rescale_ts(output_packet, decoder->video_avs->time_base, encoder->video_avs->time_base);
//    if (count < 3) printf("- av_packet_rescale_ts \n");

    // debug_avpacket_info("Packet (scaled)", output_packet);
    // printf("\n");

    response = av_interleaved_write_frame(encoder->avfc, output_packet);
    if (response != 0) { logging("Error %d while receiving packet from decoder: %s", response, av_err2str(response)); return -1;}
//    if (count < 3) printf("- av_interleaved_write_frame \n");

  }
  av_packet_unref(output_packet);
  av_packet_free(&output_packet);

//  if (count < 3) printf("< encode_video\n");

  return 0;
}

int encode_audio(StreamingContext *decoder, StreamingContext *encoder, AVFrame *input_frame) {
  if (count < 10) printf("> encode_audio\n");

//  av_log_set_level(AV_LOG_TRACE);

if (count < 10)
debug_avframe_audio_info("encode_audio", input_frame);


  AVPacket *output_packet = av_packet_alloc();
  if (!output_packet) {logging("could not allocate memory for output packet"); return -1;}

  int response = avcodec_send_frame(encoder->audio_avcc, input_frame);
  if (count < 10) printf("- avcodec_send_frame\n");

  while (response >= 0) {
    response = avcodec_receive_packet(encoder->audio_avcc, output_packet);
    if (count < 10) printf("- avcodec_receive_packet\n");

    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      logging("Error while receiving packet from encoder: %s", av_err2str(response));
      return -1;
    }

    output_packet->stream_index = decoder->audio_index;

    if (count < 10) {
       printf("> pts:%ld dts:%ld dur:%ld in.tb:%f out.tb:%f \n", output_packet->pts, output_packet->dts, output_packet->duration,
          av_q2d(av_inv_q(decoder->audio_avs->time_base)), av_q2d(av_inv_q(encoder->audio_avs->time_base)) );
       debug_avpacket_info("A Packet (in)", output_packet);
     }

    av_packet_rescale_ts(output_packet, decoder->audio_avs->time_base, encoder->audio_avs->time_base);
    if (count < 10) printf("- av_packet_rescale_ts\n");

    if (count < 10) {
       printf("< pts:%ld dts:%ld dur:%ld \n", output_packet->pts, output_packet->dts, output_packet->duration);
       debug_avpacket_info("A Packet (sc)", output_packet);
       printf("\n");
     }

    response = av_interleaved_write_frame(encoder->avfc, output_packet);
    if (response != 0) { logging("Error %d while receiving packet from decoder: %s", response, av_err2str(response)); return -1;}
    if (count < 10) printf("- av_interleaved_write_frame\n");

  }
  av_packet_unref(output_packet);
  av_packet_free(&output_packet);

  if (count < 10) printf("< encode_audio\n");

  count++;

  return 0;
}

int transcode_audio(StreamingContext *decoder, StreamingContext *encoder, AVPacket *input_packet, AVFrame *input_frame) {
  if (count < 10) printf("> transcode_audio\n");

  int response = avcodec_send_packet(decoder->audio_avcc, input_packet);
  if (response < 0) {logging("Error while sending packet to decoder: %s", av_err2str(response)); return response;}
  if (count < 10) printf("- avcodec_send_packet\n");

  while (response >= 0) {
    response = avcodec_receive_frame(decoder->audio_avcc, input_frame);
    if (count < 10) printf("- avcodec_receive_frame\n");

    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      logging("Error while receiving frame from decoder: %s", av_err2str(response));
      return response;
    }


    if (response >= 0) {
      if (count < 10) {
        debug_avframe_audio_info("input_frame", input_frame);
      }

      if (encode_audio(decoder, encoder, input_frame)) return -1;
    }
    av_frame_unref(input_frame);
  }
  if (count < 3) printf("< transcode_audio\n");

  return 0;
}

int transcode_video(StreamingContext *decoder, StreamingContext *encoder, AVPacket *input_packet, AVFrame *input_frame) {
//  if (count < 3) printf("> transcode_video\n");

  int response = avcodec_send_packet(decoder->video_avcc, input_packet);
  if (response < 0) {logging("Error while sending packet to decoder: %s", av_err2str(response)); return response;}

  while (response >= 0) {
    response = avcodec_receive_frame(decoder->video_avcc, input_frame);
    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
      break;
    } else if (response < 0) {
      logging("Error while receiving frame from decoder: %s", av_err2str(response));
      return response;
    }

    // if (count < 10) {
    //   debug_avframe_info(NULL, input_frame);
    // }
    if (response >= 0) {
      if (encode_video(decoder, encoder, input_frame)) return -1;
    }
    av_frame_unref(input_frame);
  }
//  if (count < 3) printf("> transcode_video\n");

  return 0;
}

int main(int argc, char *argv[])
{
  printf("> main\n");

  /*
   * H264 -> H265
   * Audio -> remuxed (untouched)
   * MP4 - MP4
   */
  // StreamingParams sp = {0};
  // sp.copy_audio = 1;
  // sp.copy_video = 0;
  // sp.video_codec = "libx264";
  // sp.codec_priv_key = "x264-params";
  // sp.codec_priv_value = "keyint=30:min-keyint=30:scenecut=0:force-cfr=1:bframes=0";

  /*
   * H264 -> H264 (fixed gop)
   * Audio -> remuxed (untouched)
   * MP4 - MP4
   */
  //StreamingParams sp = {0};
  //sp.copy_audio = 1;
  //sp.copy_video = 0;
  //sp.video_codec = "libx264";
  //sp.codec_priv_key = "x264-params";
  //sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";

  /*
   * H264 -> H264 (fixed gop)
   * Audio -> remuxed (untouched)
   * MP4 - fragmented MP4
   */
  //StreamingParams sp = {0};
  //sp.copy_audio = 1;
  //sp.copy_video = 0;
  //sp.video_codec = "libx264";
  //sp.codec_priv_key = "x264-params";
  //sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
  //sp.muxer_opt_key = "movflags";
  //sp.muxer_opt_value = "frag_keyframe+empty_moov+default_base_moof";

  /*
   * H264 -> H264 (fixed gop)
   * Audio -> AAC
   * MP4 - MPEG-TS
   */
  StreamingParams sp = {0};
  sp.copy_audio = 0;
  sp.copy_video = 0;
  sp.video_codec = "libx264";
  sp.codec_priv_key = "x264-params";
  sp.codec_priv_value = "keyint=60:min-keyint=60:scenecut=0:force-cfr=1";
  sp.audio_codec = "aac";
  sp.output_extension = ".ts";

  /*
   * H264 -> VP9
   * Audio -> Vorbis
   * MP4 - WebM
   */
  //StreamingParams sp = {0};
  //sp.copy_audio = 0;
  //sp.copy_video = 0;
  //sp.video_codec = "libvpx-vp9";
  //sp.audio_codec = "libvorbis";
  //sp.output_extension = ".webm";

  StreamingContext *decoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
  decoder->filename = argv[1];

  StreamingContext *encoder = (StreamingContext*) calloc(1, sizeof(StreamingContext));
  encoder->filename = argv[2];

  if (sp.output_extension)
    strcat(encoder->filename, sp.output_extension);

  if (open_media(decoder->filename, &decoder->avfc)) return -1;
  if (prepare_decoder(decoder)) return -1;

  avformat_alloc_output_context2(&encoder->avfc, NULL, NULL, encoder->filename);
  if (!encoder->avfc) {logging("could not allocate memory for output format");return -1;}
  printf("- avformat_alloc_output_context2\n");

  // TEMP: detailed logging
     av_log_set_level(AV_LOG_TRACE);

  if (!sp.copy_video) {
    AVRational input_framerate = av_guess_frame_rate(decoder->avfc, decoder->video_avs, NULL);
    prepare_video_encoder(encoder, decoder->video_avcc, input_framerate, sp);
  } else {
    if (prepare_copy(encoder->avfc, &encoder->video_avs, decoder->video_avs->codecpar)) {return -1;}
  }

  if (!sp.copy_audio) {
    if (prepare_audio_encoder(encoder, decoder->audio_avcc->sample_rate, sp)) {return -1;}
  } else {
    if (prepare_copy(encoder->avfc, &encoder->audio_avs, decoder->audio_avs->codecpar)) {return -1;}
  }

  if (encoder->avfc->oformat->flags & AVFMT_GLOBALHEADER)
    encoder->avfc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  if (!(encoder->avfc->oformat->flags & AVFMT_NOFILE)) {
    if (avio_open(&encoder->avfc->pb, encoder->filename, AVIO_FLAG_WRITE) < 0) {
      logging("could not open the output file");
      return -1;
    }
    printf("- avio_open %s\n", encoder->filename);

  }

  AVDictionary* muxer_opts = NULL;

  if (sp.muxer_opt_key && sp.muxer_opt_value) {
    av_dict_set(&muxer_opts, sp.muxer_opt_key, sp.muxer_opt_value, 0);
  }

  if (avformat_write_header(encoder->avfc, &muxer_opts) < 0) {logging("an error occurred when opening output file"); return -1;}
  printf("- avformat_write_header \n");

  AVFrame *input_frame = av_frame_alloc();
  if (!input_frame) {logging("failed to allocated memory for AVFrame"); return -1;}
  printf("- av_frame_alloc input_frame:%p \n", input_frame);

  AVPacket *input_packet = av_packet_alloc();
  if (!input_packet) {logging("failed to allocated memory for AVPacket"); return -1;}
  printf("- av_frame_alloc input_packet:%p \n", input_packet);

  while (av_read_frame(decoder->avfc, input_packet) >= 0)
  {
    if (decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
      debug_avpacket_info("[RTMP V packet]", input_packet);

      if (!sp.copy_video) {
        // TODO: refactor to be generic for audio and video (receiving a function pointer to the differences)
        if (transcode_video(decoder, encoder, input_packet, input_frame)) return -1;
        av_packet_unref(input_packet);
      } else {
        if (remux(&input_packet, &encoder->avfc, decoder->video_avs->time_base, encoder->video_avs->time_base)) return -1;
      }
    } else if (decoder->avfc->streams[input_packet->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)  {
      debug_avpacket_info("[RTMP A packet]", input_packet);

      if (!sp.copy_audio) {
        if (transcode_audio(decoder, encoder, input_packet, input_frame)) return -1;
        av_packet_unref(input_packet);
      } else {
        if (remux(&input_packet, &encoder->avfc, decoder->audio_avs->time_base, encoder->audio_avs->time_base)) return -1;
      }
    } else {
      logging("ignoring all non video or audio packets");
    }
  }
  // TODO: should I also flush the audio encoder?
  if (encode_video(decoder, encoder, NULL)) return -1;

  av_write_trailer(encoder->avfc);
  printf("- av_write_trailer \n");

  if (muxer_opts != NULL) {
    av_dict_free(&muxer_opts);
    muxer_opts = NULL;
  }

  if (input_frame != NULL) {
    av_frame_free(&input_frame);
    input_frame = NULL;
  }

  if (input_packet != NULL) {
    av_packet_free(&input_packet);
    input_packet = NULL;
  }

  avformat_close_input(&decoder->avfc);

  avformat_free_context(decoder->avfc); decoder->avfc = NULL;
  avformat_free_context(encoder->avfc); encoder->avfc = NULL;

  avcodec_free_context(&decoder->video_avcc); decoder->video_avcc = NULL;
  avcodec_free_context(&decoder->audio_avcc); decoder->audio_avcc = NULL;

  free(decoder); decoder = NULL;
  free(encoder); encoder = NULL;

  printf("< main\n");

  return 0;
}
