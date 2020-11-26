#ifndef AVCODEC_CHECKER_HEADER_H
#define AVCODEC_CHECKER_HEADER_H

#include <QObject>
#include <QString>

class AVCodecChecker : public QObject
{
    Q_OBJECT
public:
    explicit AVCodecChecker(QObject *parent = 0);
    Q_INVOKABLE QString listSupportedCodecs();
};

#endif //AVCODEC_CHECKER_HEADER_H