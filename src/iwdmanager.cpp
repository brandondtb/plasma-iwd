#include "iwdmanager.h"

#include "agent.h"
#include "dbustypes.h"
#include "iwdclient.h"
#include "knownnetworkmodel.h"
#include "networkmodel.h"

#include <QDBusPendingCallWatcher>

namespace
{
QString str(const char *s)
{
    return QString::fromLatin1(s);
}

QString humanError(const QDBusError &error)
{
    const QString name = error.name();
    if (name == QLatin1String("net.connman.iwd.Failed") || name == QLatin1String("net.connman.iwd.InvalidFormat")) {
        return QStringLiteral("Connection failed. Check the passphrase.");
    }
    if (name == QLatin1String("net.connman.iwd.Aborted") || name == QLatin1String("net.connman.iwd.Agent.Error.Canceled")) {
        return {};
    }
    if (name == QLatin1String("net.connman.iwd.NotFound")) {
        return QStringLiteral("Network not found.");
    }
    if (name == QLatin1String("org.freedesktop.DBus.Error.AccessDenied")) {
        return QStringLiteral("Permission denied by iwd's D-Bus policy.");
    }
    return error.message().isEmpty() ? name : error.message();
}
}

IwdManager::IwdManager(const QDBusConnection &bus, QObject *parent)
    : QObject(parent)
    , m_client(new IwdClient(bus, this))
    , m_agent(new Agent(m_client, this))
    , m_networks(new NetworkModel(m_client, this))
    , m_knownNetworks(new KnownNetworkModel(m_client, this))
{
    connect(m_client, &IwdClient::availableChanged, this, [this](bool available) {
        if (available) {
            m_agent->registerWithIwd();
        }
    });
    connect(m_client, &IwdClient::reset, this, [this] {
        m_signalRegisteredFor.clear();
        m_agentLevel = -1;
        updateDevice();
        m_networks->refresh();
    });
    connect(m_client, &IwdClient::interfacesAdded, this, &IwdManager::updateDevice);
    connect(m_client, &IwdClient::interfacesRemoved, this, &IwdManager::updateDevice);
    connect(m_client, &IwdClient::propertiesChanged, this, [this](const QString &path, const QString &iface) {
        if (path == m_device || iface == QLatin1String(Iwd::AdapterIface) || iface == QLatin1String(Iwd::NetworkIface)) {
            Q_EMIT changed();
        }
    });

    // signalQuality() falls back to scan results, so follow the model too.
    connect(m_networks, &QAbstractItemModel::dataChanged, this, &IwdManager::changed);
    connect(m_networks, &QAbstractItemModel::rowsInserted, this, &IwdManager::changed);

    connect(m_agent, &Agent::passphraseRequested, this, [this](const QString &networkPath) {
        // A hidden network has no row in the UI to prompt from; the user had their chance in the form.
        if (m_hiddenInProgress) {
            m_agent->cancel(networkPath);
            Q_EMIT connectFailed(QString(), QStringLiteral("This network requires a passphrase."));
            return;
        }
        Q_EMIT passphraseRequested(networkPath);
    });
    connect(m_agent, &Agent::requestCanceled, this, &IwdManager::passphraseRequestCanceled);
    connect(m_agent, &Agent::signalLevel, this, &IwdManager::onSignalLevel);

    updateDevice();
}

IwdManager::~IwdManager() = default;

bool IwdManager::isAvailable() const
{
    return m_client->isAvailable();
}

bool IwdManager::hasDevice() const
{
    return !m_device.isEmpty();
}

QVariant IwdManager::deviceProperty(const char *iface, const char *key) const
{
    return m_client->property(m_device, str(iface), str(key));
}

bool IwdManager::isPowered() const
{
    if (m_device.isEmpty() || !deviceProperty(Iwd::DeviceIface, "Powered").toBool()) {
        return false;
    }
    const QString adapter = deviceProperty(Iwd::DeviceIface, "Adapter").value<QDBusObjectPath>().path();
    return m_client->property(adapter, str(Iwd::AdapterIface), QStringLiteral("Powered")).toBool();
}

QString IwdManager::state() const
{
    if (!isAvailable() || m_device.isEmpty()) {
        return QStringLiteral("unavailable");
    }
    if (!isPowered() || !m_client->has(m_device, str(Iwd::StationIface))) {
        return QStringLiteral("off");
    }
    return deviceProperty(Iwd::StationIface, "State").toString();
}

bool IwdManager::isScanning() const
{
    return deviceProperty(Iwd::StationIface, "Scanning").toBool();
}

QString IwdManager::connectedPath() const
{
    return deviceProperty(Iwd::StationIface, "ConnectedNetwork").value<QDBusObjectPath>().path();
}

QString IwdManager::connectedSsid() const
{
    const QString path = connectedPath();
    return path.isEmpty() ? QString() : m_client->property(path, str(Iwd::NetworkIface), QStringLiteral("Name")).toString();
}

int IwdManager::signalQuality() const
{
    if (connectedPath().isEmpty()) {
        return 0;
    }
    if (m_agentLevel >= 0) {
        return Iwd::qualityFromLevel(m_agentLevel);
    }
    // Before the SignalLevelAgent reports, fall back to the last scan result.
    const auto *model = m_networks;
    for (int i = 0; i < model->rowCount(); ++i) {
        const QModelIndex idx = model->index(i);
        if (idx.data(NetworkModel::PathRole).toString() == connectedPath()) {
            return idx.data(NetworkModel::SignalQualityRole).toInt();
        }
    }
    return 0;
}

QString IwdManager::deviceName() const
{
    return deviceProperty(Iwd::DeviceIface, "Name").toString();
}

QAbstractItemModel *IwdManager::networks() const
{
    return m_networks;
}

QAbstractItemModel *IwdManager::knownNetworks() const
{
    return m_knownNetworks;
}

void IwdManager::updateDevice()
{
    // Prefer a device that currently has a Station; otherwise any (powered-off) device.
    const QStringList stations = m_client->paths(str(Iwd::StationIface));
    const QStringList devices = m_client->paths(str(Iwd::DeviceIface));
    m_device = !stations.isEmpty() ? stations.first() : devices.value(0);

    const bool hasStation = !m_device.isEmpty() && m_client->has(m_device, str(Iwd::StationIface));
    m_networks->setStation(hasStation ? m_device : QString());

    if (!hasStation) {
        m_signalRegisteredFor.clear();
        m_agentLevel = -1;
    } else if (m_signalRegisteredFor != m_device) {
        m_signalRegisteredFor = m_device;
        m_agent->registerSignalLevelAgent(m_device);
    }
    Q_EMIT changed();
}

void IwdManager::onSignalLevel(const QString &devicePath, int level)
{
    if (devicePath == m_device && level != m_agentLevel) {
        m_agentLevel = level;
        Q_EMIT changed();
    }
}

void IwdManager::watch(const QDBusPendingCall &call, const std::function<void(const QDBusError &)> &onError)
{
    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [onError](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            onError(w->error());
        }
    });
}

void IwdManager::scan()
{
    if (state() == QLatin1String("off") || state() == QLatin1String("unavailable") || isScanning()) {
        return;
    }
    // Busy/InProgress just mean iwd is already scanning.
    watch(m_client->call(m_device, str(Iwd::StationIface), QStringLiteral("Scan")), [](const QDBusError &) { });
}

void IwdManager::setPowered(bool powered)
{
    if (m_device.isEmpty()) {
        return;
    }
    const auto report = [this](const QDBusError &e) {
        Q_EMIT errorOccurred(humanError(e));
    };
    const QString adapter = deviceProperty(Iwd::DeviceIface, "Adapter").value<QDBusObjectPath>().path();
    if (powered && !m_client->property(adapter, str(Iwd::AdapterIface), QStringLiteral("Powered")).toBool()) {
        // The device can't power up while its adapter is off (e.g. after rfkill).
        auto *watcher = new QDBusPendingCallWatcher(m_client->setProperty(adapter, str(Iwd::AdapterIface), QStringLiteral("Powered"), true), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, report](QDBusPendingCallWatcher *w) {
            w->deleteLater();
            if (w->isError()) {
                report(w->error());
                return;
            }
            watch(m_client->setProperty(m_device, str(Iwd::DeviceIface), QStringLiteral("Powered"), true), report);
        });
        return;
    }
    watch(m_client->setProperty(m_device, str(Iwd::DeviceIface), QStringLiteral("Powered"), powered), report);
}

void IwdManager::connectNetwork(const QString &networkPath, const QString &passphrase)
{
    if (!passphrase.isEmpty()) {
        m_agent->setPendingPassphrase(networkPath, passphrase);
    }
    auto *watcher = new QDBusPendingCallWatcher(m_client->call(networkPath, str(Iwd::NetworkIface), QStringLiteral("Connect")), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, networkPath](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        m_agent->clearPendingPassphrase(networkPath);
        if (w->isError()) {
            const QString message = humanError(w->error());
            if (!message.isEmpty()) {
                Q_EMIT connectFailed(networkPath, message);
            }
        }
    });
}

void IwdManager::disconnectNetwork()
{
    watch(m_client->call(m_device, str(Iwd::StationIface), QStringLiteral("Disconnect")), [this](const QDBusError &e) {
        Q_EMIT errorOccurred(humanError(e));
    });
}

void IwdManager::connectHidden(const QString &ssid, const QString &passphrase)
{
    m_agent->setHiddenPassphrase(passphrase.isEmpty() ? QString() : passphrase);
    m_hiddenInProgress = true;
    auto *watcher = new QDBusPendingCallWatcher(m_client->call(m_device, str(Iwd::StationIface), QStringLiteral("ConnectHiddenNetwork"), {ssid}), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        m_agent->setHiddenPassphrase(QString());
        m_hiddenInProgress = false;
        if (w->isError()) {
            const QString message = humanError(w->error());
            if (!message.isEmpty()) {
                Q_EMIT connectFailed(QString(), message);
            }
        }
    });
}

void IwdManager::forget(const QString &knownNetworkPath)
{
    watch(m_client->call(knownNetworkPath, str(Iwd::KnownNetworkIface), QStringLiteral("Forget")), [this](const QDBusError &e) {
        Q_EMIT errorOccurred(humanError(e));
    });
}

void IwdManager::setAutoConnect(const QString &knownNetworkPath, bool enabled)
{
    watch(m_client->setProperty(knownNetworkPath, str(Iwd::KnownNetworkIface), QStringLiteral("AutoConnect"), enabled), [this](const QDBusError &e) {
        Q_EMIT errorOccurred(humanError(e));
    });
}

void IwdManager::providePassphrase(const QString &networkPath, const QString &passphrase)
{
    if (passphrase.isEmpty()) {
        m_agent->cancel(networkPath);
    } else {
        m_agent->provide(networkPath, passphrase);
    }
}

void IwdManager::cancelPassphrase(const QString &networkPath)
{
    m_agent->cancel(networkPath);
}
