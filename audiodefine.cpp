#include "audiodefine.h"


//全局变量定义
int quit =0;
PacketQueue audioQ;
int sample_rate, nb_channels;
int64_t channel_layout;
struct SwrContext * swr_ctx = NULL;
AudioParams audio_hw_params_tgt;
AudioParams audio_hw_params_src;

void AudioDefine::audio_callback(void *userdata, Uint8 * stream, int len)
{
    AVCodecContext *aCodecCtx = (AVCodecContext*)userdata;
    int len1,audio_size;
    static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE*3)/2];
    static unsigned int audio_buf_size = 0;
    static unsigned int audio_buf_index =0;
    while(len>0){
        if(audio_buf_index >= audio_buf_size){
            audio_size = audio_decode_frame(aCodecCtx,audio_buf,sizeof(audio_buf));
            if(audio_size < 0){
                audio_buf_size = 1024;
                memset(audio_buf,0,audio_buf_size);
            }else{
                audio_buf_size = audio_size;
            }
            audio_buf_index = 0;
        }
        len1 = audio_buf_size - audio_buf_index;
        if(len1 > len)
        len1 = len;
        memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
        len -= len1;
        stream += len1;
        audio_buf_index += len1;
    }
}

void packet_queue_init(PacketQueue *q)
{
    memset(q,0,sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
}

int packet_queue_put(PacketQueue *q,AVPacket *pkt)
{
    AVPacketList *pkt1;
    if(av_dup_packet(pkt)<0)
    {
        printf("dup packet error!\n");
        return -1;
    }
    pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
    if(!pkt1)
    {
        printf("malloc AVPacketList error!\n");
        return -1;
    }
    pkt1->pkt = *pkt;
    pkt1->next = NULL;
    SDL_LockMutex(q->mutex);
    if(!q->last_pkt){
        q->first_pkt = pkt1;
    }else{
        q->last_pkt->next = pkt1;
    }
    q->last_pkt = pkt1;
    q->nb_packets ++;
    q->size +=pkt1->pkt.size;
    SDL_CondSignal(q->cond);
    SDL_UnlockMutex(q->mutex);
    return 0;
}
int AudioDefine::packet_queue_get(PacketQueue *q,AVPacket *pkt,int block){
    int ret;
    AVPacketList *pkt1;
    SDL_LockMutex(q->mutex);
    for(;;){
        if(quit){
            printf("packet_queue has quit!\n");
            ret =-1;
            break;
        }
        pkt1 = q->first_pkt;
        if(pkt1){
            q->first_pkt = pkt1->next;
            if(!q->first_pkt){
                q->last_pkt = NULL;
            }
            q->nb_packets--;
            q->size -= pkt1->pkt.size;
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret =1;
            break;
        }else if(!block){
            ret =0;
            break;
        }else{
            SDL_CondWait(q->cond,q->mutex);//做了解锁互斥量的动作
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}
int AudioDefine::audio_decode_frame(AVCodecContext *aCodecCtx,uint8_t *audio_buf,int buf_size)
{
    static AVPacket pkt;
    static uint8_t *audio_pkt_data = NULL;
    static int audio_pkt_size = 0;
    int len1,data_size=0;
    static AVFrame frame;
    int got_frame=0;

    for(;;){
        while(audio_pkt_size >0 ){
            len1 = avcodec_decode_audio4(aCodecCtx, &frame, &got_frame, &pkt);
            if (len1 < 0)
            {
                /* if error, skip frame */
                audio_pkt_size = 0;
                break;
            }
            audio_pkt_data += len1;
            audio_pkt_size -= len1;
            data_size =0;
            if(got_frame){
                data_size = resample(&frame, audio_buf, &buf_size);
                assert(data_size <= buf_size);
            }
            if (data_size <= 0)
            {
                /* No data yet, get more frames */
                continue;
            }
            /* We have data, return it and come back for more later */
            return data_size;
        }
        if (pkt.data)
            av_free_packet(&pkt);
        if (quit)
        {
            return -1;
        }
        if (packet_queue_get(&audioQ, &pkt, 1) < 0)
        {
            printf("packet_queue_get error!\n");
            return -1;
        }
        audio_pkt_data = pkt.data;
        audio_pkt_size = pkt.size;
    }
    return 1;
}

//重采样
int resample(AVFrame *af, uint8_t *audio_buf, int *audio_buf_size)
{
    int data_size = 0;
    int resampled_data_size = 0;
    int64_t dec_channel_layout;
    data_size = av_samples_get_buffer_size(NULL,
            av_frame_get_channels(af),
            af->nb_samples,
            AVSampleFormat(af->format), 1);
    dec_channel_layout =(af->channel_layout&&
            av_frame_get_channels(af)== av_get_channel_layout_nb_channels(
                                    af->channel_layout)) ?
                    af->channel_layout :
                    av_get_default_channel_layout(av_frame_get_channels(af));
    if (af->format != audio_hw_params_src.fmt
            || af->sample_rate != audio_hw_params_src.freq
            || dec_channel_layout != audio_hw_params_src.channel_layout
            || !swr_ctx)
    {
        swr_free(&swr_ctx);
        swr_ctx = swr_alloc_set_opts(NULL, audio_hw_params_tgt.channel_layout,
                audio_hw_params_tgt.fmt, audio_hw_params_tgt.freq,
                dec_channel_layout, AVSampleFormat(af->format), af->sample_rate, 0, NULL);
        if (!swr_ctx || swr_init(swr_ctx) < 0)
        {
            av_log(NULL, AV_LOG_ERROR,
                    "Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
                    af->sample_rate, av_get_sample_fmt_name(AVSampleFormat(af->format)),
                    av_frame_get_channels(af), audio_hw_params_tgt.freq,
                    av_get_sample_fmt_name(audio_hw_params_tgt.fmt),
                    audio_hw_params_tgt.channels);
            swr_free(&swr_ctx);
            return -1;
        }
        printf("swr_init\n");
        audio_hw_params_src.channels = av_frame_get_channels(af);
        audio_hw_params_src.fmt = AVSampleFormat(af->format);
        audio_hw_params_src.freq = af->sample_rate;
    }

    if (swr_ctx)
    {
        const uint8_t **in = (const uint8_t **) af->extended_data;
        uint8_t **out = &audio_buf;
        int out_count = (int64_t) af->nb_samples * audio_hw_params_tgt.freq
                / af->sample_rate + 256;
        int out_size = av_samples_get_buffer_size(NULL,
                audio_hw_params_tgt.channels, out_count,
                audio_hw_params_tgt.fmt, 0);
        int len2;
        if (out_size < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
            return -1;
        }
        av_fast_malloc(&audio_buf, (unsigned int*)audio_buf_size, out_size);
        if (!audio_buf)
            return AVERROR(ENOMEM);
        len2 = swr_convert(swr_ctx, out, out_count, in, af->nb_samples);
        if (len2 < 0)
        {
            av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
            return -1;
        }
        if (len2 == out_count)
        {
            av_log(NULL, AV_LOG_WARNING,
                    "audio buffer is probably too small\n");
            if (swr_init(swr_ctx) < 0)
                swr_free(&swr_ctx);
        }
        resampled_data_size = len2 * audio_hw_params_tgt.channels
                * av_get_bytes_per_sample(audio_hw_params_tgt.fmt);
    }
    else
    {
        audio_buf = af->data[0];
        resampled_data_size = data_size;
    }

    return resampled_data_size;
}

