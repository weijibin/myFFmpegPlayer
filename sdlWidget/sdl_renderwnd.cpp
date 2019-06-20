#include "sdl_renderwnd.h"
#include <QDebug>

int SDL_RenderWnd::m_nRef = 0;

SDL_RenderWnd::SDL_RenderWnd(QWidget *parent) : QWidget(parent)
{
    setUpdatesEnabled(false);
    SDL_Related_Init();
    m_pWindow = SDL_CreateWindowFrom((void*)winId());
    m_pRender = SDL_CreateRenderer(m_pWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
}

SDL_RenderWnd::~SDL_RenderWnd()
{
    if (m_pWindow)
        SDL_DestroyWindow(m_pWindow);
    if (m_pRender)
        SDL_DestroyRenderer(m_pRender);
    if (m_pTexture)
        SDL_DestroyTexture(m_pTexture);
    SDL_Related_Uninit();
}

void SDL_RenderWnd::PresentFrame(const unsigned char* pBuffer, int nImageWidth, int nImageHeight)
{
    if (!m_pRender) {
        qDebug()<<("Render not Create\n");
    }
    else {
        int nTextureWidth = 0, nTextureHeight = 0;
        SDL_QueryTexture(m_pTexture, nullptr, nullptr, &nTextureWidth, &nTextureHeight);
        if (nTextureWidth != nImageWidth || nTextureHeight != nImageHeight) {
            if (m_pTexture)
                SDL_DestroyTexture(m_pTexture);

            m_pTexture = SDL_CreateTexture(m_pRender, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,
                nImageWidth, nImageHeight);
        }
    }

    if (!m_pTexture) {
        qDebug()<<("YUV Texture Create Failed\n");
    }
    else {

        SDL_UpdateTexture(m_pTexture, nullptr, pBuffer, nImageWidth);

        SDL_RenderClear(m_pRender);

        SDL_RenderCopy(m_pRender, m_pTexture, nullptr, nullptr);

        SDL_RenderPresent(m_pRender);
    }
}

void SDL_RenderWnd::resizeEvent(QResizeEvent *event)
{
    if (!m_pTexture) {
        qDebug()<<("YUV Texture Create Failed\n");
    }
    else {

        SDL_RenderClear(m_pRender);

        SDL_RenderCopy(m_pRender, m_pTexture, nullptr, nullptr);

        SDL_RenderPresent(m_pRender);
    }
}

void SDL_RenderWnd::clear()
{

}

void SDL_RenderWnd::SDL_Related_Init()
{
    if (0 == m_nRef++)
    {
        SDL_Init(SDL_INIT_VIDEO);
    }
}

void SDL_RenderWnd::SDL_Related_Uninit()
{
    if (0 == --m_nRef)
    {
        SDL_Quit();
    }
}
