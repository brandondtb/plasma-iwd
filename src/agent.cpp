#include "agent.h"

#include "dbustypes.h"
#include "iwdclient.h"

#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace
{
const auto CanceledError = QStringLiteral("net.connman.iwd.Agent.Error.Canceled");
}

Agent::Agent(IwdClient *client, QObject *parent)
    : QObject(parent)
    , m_client(client)
{
    new AgentAdaptor(this);
    new SignalLevelAgentAdaptor(this);
}

Agent::~Agent()
{
    for (const QDBusMessage &msg : std::as_const(m_waiting)) {
        m_client->bus().send(msg.createErrorReply(CanceledError, QStringLiteral("Agent shutting down")));
    }
    if (m_exported) {
        m_client->bus().unregisterObject(QString::fromLatin1(ObjectPath));
    }
}

void Agent::setPendingPassphrase(const QString &networkPath, const QString &passphrase)
{
    m_pending.insert(networkPath, passphrase);
}

void Agent::clearPendingPassphrase(const QString &networkPath)
{
    m_pending.remove(networkPath);
}

void Agent::setHiddenPassphrase(const QString &passphrase)
{
    m_hiddenPassphrase = passphrase;
}

bool Agent::isWaiting(const QString &networkPath) const
{
    return m_waiting.contains(networkPath);
}

void Agent::provide(const QString &networkPath, const QString &passphrase)
{
    const auto msg = m_waiting.take(networkPath);
    if (msg.type() == QDBusMessage::MethodCallMessage) {
        m_client->bus().send(msg.createReply(passphrase));
    }
}

void Agent::cancel(const QString &networkPath)
{
    const auto msg = m_waiting.take(networkPath);
    if (msg.type() == QDBusMessage::MethodCallMessage) {
        m_client->bus().send(msg.createErrorReply(CanceledError, QStringLiteral("Canceled by user")));
    }
}

void Agent::reject(const QDBusMessage &msg, const QString &reason)
{
    msg.setDelayedReply(true);
    m_client->bus().send(msg.createErrorReply(CanceledError, reason));
}

void Agent::registerWithIwd()
{
    if (!m_exported) {
        m_exported = m_client->bus().registerObject(QString::fromLatin1(ObjectPath), this, QDBusConnection::ExportAdaptors);
        if (!m_exported) {
            qWarning("iwd: failed to export agent object");
            return;
        }
    }
    const auto pending = m_client->call(QString::fromLatin1(Iwd::ManagerPath),
                                        QString::fromLatin1(Iwd::AgentManagerIface),
                                        QStringLiteral("RegisterAgent"),
                                        {QVariant::fromValue(QDBusObjectPath(QString::fromLatin1(ObjectPath)))});
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        if (w->isError()) {
            qWarning("iwd: RegisterAgent failed: %s", qPrintable(w->error().message()));
        }
    });
}

void Agent::registerSignalLevelAgent(const QString &stationPath)
{
    const auto pending = m_client->call(stationPath,
                                        QString::fromLatin1(Iwd::StationIface),
                                        QStringLiteral("RegisterSignalLevelAgent"),
                                        {QVariant::fromValue(QDBusObjectPath(QString::fromLatin1(ObjectPath))), QVariant::fromValue(Iwd::SignalThresholds)});
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [](QDBusPendingCallWatcher *w) {
        w->deleteLater();
        // AlreadyExists is expected when re-registering on the same station.
        if (w->isError() && w->error().name() != QLatin1String("net.connman.iwd.AlreadyExists")) {
            qWarning("iwd: RegisterSignalLevelAgent failed: %s", qPrintable(w->error().message()));
        }
    });
}

QString Agent::requestPassphrase(const QDBusObjectPath &network, const QDBusMessage &msg)
{
    const QString path = network.path();
    if (const auto it = m_pending.constFind(path); it != m_pending.cend()) {
        return *it;
    }
    if (!m_hiddenPassphrase.isNull()) {
        return std::exchange(m_hiddenPassphrase, QString());
    }

    msg.setDelayedReply(true);
    // A newer request for the same network supersedes the old one.
    cancel(path);
    m_waiting.insert(path, msg);
    Q_EMIT passphraseRequested(path);
    return {};
}

void Agent::cancelRequest(const QString &reason)
{
    Q_UNUSED(reason);
    // iwd only ever has one request outstanding per agent.
    const QStringList paths = m_waiting.keys();
    m_waiting.clear();
    for (const QString &path : paths) {
        Q_EMIT requestCanceled(path);
    }
}

void Agent::release()
{
    m_waiting.clear();
}

void Agent::signalLevelChanged(const QDBusObjectPath &device, uchar level)
{
    Q_EMIT signalLevel(device.path(), level);
}

AgentAdaptor::AgentAdaptor(Agent *agent)
    : QDBusAbstractAdaptor(agent)
    , m_agent(agent)
{
}

void AgentAdaptor::Release()
{
    m_agent->release();
}

QString AgentAdaptor::RequestPassphrase(const QDBusObjectPath &network, const QDBusMessage &msg)
{
    return m_agent->requestPassphrase(network, msg);
}

QString AgentAdaptor::RequestPrivateKeyPassphrase(const QDBusObjectPath &, const QDBusMessage &msg)
{
    return refuse(msg);
}

QString AgentAdaptor::RequestUserNameAndPassword(const QDBusObjectPath &, const QDBusMessage &msg)
{
    return refuse(msg);
}

QString AgentAdaptor::RequestUserPassword(const QDBusObjectPath &, const QString &, const QDBusMessage &msg)
{
    return refuse(msg);
}

void AgentAdaptor::Cancel(const QString &reason)
{
    m_agent->cancelRequest(reason);
}

QString AgentAdaptor::refuse(const QDBusMessage &msg)
{
    // 802.1X credentials are out of scope; provision those with iwd config files.
    m_agent->reject(msg, QStringLiteral("802.1X networks are not supported"));
    return {};
}

SignalLevelAgentAdaptor::SignalLevelAgentAdaptor(Agent *agent)
    : QDBusAbstractAdaptor(agent)
    , m_agent(agent)
{
}

void SignalLevelAgentAdaptor::Release(const QDBusObjectPath &)
{
}

void SignalLevelAgentAdaptor::Changed(const QDBusObjectPath &device, uchar level)
{
    m_agent->signalLevelChanged(device, level);
}
