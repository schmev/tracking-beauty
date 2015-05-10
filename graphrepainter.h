#ifndef GRAPHREPAINTER_H
#define GRAPHREPAINTER_H


#include <QLabel>
#include <QPaintEvent>
#include <QPainter>

#include "camera.h"
#include "ui_camera.h"

class GraphRepainter : public QObject
{
    Q_OBJECT

public:

    GraphRepainter( QLabel *graph, Camera *camera )
        : graph( graph ), camera( camera )
    {}

protected:

    bool eventFilter( QObject *obj, QEvent *event )
    {
        if( event->type() == QEvent::Paint )
        {
            QPaintEvent *paintEvent = static_cast<QPaintEvent *>(event);
            QPainter p( camera->ui->graphView );
            //p.setPen( Qt::green );

            Qt::GlobalColor pen_colours[] = { Qt::green, Qt::red, Qt::yellow, Qt::black };
            //p.drawImage(0, 0, *img);

            int w = camera->ui->graphView->width();
            int h = camera->ui->graphView->height();
            std::vector< MuseDataStream > &data = camera->muse_data;
            size_t idx_offset = camera->cur_ring_idx;
            const int num_samples = 500;
            float y_scale = .05;

            for( int channel = 0; channel < 4; ++channel )
            {
                p.setPen( pen_colours[channel] );

                for( size_t i = 0; i < num_samples; ++i )
                {
                    float x1 = w * i/(float)num_samples;
                    float y1 = (channel+1) * h/6 + y_scale*data[(i + idx_offset)%num_samples].eeg[channel];
                    float x2 = w * (i+1)/(float)num_samples;
                    float y2 = (channel+1) * h/6 + y_scale*data[((i+1) + idx_offset)%num_samples].eeg[channel];

                    QPoint p1(x1, y1), p2(x2, y2);
                    p.drawLine(p1, p2);
                }
            }

            //qDebug("**********eventFilter Paint event***********");
            return true;
        }
        else
        {
            // standard event processing
            return QObject::eventFilter( obj, event );
        }
    }

    QLabel *graph;
    Camera *camera;
    //std::vector< MuseDataStream > &muse_data;
};

#endif // GRAPHREPAINTER_H
