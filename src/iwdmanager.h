#pragma once

#include <QAbstractItemModel>
#include <QDBusConnection>
#include <QObject>

#include <functional>

class Agent;
class IwdClient;
class KnownNetworkModel;
class NetworkModel;

class IwdManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ isAvailable NOTIFY changed)
    Q_PROPERTY(bool hasDevice READ hasDevice NOTIFY changed)
    Q_PROPERTY(bool powered READ isPowered NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool scanning READ isScanning NOTIFY changed)
    Q_PROPERTY(QString connectedSsid READ connectedSsid NOTIFY changed)
    Q_PROPERTY(QString connectedPath READ connectedPath NOTIFY changed)
    Q_PROPERTY(int signalQuality READ signalQuality NOTIFY changed)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY changed)
    Q_PROPERTY(QAbstractItemModel *networks READ networks CONSTANT)
    Q_PROPERTY(QAbstractItemModel *knownNetworks READ knownNetworks CONSTANT)

public:
    explicit IwdManager(const QDBusConnection &bus, QObject *parent = nullptr);
    ~IwdManager() override;

    bool isAvailable() const;
    bool hasDevice() const;
    bool isPowered() const;
    // "off", "disconnected", "connecting", "connected", "roaming", "disconnecting", or "unavailable".
    QString state() const;
    bool isScanning() const;
    QString connectedSsid() const;
    QString connectedPath() const;
    // 0 (weak) .. 4 (excellent); 0 when not connected.
    int signalQuality() const;
    QString deviceName() const;
    QAbstractItemModel *networks() const;
    QAbstractItemModel *knownNetworks() const;

    Q_INVOKABLE void scan();
    Q_INVOKABLE void setPowered(bool powered);
    Q_INVOKABLE void connectNetwork(const QString &networkPath, const QString &passphrase = {});
    Q_INVOKABLE void disconnectNetwork();
    Q_INVOKABLE void connectHidden(const QString &ssid, const QString &passphrase = {});
    Q_INVOKABLE void forget(const QString &knownNetworkPath);
    Q_INVOKABLE void setAutoConnect(const QString &knownNetworkPath, bool enabled);
    // Answer an outstanding passphraseRequested(); an empty passphrase cancels.
    Q_INVOKABLE void providePassphrase(const QString &networkPath, const QString &passphrase);
    Q_INVOKABLE void cancelPassphrase(const QString &networkPath);

Q_SIGNALS:
    void changed();
    void passphraseRequested(const QString &networkPath);
    void passphraseRequestCanceled(const QString &networkPath);
    void connectFailed(const QString &networkPath, const QString &message);
    void errorOccurred(const QString &message);

private:
    void updateDevice();
    void onSignalLevel(const QString &devicePath, int level);
    QVariant deviceProperty(const char *iface, const char *key) const;
    void watch(const QDBusPendingCall &call, const std::function<void(const QDBusError &)> &onError);

    IwdClient *m_client;
    Agent *m_agent;
    NetworkModel *m_networks;
    KnownNetworkModel *m_knownNetworks;
    QString m_device;
    QString m_signalRegisteredFor;
    int m_agentLevel = -1;
    bool m_hiddenInProgress = false;
};
