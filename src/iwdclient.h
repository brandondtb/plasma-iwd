#pragma once

#include "dbustypes.h"

#include <QDBusConnection>
#include <QDBusPendingCall>
#include <QHash>
#include <QObject>

class QDBusMessage;
class QDBusServiceWatcher;

// Mirrors every object iwd exports via ObjectManager, keeping properties current.
class IwdClient : public QObject
{
    Q_OBJECT

public:
    explicit IwdClient(const QDBusConnection &bus, QObject *parent = nullptr);

    // System bus, or the session bus when IWD_CP_BUS=session (used by tests).
    static QDBusConnection defaultBus();

    QDBusConnection bus() const
    {
        return m_bus;
    }
    bool isAvailable() const
    {
        return m_available;
    }

    bool has(const QString &path, const QString &iface) const;
    QVariant property(const QString &path, const QString &iface, const QString &key) const;
    QStringList paths(const QString &iface) const;

    QDBusPendingCall call(const QString &path, const QString &iface, const QString &method, const QVariantList &args = {}) const;
    QDBusPendingCall setProperty(const QString &path, const QString &iface, const QString &key, const QVariant &value) const;

Q_SIGNALS:
    void availableChanged(bool available);
    void reset();
    void interfacesAdded(const QString &path, const QStringList &ifaces);
    void interfacesRemoved(const QString &path, const QStringList &ifaces);
    void propertiesChanged(const QString &path, const QString &iface, const QStringList &keys);

private Q_SLOTS:
    void onInterfacesAdded(const QDBusObjectPath &path, const InterfaceMap &ifaces);
    void onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &ifaces);
    void onPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated, const QDBusMessage &msg);

private:
    void load();
    void setAvailable(bool available);

    QDBusConnection m_bus;
    QDBusServiceWatcher *m_watcher;
    QHash<QString, InterfaceMap> m_objects;
    bool m_available = false;
};
