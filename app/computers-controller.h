#ifndef COMPUTERS_CONTROLLER_HEADER_H
#define COMPUTERS_CONTROLLER_HEADER_H

#include <QObject>

#include "backend/nvcomputer.h"

class ComputersController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariant* computers READ computers NOTIFY computersChanged)
public:
    explicit ComputersController(QObject *parent = nullptr);
    ~ComputersController();

    static void registerQmlType();

private:
    QVariant _computers;
};

#endif //COMPUTERS_CONTROLLER_HEADER_H