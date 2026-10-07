#include "fakeiwd.h"

#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QTimer>

namespace
{
QString str(const char *s)
{
    return QString::fromLatin1(s);
}

QString hexName(const QString &ssid, const QString &type)
{
    return QString::fromLatin1(ssid.toUtf8().toHex()) + u'_' + type;
}
}

FakeIwd::FakeIwd()
    : m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fakeiwd")))
{
    registerIwdDBusTypes();
    m_objects[str(Adapter)][str(Iwd::AdapterIface)] = {{QStringLiteral("Powered"), true}, {QStringLiteral("Name"), QStringLiteral("phy0")}};
    m_objects[str(Device)][str(Iwd::DeviceIface)] = {
        {QStringLiteral("Powered"), true},
        {QStringLiteral("Name"), QStringLiteral("wlan0")},
        {QStringLiteral("Mode"), QStringLiteral("station")},
        {QStringLiteral("Adapter"), QVariant::fromValue(QDBusObjectPath(str(Adapter)))},
    };
    m_objects[str(Device)][str(Iwd::StationIface)] = {{QStringLiteral("State"), QStringLiteral("disconnected")}, {QStringLiteral("Scanning"), false}};
    m_objects[str(Iwd::ManagerPath)][str(Iwd::AgentManagerIface)] = {};
}

FakeIwd::~FakeIwd()
{
    stop();
    QDBusConnection::disconnectFromBus(QStringLiteral("fakeiwd"));
}

void FakeIwd::start()
{
    m_bus.registerVirtualObject(QStringLiteral("/"), this, QDBusConnection::SubPath);
    m_bus.registerService(str(Iwd::Service));
}

void FakeIwd::stop()
{
    m_bus.unregisterService(str(Iwd::Service));
    m_bus.unregisterObject(QStringLiteral("/"));
    m_agentPath.clear();
    m_signalAgentPath.clear();
}

void FakeIwd::addNetwork(const QString &ssid, const QString &type, int dbm, const QString &passphrase, bool known)
{
    const QString path = str(Device) + u'/' + hexName(ssid, type);
    QVariantMap props{
        {QStringLiteral("Name"), ssid},
        {QStringLiteral("Type"), type},
        {QStringLiteral("Connected"), false},
        {QStringLiteral("Device"), QVariant::fromValue(QDBusObjectPath(str(Device)))},
    };
    if (known) {
        const QString kpath = str(Iwd::ManagerPath) + u'/' + hexName(ssid, type);
        addInterface(kpath,
                     str(Iwd::KnownNetworkIface),
                     {{QStringLiteral("Name"), ssid},
                      {QStringLiteral("Type"), type},
                      {QStringLiteral("Hidden"), false},
                      {QStringLiteral("AutoConnect"), true},
                      {QStringLiteral("LastConnectedTime"), QStringLiteral("2026-01-01T00:00:00Z")}});
        props.insert(QStringLiteral("KnownNetwork"), QVariant::fromValue(QDBusObjectPath(kpath)));
    }
    addInterface(path, str(Iwd::NetworkIface), props);
    m_passphrases.insert(path, passphrase);
    m_ordered.append({QDBusObjectPath(path), qint16(dbm * 100)});
}

QString FakeIwd::networkPath(const QString &ssid) const
{
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
        if (it->value(str(Iwd::NetworkIface)).value(QStringLiteral("Name")).toString() == ssid) {
            return it.key();
        }
    }
    return {};
}

QString FakeIwd::knownPath(const QString &ssid) const
{
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
        if (it->value(str(Iwd::KnownNetworkIface)).value(QStringLiteral("Name")).toString() == ssid) {
            return it.key();
        }
    }
    return {};
}

void FakeIwd::setOrder(const QStringList &ssids, const QList<int> &dbms)
{
    m_ordered.clear();
    for (int i = 0; i < ssids.size(); ++i) {
        m_ordered.append({QDBusObjectPath(networkPath(ssids[i])), qint16(dbms[i] * 100)});
    }
}

void FakeIwd::emitSignalLevel(uchar level)
{
    auto msg = QDBusMessage::createMethodCall(m_signalAgentService, m_signalAgentPath, str(Iwd::SignalLevelAgentIface), QStringLiteral("Changed"));
    msg.setArguments({QVariant::fromValue(QDBusObjectPath(str(Device))), QVariant::fromValue(level)});
    m_bus.asyncCall(msg);
}

QVariant FakeIwd::property(const QString &path, const QString &iface, const QString &key) const
{
    return m_objects.value(path).value(iface).value(key);
}

bool FakeIwd::has(const QString &path, const QString &iface) const
{
    return m_objects.value(path).contains(iface);
}

QString FakeIwd::introspect(const QString &) const
{
    return {};
}

void FakeIwd::setProperty(const QString &path, const QString &iface, const QString &key, const QVariant &value)
{
    m_objects[path][iface][key] = value;
    auto signal = QDBusMessage::createSignal(path, str(Iwd::PropertiesIface), QStringLiteral("PropertiesChanged"));
    signal.setArguments({iface, QVariantMap{{key, value}}, QStringList()});
    m_bus.send(signal);
}

void FakeIwd::invalidateProperty(const QString &path, const QString &iface, const QString &key)
{
    m_objects[path][iface].remove(key);
    auto signal = QDBusMessage::createSignal(path, str(Iwd::PropertiesIface), QStringLiteral("PropertiesChanged"));
    signal.setArguments({iface, QVariantMap(), QStringList{key}});
    m_bus.send(signal);
}

void FakeIwd::addInterface(const QString &path, const QString &iface, const QVariantMap &props)
{
    m_objects[path][iface] = props;
    auto signal = QDBusMessage::createSignal(QStringLiteral("/"), str(Iwd::ObjectManagerIface), QStringLiteral("InterfacesAdded"));
    signal.setArguments({QVariant::fromValue(QDBusObjectPath(path)), QVariant::fromValue(InterfaceMap{{iface, props}})});
    m_bus.send(signal);
}

void FakeIwd::removeInterface(const QString &path, const QString &iface)
{
    m_objects[path].remove(iface);
    if (m_objects[path].isEmpty()) {
        m_objects.remove(path);
    }
    auto signal = QDBusMessage::createSignal(QStringLiteral("/"), str(Iwd::ObjectManagerIface), QStringLiteral("InterfacesRemoved"));
    signal.setArguments({QVariant::fromValue(QDBusObjectPath(path)), QStringList{iface}});
    m_bus.send(signal);
}

void FakeIwd::sendError(const QDBusMessage &msg, const char *name)
{
    m_bus.send(msg.createErrorReply(str(name), str(name)));
}

bool FakeIwd::handleMessage(const QDBusMessage &msg, const QDBusConnection &)
{
    const QString path = msg.path();
    const QString iface = msg.interface();
    const QString member = msg.member();
    const QVariantList args = msg.arguments();

    if (iface == str(Iwd::ObjectManagerIface) && member == QLatin1String("GetManagedObjects")) {
        ManagedObjects objects;
        for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it) {
            objects.insert(QDBusObjectPath(it.key()), it.value());
        }
        m_bus.send(msg.createReply(QVariant::fromValue(objects)));
        return true;
    }

    if (iface == str(Iwd::PropertiesIface) && member == QLatin1String("Set")) {
        const QString targetIface = args[0].toString();
        const QString key = args[1].toString();
        const QVariant value = args[2].value<QDBusVariant>().variant();
        if (targetIface == str(Iwd::DeviceIface) && key == QLatin1String("Powered")) {
            if (value.toBool() && !property(str(Adapter), str(Iwd::AdapterIface), QStringLiteral("Powered")).toBool()) {
                sendError(msg, "net.connman.iwd.NotAvailable");
                return true;
            }
            setProperty(path, targetIface, key, value);
            if (value.toBool()) {
                addInterface(path, str(Iwd::StationIface), {{QStringLiteral("State"), QStringLiteral("disconnected")}, {QStringLiteral("Scanning"), false}});
            } else {
                removeInterface(path, str(Iwd::StationIface));
            }
        } else {
            setProperty(path, targetIface, key, value);
        }
        m_bus.send(msg.createReply());
        return true;
    }

    if (iface == str(Iwd::AgentManagerIface) && member == QLatin1String("RegisterAgent")) {
        m_agentService = msg.service();
        m_agentPath = args[0].value<QDBusObjectPath>().path();
        m_bus.send(msg.createReply());
        return true;
    }

    if (iface == str(Iwd::StationIface)) {
        if (!has(path, iface)) {
            sendError(msg, "net.connman.iwd.NotAvailable");
            return true;
        }
        if (member == QLatin1String("GetOrderedNetworks")) {
            m_bus.send(msg.createReply(QVariant::fromValue(m_ordered)));
        } else if (member == QLatin1String("Scan")) {
            setProperty(path, iface, QStringLiteral("Scanning"), true);
            m_bus.send(msg.createReply());
            QTimer::singleShot(0, this, [this, path] {
                setProperty(path, str(Iwd::StationIface), QStringLiteral("Scanning"), false);
            });
        } else if (member == QLatin1String("Disconnect")) {
            const QString connected = property(path, iface, QStringLiteral("ConnectedNetwork")).value<QDBusObjectPath>().path();
            if (!connected.isEmpty()) {
                setProperty(connected, str(Iwd::NetworkIface), QStringLiteral("Connected"), false);
            }
            setProperty(path, iface, QStringLiteral("State"), QStringLiteral("disconnected"));
            invalidateProperty(path, iface, QStringLiteral("ConnectedNetwork"));
            m_bus.send(msg.createReply());
        } else if (member == QLatin1String("RegisterSignalLevelAgent")) {
            m_signalAgentService = msg.service();
            m_signalAgentPath = args[0].value<QDBusObjectPath>().path();
            m_bus.send(msg.createReply());
        } else if (member == QLatin1String("ConnectHiddenNetwork")) {
            const QString ssid = args[0].toString();
            const QString hiddenPath = str(Device) + u'/' + hexName(ssid, QStringLiteral("psk"));
            addInterface(hiddenPath,
                         str(Iwd::NetworkIface),
                         {{QStringLiteral("Name"), ssid}, {QStringLiteral("Type"), QStringLiteral("psk")}, {QStringLiteral("Connected"), false}});
            m_passphrases.insert(hiddenPath, QStringLiteral("hiddenpass"));
            handleConnect(msg, hiddenPath);
        } else {
            return false;
        }
        return true;
    }

    if (iface == str(Iwd::NetworkIface) && member == QLatin1String("Connect")) {
        handleConnect(msg, path);
        return true;
    }

    if (iface == str(Iwd::KnownNetworkIface) && member == QLatin1String("Forget")) {
        for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
            if (it->value(str(Iwd::NetworkIface)).value(QStringLiteral("KnownNetwork")).value<QDBusObjectPath>().path() == path) {
                it.value()[str(Iwd::NetworkIface)].remove(QStringLiteral("KnownNetwork"));
            }
        }
        removeInterface(path, iface);
        m_bus.send(msg.createReply());
        return true;
    }

    return false;
}

void FakeIwd::handleConnect(const QDBusMessage &msg, const QString &networkPath)
{
    msg.setDelayedReply(true);
    const auto net = m_objects.value(networkPath).value(str(Iwd::NetworkIface));
    const bool known = net.contains(QStringLiteral("KnownNetwork"));
    if (net.value(QStringLiteral("Type")).toString() != QLatin1String("psk") || known) {
        markConnected(networkPath);
        m_bus.send(msg.createReply());
        return;
    }

    setProperty(str(Device), str(Iwd::StationIface), QStringLiteral("State"), QStringLiteral("connecting"));
    auto request = QDBusMessage::createMethodCall(m_agentService, m_agentPath, str(Iwd::AgentIface), QStringLiteral("RequestPassphrase"));
    request.setArguments({QVariant::fromValue(QDBusObjectPath(networkPath))});
    auto *watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(request), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, msg, networkPath](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        const QDBusPendingReply<QString> reply = *w;
        if (reply.isError()) {
            setProperty(str(Device), str(Iwd::StationIface), QStringLiteral("State"), QStringLiteral("disconnected"));
            sendError(msg, "net.connman.iwd.Aborted");
        } else if (reply.value() != m_passphrases.value(networkPath)) {
            setProperty(str(Device), str(Iwd::StationIface), QStringLiteral("State"), QStringLiteral("disconnected"));
            sendError(msg, "net.connman.iwd.Failed");
        } else {
            markConnected(networkPath);
            m_bus.send(msg.createReply());
        }
    });
}

void FakeIwd::markConnected(const QString &networkPath)
{
    const QString ssid = property(networkPath, str(Iwd::NetworkIface), QStringLiteral("Name")).toString();
    const QString type = property(networkPath, str(Iwd::NetworkIface), QStringLiteral("Type")).toString();
    const QString kpath = str(Iwd::ManagerPath) + u'/' + hexName(ssid, type);
    if (!has(kpath, str(Iwd::KnownNetworkIface))) {
        addInterface(kpath,
                     str(Iwd::KnownNetworkIface),
                     {{QStringLiteral("Name"), ssid},
                      {QStringLiteral("Type"), type},
                      {QStringLiteral("Hidden"), false},
                      {QStringLiteral("AutoConnect"), true},
                      {QStringLiteral("LastConnectedTime"), QStringLiteral("2026-10-07T00:00:00Z")}});
        setProperty(networkPath, str(Iwd::NetworkIface), QStringLiteral("KnownNetwork"), QVariant::fromValue(QDBusObjectPath(kpath)));
    }
    setProperty(networkPath, str(Iwd::NetworkIface), QStringLiteral("Connected"), true);
    setProperty(str(Device), str(Iwd::StationIface), QStringLiteral("ConnectedNetwork"), QVariant::fromValue(QDBusObjectPath(networkPath)));
    setProperty(str(Device), str(Iwd::StationIface), QStringLiteral("State"), QStringLiteral("connected"));
}
