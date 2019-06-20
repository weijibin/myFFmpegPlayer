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

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget)
{
    qDebug()<<"Widget======start";
    ui->setupUi(this);
    qDebug()<<"Widget======"<< avcodec_version();
    qDebug()<<"Widget======"<< avcodec_configuration();


    QTimer::singleShot(2000,[=](){
        this->testSDL();
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
    int i, videoStream;
    AVCodecContext *pCodecCtx;
    AVCodec *pCodec;
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
    i = 0;
    struct SwsContext *img_convert_ctx;
    int err_code;
    char buf[1024];
    FILE *fp_yuv;
    int y_size;
    //*******************************************************************************
    av_register_all();
    //1、打开视频文件*************************************************
    pFormatCtx = avformat_alloc_context();
    err_code = avformat_open_input(&pFormatCtx, filename, NULL, NULL);
    if (err_code != 0)
    {//打开文件失败
        av_strerror(err_code, buf, 1024);
        printf("coundn't open the file!,error code = %d(%s)\n", err_code, buf);
        return -1;
    }
    if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
    {
        printf("Couldn't find stream information.\n");
        return -1;
    }
    //2、找到第一个视频流****************************
    videoStream = -1;
    for (i = 0; i < pFormatCtx->nb_streams; i++)
    {
        if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;//得到视频流的索引
            break;
        }
    }
    if (videoStream == -1)
    {
        printf("Didn't find a video stream.\n");
        return -1;
    }
    /* 3、从视频流中得到一个编解码上下文,里面包含了编解码器的所有信息和一个
    指向真正的编解码器     ,然后我们找到这个解码器*/
    pCodecCtx = pFormatCtx->streams[videoStream]->codec;
    pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
    if (pCodec == NULL)
    {
        fprintf(stderr, "Unsupported codec !\n");
        return -1;
    }
    //4、打开该编解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        printf("cann't open the codec!\n");
        return -1;
    }
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
                SDL_Delay(40);//延时
            }
        }
        av_free_packet(&packet);//释放读出来的包
    }
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
    avformat_close_input(&pFormatCtx);
    return EXIT_SUCCESS;
}

void Widget::testSDL()
{
    //The window we'll be rendering to
    SDL_Window* gWindow = NULL;
    //The surface contained by the window
    SDL_Surface* gScreenSurface = NULL;
    //The image we will load and show on the screen
    SDL_Surface* gHelloWorld = NULL;
    //首先初始化   初始化SD视频子系统
    if(SDL_Init(SDL_INIT_VIDEO)<0) {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        return;
    }

    //创建窗口
    gWindow=SDL_CreateWindow("SHOW BMP",//窗口标题
                             SDL_WINDOWPOS_UNDEFINED,//窗口位置设置
                             SDL_WINDOWPOS_UNDEFINED,
                             SCREEN_WIDTH,//窗口的宽度
                             SCREEN_HEIGHT,//窗口的高度
                             SDL_WINDOW_SHOWN//显示窗口
                             );
    if(gWindow==NULL) {
        printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
        return ;
    }
    //Use this function to get the SDL surface associated with the window.
    //获取窗口对应的surface
    gScreenSurface=SDL_GetWindowSurface(gWindow);
    //加载图片
    gHelloWorld = SDL_LoadBMP("./orange.bmp");
    //加载图片
    if( gHelloWorld == NULL ) {
        qDebug()<<QString("Unable to load image %s! SDL Error: %1\n").arg(SDL_GetError());
        return ;
    }

    //Use this function to perform a fast surface copy to a destination surface.
    //surface的快速复制
    //下面函数的参数分别为： SDL_Surface* src ,const SDL_Rect* srcrect , SDL_Surface* dst ,  SDL_Rect* dstrect
    SDL_BlitSurface( gHelloWorld , NULL , gScreenSurface , NULL);
    SDL_UpdateWindowSurface(gWindow);//更新显示copy the window surface to the screen
    SDL_Delay(2000);//延时2000毫秒

    //释放内存
    SDL_FreeSurface( gHelloWorld );//释放空间
    gHelloWorld = NULL;
    SDL_DestroyWindow(gWindow);//销毁窗口
    gWindow = NULL ;
    SDL_Quit();//退出SDL
}
