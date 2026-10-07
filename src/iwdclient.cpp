#include "iwdclient.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusServiceWatcher>
#include <QDBusVariant>

IwdClient::IwdClient(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_bus(bus)
    , m_watcher(new QDBusServiceWatcher(QString::fromLatin1(Iwd::Service), bus, QDBusServiceWatcher::WatchForOwnerChange, this))
{
    registerIwdDBusTypes();

    const QString service = QString::fromLatin1(Iwd::Service);
    const QString om = QString::fromLatin1(Iwd::ObjectManagerIface);
    m_bus.connect(service, QStringLiteral("/"), om, QStringLiteral("InterfacesAdded"), this, SLOT(onInterfacesAdded(QDBusObjectPath, InterfaceMap)));
    m_bus.connect(service, QStringLiteral("/"), om, QStringLiteral("InterfacesRemoved"), this, SLOT(onInterfacesRemoved(QDBusObjectPath, QStringList)));
    // Empty path: match PropertiesChanged from every iwd object.
    m_bus.connect(service,
                  QString(),
                  QString::fromLatin1(Iwd::PropertiesIface),
                  QStringLiteral("PropertiesChanged"),
                  this,
                  SLOT(onPropertiesChanged(QString, QVariantMap, QStringList, QDBusMessage)));

    connect(m_watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        if (newOwner.isEmpty()) {
            setAvailable(false);
        } else {
            load();
        }
    });

    if (m_bus.interface() && m_bus.interface()->isServiceRegistered(service)) {
        load();
    }
}

QDBusConnection IwdClient::defaultBus()
{
    return qEnvironmentVariable("IWD_CP_BUS") == QLatin1String("session") ? QDBusConnection::sessionBus() : QDBusConnection::systemBus();
}

bool IwdClient::has(const QString &path, const QString &iface) const
{
    const auto it = m_objects.constFind(path);
    return it != m_objects.cend() && it->contains(iface);
}

QVariant IwdClient::property(const QString &path, const QString &iface, const QString &key) const
{
    const auto it = m_objects.constFind(path);
    return it == m_objects.cend() ? QVariant() : it->value(iface).value(key);
}

QStringList IwdClient::paths(const QString &iface) const
{
    QStringList result;
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
        if (it->contains(iface)) {
            result << it.key();
        }
    }
    result.sort();
    return result;
}

QDBusPendingCall IwdClient::call(const QString &path, const QString &iface, const QString &method, const QVariantList &args) const
{
    auto msg = QDBusMessage::createMethodCall(QString::fromLatin1(Iwd::Service), path, iface, method);
    msg.setArguments(args);
    // Connect/passphrase flows can take a while; don't let the default 25s timeout cut them off.
    return m_bus.asyncCall(msg, 120 * 1000);
}

QDBusPendingCall IwdClient::setProperty(const QString &path, const QString &iface, const QString &key, const QVariant &value) const
{
    return call(path, QString::fromLatin1(Iwd::PropertiesIface), QStringLiteral("Set"), {iface, key, QVariant::fromValue(QDBusVariant(value))});
}

void IwdClient::load()
{
    const auto pending = call(QStringLiteral("/"), QString::fromLatin1(Iwd::ObjectManagerIface), QStringLiteral("GetManagedObjects"));
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<ManagedObjects> reply = *w;
        if (reply.isError()) {
            qWarning("iwd: GetManagedObjects failed: %s", qPrintable(reply.error().message()));
            return;
        }
        m_objects.clear();
        const ManagedObjects objects = reply.value();
        for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
            m_objects.insert(it.key().path(), it.value());
        }
        setAvailable(true);
        Q_EMIT reset();
    });
}

void IwdClient::setAvailable(bool available)
{
    if (!available && !m_objects.isEmpty()) {
        m_objects.clear();
        Q_EMIT reset();
    }
    if (m_available != available) {
        m_available = available;
        Q_EMIT availableChanged(available);
    }
}

void IwdClient::onInterfacesAdded(const QDBusObjectPath &path, const InterfaceMap &ifaces)
{
    InterfaceMap &object = m_objects[path.path()];
    for (auto it = ifaces.cbegin(); it != ifaces.cend(); ++it) {
        object.insert(it.key(), it.value());
    }
    Q_EMIT interfacesAdded(path.path(), ifaces.keys());
}

void IwdClient::onInterfacesRemoved(const QDBusObjectPath &path, const QStringList &ifaces)
{
    auto it = m_objects.find(path.path());
    if (it == m_objects.end()) {
        return;
    }
    for (const QString &iface : ifaces) {
        it->remove(iface);
    }
    if (it->isEmpty()) {
        m_objects.erase(it);
    }
    Q_EMIT interfacesRemoved(path.path(), ifaces);
}

void IwdClient::onPropertiesChanged(const QString &iface, const QVariantMap &changed, const QStringList &invalidated, const QDBusMessage &msg)
{
    auto it = m_objects.find(msg.path());
    if (it == m_objects.end() || !it->contains(iface)) {
        return;
    }
    QVariantMap &props = (*it)[iface];
    for (auto c = changed.cbegin(); c != changed.cend(); ++c) {
        props.insert(c.key(), c.value());
    }
    for (const QString &key : invalidated) {
        props.remove(key);
    }
    Q_EMIT propertiesChanged(msg.path(), iface, changed.keys() + invalidated);
}
