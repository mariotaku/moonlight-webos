#include "avcodec-check.h"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

AVCodecChecker::AVCodecChecker(QObject *parent) : QObject(parent)
{
    /* initialize libavcodec, and register all codecs and formats */
    av_register_all();
}

Q_INVOKABLE QString AVCodecChecker::listSupportedCodecs()
{
    QString codecs = "";
    /* Enumerate the codecs*/
    AVCodec *codec = av_codec_next(NULL);
    while (codec != NULL)
    {
        fprintf(stderr, "%s\n", codec->long_name);
        codec = av_codec_next(codec);
        codecs.append(codec->name);
        codecs.append("\n");
    }
    return codecs;
}
