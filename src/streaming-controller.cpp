#include "streaming-controller.h"

#include <QtQml>
#include <QtDebug>
#include <QVideoFrame>
#include <QVideoSurfaceFormat>
#include <QAbstractVideoBuffer>

void StreamingController::registerQmlType()
{
    // @uri DesktopVideoProducer 
    qmlRegisterType<StreamingController>(
        "Moonlight.Streaming", 0, 1,
        "StreamingController");
}

StreamingController::StreamingController(QObject *parent)
    : QObject(parent), _surface(0)
{
    qDebug("Streaming Controller Created");

    QImage img;
    img.load("/media/developer/apps/usr/palm/applications/com.limelight.webos/assets/shiva.jpg");
    QSize sz = img.size();
    qDebug("Image: w: %d, h: %d, pxfmt: %d", sz.width(), sz.height(), img.pixelFormat());
    _frame = (img);
    setFormat(sz, _frame.pixelFormat());
}

StreamingController::~StreamingController()
{
    closeSurface();
    // End session here?
}

void StreamingController::setVideoSurface(QAbstractVideoSurface *s)
{
    qDebug("setVideoSurface %p", s);
    closeSurface();
    _surface = s;
    startSurface();
}

void StreamingController::setFormat(QSize size, QVideoFrame::PixelFormat pxfmt)
{
    QVideoSurfaceFormat format(size, pxfmt);
    _format = format;

    closeSurface();
    startSurface();
}

void StreamingController::onNativeResolutionChanged()
{
    if (!_surface || !_surface->isActive())
        return;
    QSize res = _surface->nativeResolution();
    qDebug("Video Surface res changed: %d*%d", res.width(), res.height());
}

void StreamingController::closeSurface()
{
    if (_surface && _surface->isActive())
        _surface->stop();
}

void StreamingController::startSurface()
{
    if (_surface && !_surface->isActive() && _format.isValid())
    {
        _format = _surface->nearestFormat(_format);
        QObject::connect(_surface, &QAbstractVideoSurface::nativeResolutionChanged,
                         this, &StreamingController::onNativeResolutionChanged);
        _surface->start(_format);
    }
}