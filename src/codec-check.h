#ifndef CODEC_CHECKER_HEADER_H
#define CODEC_CHECKER_HEADER_H

#include <QObject>
#include <QString>

class CodecChecker : public QObject
{
    Q_OBJECT
public:
    explicit CodecChecker(QObject *parent = 0);
    Q_INVOKABLE QString listSupportedCodecs();
};

#endif //CODEC_CHECKER_HEADER_H