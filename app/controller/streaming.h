#pragma once
#include <QtCore>

class StreamingController : public QObject
{
    Q_OBJECT
public:
    explicit StreamingController(QObject *parent = nullptr);
    ~StreamingController();

    static void registerQmlType();
    Q_INVOKABLE void testPlay();

private:
};