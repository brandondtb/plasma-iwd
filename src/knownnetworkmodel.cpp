#include "knownnetworkmodel.h"

#include "dbustypes.h"
#include "iwdclient.h"

#include <QDateTime>

#include <algorithm>

KnownNetworkModel::KnownNetworkModel(IwdClient *client, QObject *parent)
    : QAbstractListModel(parent)
    , m_client(client)
{
    const QString iface = QString::fromLatin1(Iwd::KnownNetworkIface);

    connect(m_client, &IwdClient::reset, this, &KnownNetworkModel::reload);
    connect(m_client, &IwdClient::interfacesAdded, this, [this, iface](const QString &path, const QStringList &ifaces) {
        if (ifaces.contains(iface) && !m_paths.contains(path)) {
            const int row = insertPosition(path);
            beginInsertRows({}, row, row);
            m_paths.insert(row, path);
            endInsertRows();
        }
    });
    connect(m_client, &IwdClient::interfacesRemoved, this, [this, iface](const QString &path, const QStringList &ifaces) {
        const int row = int(m_paths.indexOf(path));
        if (ifaces.contains(iface) && row >= 0) {
            beginRemoveRows({}, row, row);
            m_paths.removeAt(row);
            endRemoveRows();
        }
    });
    connect(m_client, &IwdClient::propertiesChanged, this, [this, iface](const QString &path, const QString &changedIface) {
        const int row = int(m_paths.indexOf(path));
        if (changedIface == iface && row >= 0) {
            Q_EMIT dataChanged(index(row), index(row));
        }
    });

    reload();
}

int KnownNetworkModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_paths.size());
}

QVariant KnownNetworkModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const QString &path = m_paths[index.row()];
    const auto prop = [&](const char *key) {
        return m_client->property(path, QString::fromLatin1(Iwd::KnownNetworkIface), QString::fromLatin1(key));
    };
    switch (role) {
    case PathRole:
        return path;
    case Qt::DisplayRole:
    case NameRole:
        return prop("Name");
    case TypeRole:
        return prop("Type");
    case HiddenRole:
        return prop("Hidden").toBool();
    case AutoConnectRole:
        return prop("AutoConnect").toBool();
    case LastConnectedRole:
        return QDateTime::fromString(prop("LastConnectedTime").toString(), Qt::ISODate);
    }
    return {};
}

QHash<int, QByteArray> KnownNetworkModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {HiddenRole, "hidden"},
        {AutoConnectRole, "autoConnect"},
        {LastConnectedRole, "lastConnected"},
    };
}

void KnownNetworkModel::reload()
{
    beginResetModel();
    m_paths = m_client->paths(QString::fromLatin1(Iwd::KnownNetworkIface));
    // ISO 8601 UTC timestamps sort lexically; never-connected (empty) sorts last.
    std::stable_sort(m_paths.begin(), m_paths.end(), [this](const QString &a, const QString &b) {
        return lastConnected(a) > lastConnected(b);
    });
    endResetModel();
}

QString KnownNetworkModel::lastConnected(const QString &path) const
{
    return m_client->property(path, QString::fromLatin1(Iwd::KnownNetworkIface), QStringLiteral("LastConnectedTime")).toString();
}

int KnownNetworkModel::insertPosition(const QString &path) const
{
    const QString key = lastConnected(path);
    const auto it = std::find_if(m_paths.cbegin(), m_paths.cend(), [&](const QString &p) {
        return lastConnected(p) < key;
    });
    return int(it - m_paths.cbegin());
}
