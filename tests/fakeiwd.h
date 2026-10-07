#pragma once

#include "dbustypes.h"

#include <QDBusConnection>
#include <QDBusVirtualObject>
#include <QHash>

// Minimal in-process stand-in for iwd, served on its own session-bus connection.
class FakeIwd : public QDBusVirtualObject
{
    Q_OBJECT

public:
    static constexpr auto Adapter = "/net/connman/iwd/0";
    static constexpr auto Device = "/net/connman/iwd/0/5";

    FakeIwd();
    ~FakeIwd() override;

    void start();
    void stop();

    // Seeds an adapter, a powered station, and the given networks.
    void addNetwork(const QString &ssid, const QString &type, int dbm, const QString &passphrase = {}, bool known = false);
    QString networkPath(const QString &ssid) const;
    QString knownPath(const QString &ssid) const;
    void setOrder(const QStringList &ssids, const QList<int> &dbms);
    void emitSignalLevel(uchar level);

    QVariant property(const QString &path, const QString &iface, const QString &key) const;
    bool has(const QString &path, const QString &iface) const;
    QString agentPath() const
    {
        return m_agentPath;
    }
    QString signalAgentPath() const
    {
        return m_signalAgentPath;
    }

    QString introspect(const QString &path) const override;
    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;

private:
    void setProperty(const QString &path, const QString &iface, const QString &key, const QVariant &value);
    void invalidateProperty(const QString &path, const QString &iface, const QString &key);
    void addInterface(const QString &path, const QString &iface, const QVariantMap &props);
    void removeInterface(const QString &path, const QString &iface);
    void handleConnect(const QDBusMessage &msg, const QString &networkPath);
    void markConnected(const QString &networkPath);
    void sendError(const QDBusMessage &msg, const char *name);

    QDBusConnection m_bus;
    QHash<QString, InterfaceMap> m_objects;
    QHash<QString, QString> m_passphrases;
    OrderedNetworkList m_ordered;
    QString m_agentService;
    QString m_agentPath;
    QString m_signalAgentService;
    QString m_signalAgentPath;
};
