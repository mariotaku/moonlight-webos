#pragma once

#include "backend/nvcomputer.h"

#include <qmdnsengine/server.h>
#include <qmdnsengine/cache.h>
#include <qmdnsengine/browser.h>
#include <qmdnsengine/service.h>
#include <qmdnsengine/resolver.h>

#include <QObject>
#include <QVector>

#include <QThread>
#include <QReadWriteLock>
#include <QRunnable>
#include <QTimer>

class MdnsPendingComputer : public QObject
{
    Q_OBJECT

public:
    explicit MdnsPendingComputer(QMdnsEngine::Server* server,
                                 const QMdnsEngine::Service& service)
        : m_Hostname(service.hostname()),
          m_Server(server),
          m_Resolver(nullptr)
    {
        // Start resolving
        resolve();
    }

    virtual ~MdnsPendingComputer()
    {
        delete m_Resolver;
    }

    QString hostname()
    {
        return m_Hostname;
    }

private slots:
    void handleResolvedTimeout()
    {
        if (m_Addresses.isEmpty()) {
            // Try again
            qInfo() << "Resolving" << hostname() << "timed out. Retrying...";
            resolve();
        }
        else {
            Q_ASSERT(!m_Addresses.isEmpty());
            emit resolvedHost(this, m_Addresses);
        }
    }

    void handleResolvedAddress(const QHostAddress& address)
    {
        qInfo() << "Resolved" << hostname() << "to" << address;
        m_Addresses.push_back(address);
    }

signals:
    void resolvedHost(MdnsPendingComputer*,QVector<QHostAddress>&);

private:
    void resolve()
    {
        delete m_Resolver;
        m_Resolver = new QMdnsEngine::Resolver(m_Server, m_Hostname);
        connect(m_Resolver, &QMdnsEngine::Resolver::resolved,
                this, &MdnsPendingComputer::handleResolvedAddress);
        QTimer::singleShot(2000, this, SLOT(handleResolvedTimeout()));
    }

    QByteArray m_Hostname;
    QMdnsEngine::Server* m_Server;
    QMdnsEngine::Resolver* m_Resolver;
    QVector<QHostAddress> m_Addresses;
};

class ComputerPollingEntry
{
public:
    ComputerPollingEntry()
        : m_ActiveThread(nullptr)
    {

    }

    virtual ~ComputerPollingEntry()
    {
        interrupt();

        // interrupt() should have taken care of this
        Q_ASSERT(m_ActiveThread == nullptr);

        for (QThread* thread : m_InactiveList) {
            thread->wait();
            delete thread;
        }
    }

    bool isActive()
    {
        cleanInactiveList();

        return m_ActiveThread != nullptr;
    }

    void setActiveThread(QThread* thread)
    {
        cleanInactiveList();

        Q_ASSERT(!isActive());
        m_ActiveThread = thread;
    }

    void interrupt()
    {
        cleanInactiveList();

        if (m_ActiveThread != nullptr) {
            // Interrupt the active thread
            m_ActiveThread->requestInterruption();

            // Place it on the inactive list awaiting death
            m_InactiveList.append(m_ActiveThread);

            m_ActiveThread = nullptr;
        }
    }

private:
    void cleanInactiveList()
    {
        QMutableListIterator<QThread*> i(m_InactiveList);

        // Reap any threads that have finished
        while (i.hasNext()) {
            i.next();

            QThread* thread = i.value();
            if (thread->isFinished()) {
                delete thread;
                i.remove();
            }
        }
    }

    QThread* m_ActiveThread;
    QList<QThread*> m_InactiveList;
};

class ComputerManager : public QObject
{
    Q_OBJECT
    
    friend class DeferredHostDeletionTask;
    friend class PendingAddTask;

public:
    explicit ComputerManager(QObject *parent = nullptr);

    virtual ~ComputerManager();

    Q_INVOKABLE void startPolling();

    Q_INVOKABLE void stopPollingAsync();

    Q_INVOKABLE void addNewHost(QString address, bool mdns, QHostAddress mdnsIpv6Address = QHostAddress());

    QVector<NvComputer*> getComputers();

signals:
    void computerStateChanged(NvComputer* computer);

    void computerAddCompleted(QVariant success, QVariant detectedPortBlocking);
    
    void quitAppCompleted(QVariant error);


private slots:

    void handleComputerStateChanged(NvComputer* computer);

    void handleMdnsServiceResolved(MdnsPendingComputer* computer, QVector<QHostAddress>& addresses);


private:
    void saveHosts();

    QHostAddress getBestGlobalAddressV6(QVector<QHostAddress>& addresses);

    void startPollingComputer(NvComputer* computer);

    int m_PollingRef;
    QReadWriteLock m_Lock;
    QMap<QString, NvComputer*> m_KnownHosts;
    QMap<QString, ComputerPollingEntry*> m_PollEntries;
    QMdnsEngine::Server m_MdnsServer;
    QMdnsEngine::Browser* m_MdnsBrowser;
    QMdnsEngine::Cache m_MdnsCache;
    QVector<MdnsPendingComputer*> m_PendingResolution;
};