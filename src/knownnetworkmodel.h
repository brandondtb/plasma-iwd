#pragma once

#include <QAbstractListModel>

class IwdClient;

// Saved networks, most recently used first.
class KnownNetworkModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        HiddenRole,
        AutoConnectRole,
        LastConnectedRole,
    };
    Q_ENUM(Role)

    explicit KnownNetworkModel(IwdClient *client, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    void reload();
    QString lastConnected(const QString &path) const;
    int insertPosition(const QString &path) const;

    IwdClient *m_client;
    QStringList m_paths;
};
