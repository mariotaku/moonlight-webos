#include "backend/computermanager.h"
// #include "streaming/session.h"

#include <QAbstractListModel>

class ComputerModel : public QAbstractListModel
{
    Q_OBJECT

    enum Roles
    {
        NameRole = Qt::UserRole,
        OnlineRole,
        PairedRole,
        BusyRole,
        WakeableRole,
        StatusUnknownRole
    };

public:
    explicit ComputerModel(QObject* object = nullptr);

    // Must be called before any QAbstractListModel functions
    Q_INVOKABLE void initialize(ComputerManager* computerManager);

    QVariant data(const QModelIndex &index, int role) const override;

    int rowCount(const QModelIndex &parent) const override;

    virtual QHash<int, QByteArray> roleNames() const override;

private slots:
    void handleComputerStateChanged(NvComputer* computer);

private:
    QVector<NvComputer*> m_Computers;
    ComputerManager* m_ComputerManager;
};