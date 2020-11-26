#include "codec-check.h"
#include <cstdio>
extern "C"
{
#include <gst/gst.h>
}

CodecChecker::CodecChecker(QObject *parent) : QObject(parent)
{
    if (!g_thread_supported())
        g_thread_init(NULL);

    gst_init(NULL, NULL);
}

Q_INVOKABLE QString CodecChecker::listSupportedCodecs()
{
    QString codecs = "";
    guint major, minor, micro, nano;

    gst_version(&major, &minor, &micro, &nano);
    printf("GStreamer %d.%d.%d\n", major, minor, micro);
    return QString("GStreamer %1.%2.%3").arg(QString::number(major), QString::number(minor), QString::number(micro));
}
