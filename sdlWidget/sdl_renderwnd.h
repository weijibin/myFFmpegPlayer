#ifndef SDL_RENDERWND_H
#define SDL_RENDERWND_H

#include <QWidget>
#include "widget.h"

struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Window;

class SDL_RenderWnd : public QWidget
{
    Q_OBJECT
public:
    explicit SDL_RenderWnd(QWidget *parent = 0);
    virtual ~SDL_RenderWnd();

    void clear();
signals:

public slots:

protected:
    virtual void resizeEvent(QResizeEvent*event) override;

private:
    static void SDL_Related_Init();
    static void SDL_Related_Uninit();

public slots:
    //根据传入数据流显示视频
    void PresentFrame(const unsigned char* pBuffer, int nImageWidth, int nImageHeight);
private:
    SDL_Renderer*        m_pRender;
    SDL_Texture*         m_pTexture = nullptr;
    SDL_Window*          m_pWindow;

    static int            m_nRef;        //引用计数来确定SDL全局资源的创建和回收

};

#endif // SDL_RENDERWND_H
