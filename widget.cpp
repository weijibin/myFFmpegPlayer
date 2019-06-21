#include "widget.h"
#include "ui_widget.h"
#include <QDebug>
#include <QTimer>
#include <stdio.h>

using namespace std;
const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;

//是否将YUV420P内容输出到文件
#define OUTPUT_YUV420P 0
//要播放的文件路径
#define filename "./good.mp4"
//要输出YUV420P内容的文件路径
#define outfilename "./output.yuv"

#include "audiodefine.h"

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    qDebug()<<"Widget======start";
    ui->setupUi(this);
    qDebug()<<"Widget======"<< avcodec_version();
    qDebug()<<"Widget======"<< avcodec_configuration();


    QTimer::singleShot(2000,[=](){
        qDebug()<<"play";
        play();
    });

    qDebug()<<"Widget======end";
}

Widget::~Widget()
{
    delete ui;
}

int Widget::play()
{

    //变量定义*********************************************************************
        AVFormatContext *pFormatCtx;
        int i=0;
        int videoStream;
        int audioStream;
        AVCodecContext *pCodecCtx;
        AVCodecContext  *aCodecCtxOrig;
        AVCodecContext *aCodecCtx;
        AVCodec *pCodec;
        AVCodec *aCodec;
        AVFrame *pFrame;
        AVFrame *pFrameYUV;
        uint8_t *buffer;
        int numBytes;

        SDL_Window *screen;
        SDL_Renderer *sdlRender;
        SDL_Texture *sdlTexture;
        SDL_Rect sdlRect;
        int frameFinished;
        AVPacket packet;
        struct SwsContext *img_convert_ctx;
        int err_code;
        char buf[1024];
        FILE *fp_yuv;
        int y_size;
        SDL_AudioSpec audioSpec;
        SDL_AudioSpec spec;
        SDL_Event       event;
        //*******************************************************************************
        av_register_all();
        //1、打开视频文件*************************************************
        pFormatCtx = avformat_alloc_context();
        err_code = avformat_open_input(&pFormatCtx, filename, NULL, NULL);
        if (err_code != 0)
        {//打开文件失败
            av_strerror(err_code, buf, 1024);
            printf("coundn't open the file!,error code = %d(%s)\n", err_code, buf);
//            return -1;
        }
        if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
        {
            printf("Couldn't find stream information.\n");
            return -1;
        }
        // 打印信息
          av_dump_format(pFormatCtx, 0, filename, 0);
        //2、找到第一个视频流和第一个音频流****************************
        videoStream = -1;
        audioStream = -1;
        for (i = 0; i < pFormatCtx->nb_streams; i++)
        {
            if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO && videoStream<0)
            {
                videoStream = i;//得到视频流的索引
            }
            if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&audioStream < 0)
            {
                audioStream = i;//得到音频流的索引
            }
        }
        if (videoStream == -1)
        {
            printf("Didn't find a video stream.\n");
            return -1;
        }
        if(audioStream == -1)
        {
            printf("coundn't find a audio stream!\n");
            return -1;
        }
        /* 3、从视频流中得到一个音频和视频编解码上下文,里面包含了编解码器的所有信息和一个
        指向真正的编解码器     ,然后我们找到音频和视频编解码器*/
        pCodecCtx = pFormatCtx->streams[videoStream]->codec;
        aCodecCtxOrig = pFormatCtx->streams[audioStream]->codec;
        pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
        aCodec = avcodec_find_decoder(aCodecCtxOrig->codec_id);
        if (pCodec == NULL)
        {
            fprintf(stderr, "Unsupported codec !\n");
            return -1;
        }
        if(aCodec == NULL)
        {
            fprintf(stderr,"Unsupported codec!\n");
            return -1;
        }
        //拷贝上下文
        aCodecCtx = avcodec_alloc_context3(aCodec);
        if(avcodec_copy_context(aCodecCtx,aCodecCtxOrig) != 0)
        {
            fprintf(stderr,"couldn't copy codec context!\n");
            return -1;
        }
        //4、打开音频和视频编解码器
        if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
        {
            printf("cann't open the codec!\n");
            return -1;
        }
        if(avcodec_open2(aCodecCtx,aCodec,NULL) < 0 )
        {
            printf("cann't open the audio codec!\n");
            return -1;
        }
        //设置声音参数
        sample_rate = aCodecCtx->sample_rate;
        nb_channels = aCodecCtx->channels;
        channel_layout = aCodecCtx->channel_layout;

    //    printf("channel_layout=%" PRId64 "\n", channel_layout);
        printf("nb_channels=%d\n", nb_channels);
        printf("freq=%d\n", sample_rate);

        if (!channel_layout|| nb_channels != av_get_channel_layout_nb_channels(channel_layout))
        {
            channel_layout = av_get_default_channel_layout(nb_channels);
            channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
            printf("correction\n");
        }
        /*通故编解码上下文中的所有信息来建立音频的信息*/
        audioSpec.freq = aCodecCtx->sample_rate;
        audioSpec.format = AUDIO_S16SYS;
        audioSpec.channels = aCodecCtx->channels;
        audioSpec.silence = 0;
        audioSpec.samples = SDL_AUDIO_BUFFER_SIZE;
        audioSpec.callback = AudioDefine::audio_callback;
        audioSpec.userdata = aCodecCtx;
        //打开音频设备和初始化
        if(SDL_OpenAudio(&audioSpec,&spec) < 0)
        //其中回调函数在需要更多音频数据的时候被调用（即播放完后需要从回调取数据播放）
        {
            fprintf(stderr,"SDL_OpenAudio:    %s\n",SDL_GetError());
            qDebug()<<"SDL_OpenAudio:    "<<SDL_GetError();
//            return -1;
        }
        printf("freq: %d\tchannels: %d\n", spec.freq, spec.channels);
        //5、分配两个视频帧，一个保存得到的原始视频帧，一个保存为指定格式的视频帧（该帧通过原始帧转换得来）
        pFrame = av_frame_alloc();
        if (pFrame == NULL)
        {
            printf("pFrame alloc fail!\n");
            return -1;
        }
        pFrameYUV = av_frame_alloc();
        if (pFrameYUV == NULL)
        {
            printf("pFrameYUV alloc fail!\n");
            return -1;
        }
        //6、得到一帧视频截图的内存大小并分配内存，并将YUV数据填充进去
        numBytes = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width,
                pCodecCtx->height,1);
        buffer = (uint8_t*) av_mallocz(numBytes * sizeof(uint8_t));
        if (!buffer)
        {
            printf("numBytes :%d , buffer malloc 's mem \n", numBytes);
            return -1;
        }
        //打印信息
        printf("--------------- File Information ----------------\n");
        av_dump_format(pFormatCtx, 0, filename, 0);
        printf("-------------------------------------------------\n");
        av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,buffer,
                AV_PIX_FMT_YUV420P,pCodecCtx->width, pCodecCtx->height,1);
        //7、得到指定转换格式的上下文**********************************
        img_convert_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                pCodecCtx->pix_fmt, pCodecCtx->width, pCodecCtx->height,
                AV_PIX_FMT_YUV420P,
                SWS_BICUBIC,
                NULL, NULL, NULL);
        if (img_convert_ctx == NULL)
        {
            fprintf(stderr, "Cannot initialize the conversion context!\n");
            return -1;
        }
        //***********************************************************
    #if OUTPUT_YUV420P
        fp_yuv = fopen(outfilename, "wb+");
    #endif
        //8、SDL初始化和创建多重windows等准备工作
        if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_VIDEO))
        {
            fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
            return -1;
        }
        //使用SDL_CreateWindow代替SDL_SetVideoMode
        //创建一个给定高度和宽度、位置和标示的windows。
        screen = SDL_CreateWindow("Simplest ffmpeg player's Window",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, pCodecCtx->width,
                pCodecCtx->height, SDL_WINDOW_OPENGL);
        if (!screen)
        {
            fprintf(stderr, "SDL: could not create window - exiting - %s\n",SDL_GetError());
            return -1;
        }
        //对该window创建一个2D渲染上下文
        sdlRender = SDL_CreateRenderer(screen, -1, 0);
        if (!sdlRender)
        {
            fprintf(stderr, "SDL:cound not create render :    %s\n", SDL_GetError());
            return -1;
        }
        //Create a texture for a rendering context.
        //为一个渲染上下文创建一个纹理
        //IYUV: Y + U + V  (3 planes)
        //YV12: Y + V + U  (3 planes)
        sdlTexture = SDL_CreateTexture(sdlRender, SDL_PIXELFORMAT_IYUV,
                SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width, pCodecCtx->height);
        if (!sdlTexture)
        {
            fprintf(stderr, "SDL:cound not create Texture :    %s\n", SDL_GetError());
            return -1;
        }
        //建立一个矩形变量，提供后面使用
        sdlRect.x = 0;
        sdlRect.y = 0;
        sdlRect.w = pCodecCtx->width;
        sdlRect.h = pCodecCtx->height;
        //*****************************************************
        //声音部分代码
        audio_hw_params_tgt.fmt = AV_SAMPLE_FMT_S16;
        audio_hw_params_tgt.freq = spec.freq;
        audio_hw_params_tgt.channel_layout = channel_layout;
        audio_hw_params_tgt.channels = spec.channels;
        audio_hw_params_tgt.frame_size = av_samples_get_buffer_size(NULL,
        audio_hw_params_tgt.channels, 1, audio_hw_params_tgt.fmt, 1);
        audio_hw_params_tgt.bytes_per_sec = av_samples_get_buffer_size(NULL,
                audio_hw_params_tgt.channels, audio_hw_params_tgt.freq,
                audio_hw_params_tgt.fmt, 1);
        if (audio_hw_params_tgt.bytes_per_sec <= 0|| audio_hw_params_tgt.frame_size <= 0)
        {
            printf("size error\n");
            return -1;
        }
        audio_hw_params_src = audio_hw_params_tgt;
        //*****************************************************
        packet_queue_init(&audioQ);
        SDL_PauseAudio(0);
        //9、正式开始读取数据*****************************************
        while (av_read_frame(pFormatCtx, &packet) >= 0)
        {
            //如果读取的包来自视频流
            if (packet.stream_index == videoStream)
            {
                //从包中得到解码后的帧
                if (avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,&packet) < 0)
                {
                    printf("Decode Error!\n");
                    return -1;
                }
                //如果确定完成得到该视频帧
                if (frameFinished)
                {
                    //转换帧数据格式
                    sws_scale(img_convert_ctx, pFrame->data, pFrame->linesize, 0,
                            pCodecCtx->height,
                            pFrameYUV->data,
                            pFrameYUV->linesize);
    #if OUTPUT_YUV420P
                    y_size = pCodecCtx->width * pCodecCtx->height;
                    fwrite(pFrameYUV->data[0], 1, y_size, fp_yuv);    //Y
                    fwrite(pFrameYUV->data[1], 1, y_size / 4, fp_yuv);  //U
                    fwrite(pFrameYUV->data[2], 1, y_size / 4, fp_yuv);  //V
    #endif
                    //SDL显示~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    #if 0
                    SDL_UpdateTexture(sdlTexture,NULL,pFrameYUV->data[0],pFrameYUV->linesize[0]);
    #else
                    SDL_UpdateYUVTexture(sdlTexture, &sdlRect, pFrameYUV->data[0],
                            pFrameYUV->linesize[0], pFrameYUV->data[1],
                            pFrameYUV->linesize[1], pFrameYUV->data[2],
                            pFrameYUV->linesize[2]);
    #endif
                    SDL_RenderClear(sdlRender);
                    SDL_RenderCopy(sdlRender, sdlTexture, NULL, &sdlRect);
                    SDL_RenderPresent(sdlRender);
                    //结束SDL~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                }
            }
            else if(packet.stream_index == audioStream)
            {
                    packet_queue_put(&audioQ,&packet);
            }else{
                    av_free_packet(&packet);//释放读出来的包
            }
            SDL_PollEvent(&event);
            switch (event.type)
            {
            case SDL_QUIT:
                quit = 1;
                SDL_Quit();
                exit(0);
                break;
            default:
                break;
            }
        }
        while(1) SDL_Delay(1000);
        //**************************************************************************************
        //10、释放分配的内存或关闭文件等操作
    #if OUTPUT_YUV420P
        fclose(fp_yuv);
    #endif
        sws_freeContext(img_convert_ctx);
        SDL_Quit();
        av_free(buffer);
        av_free(pFrame);
        av_free(pFrameYUV);
        avcodec_close(pCodecCtx);
        avcodec_close(aCodecCtxOrig);
        avcodec_close(aCodecCtx);
        avformat_close_input(&pFormatCtx);
        return EXIT_SUCCESS;
}
