#ifndef AUDIODEFINE_H
#define AUDIODEFINE_H

#include "widget.h"

    #define SDL_AUDIO_BUFFER_SIZE 1024
    #define MAX_AUDIO_FRAME_SIZE 192000
    //结构体定义
typedef struct PacketQueue{
    AVPacketList *first_pkt,*last_pkt;//队首、队尾
    int nb_packets;    //包的个数
    int size;//队列的字节数
    SDL_mutex *mutex;//互斥量
    SDL_cond    *cond;//条件变量
}PacketQueue;

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
    int frame_size;
    int bytes_per_sec;
} AudioParams;
    //函数定义
    int resample(AVFrame *af,uint8_t *audio_buf,int *audio_buf_size);
//    static void audio_callback(void *userdata, Uint8 * stream, int len);//
//    static int packet_queue_get(PacketQueue *q,AVPacket *pkt,int block);
    int    packet_queue_put(PacketQueue *q,AVPacket *pkt);
    void packet_queue_init(PacketQueue *q);
//    static int audio_decode_frame(AVCodecContext *aCodecCtx,uint8_t *audio_buf,int buf_size);


//全局变量声明
extern int quit;
extern PacketQueue audioQ;
extern int sample_rate, nb_channels;
extern int64_t channel_layout;
extern struct SwrContext * swr_ctx;
extern AudioParams audio_hw_params_tgt;
extern AudioParams audio_hw_params_src;

class AudioDefine
{
public:
    static void audio_callback(void *userdata, Uint8 * stream, int len);//
    static int packet_queue_get(PacketQueue *q,AVPacket *pkt,int block);
    static int audio_decode_frame(AVCodecContext *aCodecCtx,uint8_t *audio_buf,int buf_size);
};

//===================================

#endif // AUDIODEFINE_H
