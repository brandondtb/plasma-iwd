#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QHash>
#include <QObject>

class IwdClient;

// Answers iwd's passphrase requests and receives signal-level updates.
// Passphrases are either supplied up front (connect with a typed password) or
// requested interactively via passphraseRequested() and answered with provide()/cancel().
class Agent : public QObject
{
    Q_OBJECT

public:
    static constexpr auto ObjectPath = "/io/radiation/plasmaiwd/agent";

    explicit Agent(IwdClient *client, QObject *parent = nullptr);
    ~Agent() override;

    void setPendingPassphrase(const QString &networkPath, const QString &passphrase);
    void clearPendingPassphrase(const QString &networkPath);
    // Used for ConnectHiddenNetwork, where the network path isn't known in advance.
    void setHiddenPassphrase(const QString &passphrase);

    bool isWaiting(const QString &networkPath) const;
    void provide(const QString &networkPath, const QString &passphrase);
    void cancel(const QString &networkPath);
    void reject(const QDBusMessage &msg, const QString &reason);

    // Must be called while iwd is available; registers this agent with AgentManager.
    void registerWithIwd();
    void registerSignalLevelAgent(const QString &stationPath);

    // D-Bus entry points (net.connman.iwd.Agent / SignalLevelAgent).
    QString requestPassphrase(const QDBusObjectPath &network, const QDBusMessage &msg);
    void cancelRequest(const QString &reason);
    void release();
    void signalLevelChanged(const QDBusObjectPath &device, uchar level);

Q_SIGNALS:
    void passphraseRequested(const QString &networkPath);
    void requestCanceled(const QString &networkPath);
    void signalLevel(const QString &devicePath, int level);

private:
    IwdClient *m_client;
    bool m_exported = false;
    QHash<QString, QString> m_pending;
    QString m_hiddenPassphrase;
    QHash<QString, QDBusMessage> m_waiting;
};

class AgentAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.iwd.Agent")

public:
    explicit AgentAdaptor(Agent *agent);

public Q_SLOTS:
    void Release();
    QString RequestPassphrase(const QDBusObjectPath &network, const QDBusMessage &msg);
    QString RequestPrivateKeyPassphrase(const QDBusObjectPath &network, const QDBusMessage &msg);
    QString RequestUserNameAndPassword(const QDBusObjectPath &network, const QDBusMessage &msg);
    QString RequestUserPassword(const QDBusObjectPath &network, const QString &user, const QDBusMessage &msg);
    void Cancel(const QString &reason);

private:
    QString refuse(const QDBusMessage &msg);
    Agent *m_agent;
};

class SignalLevelAgentAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "net.connman.iwd.SignalLevelAgent")

public:
    explicit SignalLevelAgentAdaptor(Agent *agent);

public Q_SLOTS:
    void Release(const QDBusObjectPath &device);
    void Changed(const QDBusObjectPath &device, uchar level);

private:
    Agent *m_agent;
};
