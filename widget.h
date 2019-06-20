#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavdevice/avdevice.h>
    #include <libavformat/version.h>
    #include <libavutil/time.h>
    #include <libavutil/mathematics.h>

    #include "libavfilter/buffersink.h"
    #include "libavfilter/buffersrc.h"
    #include "libavutil/avutil.h"
    #include "libavutil/imgutils.h"


    #include <SDL.h>

    #undef main   /* Prevents SDL from overriding main() */

}

namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

    void testSDL();

    int play();
private:
    Ui::Widget *ui;
};

#endif // WIDGET_H
