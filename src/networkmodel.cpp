#include "networkmodel.h"

#include "dbustypes.h"
#include "iwdclient.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

NetworkModel::NetworkModel(IwdClient *client, QObject *parent)
    : QAbstractListModel(parent)
    , m_client(client)
{
    connect(m_client, &IwdClient::propertiesChanged, this, [this](const QString &path, const QString &iface) {
        onPropertiesChanged(path, iface);
    });
    connect(m_client, &IwdClient::interfacesRemoved, this, [this](const QString &path, const QStringList &ifaces) {
        if (ifaces.contains(QLatin1String(Iwd::NetworkIface))) {
            for (int i = 0; i < m_entries.size(); ++i) {
                if (m_entries[i].path == path) {
                    beginRemoveRows({}, i, i);
                    m_entries.removeAt(i);
                    endRemoveRows();
                    return;
                }
            }
        }
    });
    // Known-ness comes from the Network's KnownNetwork property, but forgetting a network
    // removes the KnownNetwork object first; refresh so the row reflects it.
    connect(m_client, &IwdClient::interfacesRemoved, this, [this](const QString &, const QStringList &ifaces) {
        if (ifaces.contains(QLatin1String(Iwd::KnownNetworkIface)) && !m_entries.isEmpty()) {
            Q_EMIT dataChanged(index(0), index(m_entries.size() - 1), {KnownRole, KnownPathRole});
        }
    });
}

int NetworkModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_entries.size());
}

QVariant NetworkModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid)) {
        return {};
    }
    const Entry &e = m_entries[index.row()];
    const auto prop = [&](const char *key) {
        return m_client->property(e.path, QString::fromLatin1(Iwd::NetworkIface), QString::fromLatin1(key));
    };
    switch (role) {
    case PathRole:
        return e.path;
    case Qt::DisplayRole:
    case NameRole:
        return prop("Name");
    case TypeRole:
        return prop("Type");
    case SignalQualityRole:
        return Iwd::qualityFromDbm(e.dbm);
    case ConnectedRole:
        return prop("Connected").toBool();
    case KnownRole:
        return !data(index, KnownPathRole).toString().isEmpty();
    case KnownPathRole: {
        const QString known = prop("KnownNetwork").value<QDBusObjectPath>().path();
        return m_client->has(known, QString::fromLatin1(Iwd::KnownNetworkIface)) ? known : QString();
    }
    }
    return {};
}

QHash<int, QByteArray> NetworkModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {NameRole, "name"},
        {TypeRole, "type"},
        {SignalQualityRole, "signalQuality"},
        {ConnectedRole, "connected"},
        {KnownRole, "known"},
        {KnownPathRole, "knownPath"},
    };
}

void NetworkModel::setStation(const QString &stationPath)
{
    if (m_station == stationPath) {
        return;
    }
    m_station = stationPath;
    apply({});
    refresh();
}

void NetworkModel::refresh()
{
    const quint64 generation = ++m_generation;
    if (m_station.isEmpty()) {
        apply({});
        return;
    }
    const auto pending = m_client->call(m_station, QString::fromLatin1(Iwd::StationIface), QStringLiteral("GetOrderedNetworks"));
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, generation](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        // Drop stale replies that raced with a newer refresh or a station change.
        if (generation != m_generation) {
            return;
        }
        const QDBusPendingReply<OrderedNetworkList> reply = *w;
        if (reply.isError()) {
            qWarning("iwd: GetOrderedNetworks failed: %s", qPrintable(reply.error().message()));
            return;
        }
        QList<Entry> entries;
        for (const OrderedNetwork &n : reply.value()) {
            entries.append({n.path.path(), n.signal / 100});
        }
        apply(entries);
    });
}

// Incremental sync so QML delegates (and any open password field) survive a rescan.
void NetworkModel::apply(const QList<Entry> &entries)
{
    QSet<QString> wanted;
    for (const Entry &e : entries) {
        wanted.insert(e.path);
    }
    for (int i = int(m_entries.size()) - 1; i >= 0; --i) {
        if (!wanted.contains(m_entries[i].path)) {
            beginRemoveRows({}, i, i);
            m_entries.removeAt(i);
            endRemoveRows();
        }
    }

    for (int i = 0; i < entries.size(); ++i) {
        const Entry &want = entries[i];
        int found = -1;
        for (int j = i; j < m_entries.size(); ++j) {
            if (m_entries[j].path == want.path) {
                found = j;
                break;
            }
        }
        if (found < 0) {
            beginInsertRows({}, i, i);
            m_entries.insert(i, want);
            endInsertRows();
            continue;
        }
        if (found != i) {
            beginMoveRows({}, found, found, {}, i);
            m_entries.move(found, i);
            endMoveRows();
        }
        if (m_entries[i].dbm != want.dbm) {
            m_entries[i].dbm = want.dbm;
            Q_EMIT dataChanged(index(i), index(i), {SignalQualityRole});
        }
    }
}

void NetworkModel::onPropertiesChanged(const QString &path, const QString &iface)
{
    if (iface == QLatin1String(Iwd::StationIface) && path == m_station) {
        const bool scanDone = !m_client->property(path, iface, QStringLiteral("Scanning")).toBool();
        if (scanDone) {
            refresh();
        }
        return;
    }
    if (iface != QLatin1String(Iwd::NetworkIface)) {
        return;
    }
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].path == path) {
            Q_EMIT dataChanged(index(i), index(i));
            return;
        }
    }
}
