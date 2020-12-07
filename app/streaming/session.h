#pragma once

#include <QObject>

#include "backend/nvapp.h"
#include "backend/nvcomputer.h"
#include "settings/streamingpreferences.h"

class Session : public QObject
{
    Q_OBJECT
    
public:
    explicit Session(NvComputer* computer, NvApp& app, StreamingPreferences *preferences = nullptr);

    virtual ~Session();
};