#include "dbustypes.h"
#include "fakeiwd.h"
#include "iwdmanager.h"
#include "knownnetworkmodel.h"
#include "networkmodel.h"

#include <QDBusMessage>
#include <QSignalSpy>
#include <QTest>

using namespace Qt::StringLiterals;

class TestIwdManager : public QObject
{
    Q_OBJECT

private:
    std::unique_ptr<FakeIwd> m_fake;
    std::unique_ptr<IwdManager> m_manager;

    QStringList networkNames() const
    {
        QStringList names;
        const auto *model = m_manager->networks();
        for (int i = 0; i < model->rowCount(); ++i) {
            names << model->index(i, 0).data(NetworkModel::NameRole).toString();
        }
        return names;
    }

    // Waits until the fake has seen our agent registration.
    void waitReady()
    {
        QTRY_VERIFY(m_manager->isAvailable());
        QTRY_VERIFY(!m_fake->agentPath().isEmpty());
        QTRY_VERIFY(!m_fake->signalAgentPath().isEmpty());
    }

private Q_SLOTS:
    void init()
    {
        m_fake = std::make_unique<FakeIwd>();
        m_fake->addNetwork(u"Home"_s, u"psk"_s, -55, u"correct horse"_s, true);
        m_fake->addNetwork(u"Cafe"_s, u"psk"_s, -70, u"espresso1"_s);
        m_fake->addNetwork(u"Library"_s, u"open"_s, -85);
        m_fake->start();
        m_manager = std::make_unique<IwdManager>(QDBusConnection::sessionBus());
        waitReady();
    }

    void cleanup()
    {
        m_manager.reset();
        m_fake.reset();
    }

    void signalQuality_data()
    {
        QTest::addColumn<int>("dbm");
        QTest::addColumn<int>("quality");
        QTest::newRow("excellent") << -40 << 4;
        QTest::newRow("edge") << -60 << 4;
        QTest::newRow("good") << -65 << 3;
        QTest::newRow("ok") << -70 << 2;
        QTest::newRow("weak") << -80 << 1;
        QTest::newRow("none") << -90 << 0;
    }

    void signalQuality()
    {
        QFETCH(int, dbm);
        QFETCH(int, quality);
        QCOMPARE(Iwd::qualityFromDbm(dbm), quality);
    }

    void initialState()
    {
        QVERIFY(m_manager->hasDevice());
        QVERIFY(m_manager->isPowered());
        QCOMPARE(m_manager->state(), u"disconnected"_s);
        QCOMPARE(m_manager->deviceName(), u"wlan0"_s);
        QTRY_COMPARE(networkNames(), (QStringList{u"Home"_s, u"Cafe"_s, u"Library"_s}));

        const auto *networks = m_manager->networks();
        QCOMPARE(networks->index(0, 0).data(NetworkModel::KnownRole).toBool(), true);
        QCOMPARE(networks->index(0, 0).data(NetworkModel::SignalQualityRole).toInt(), 4);
        QCOMPARE(networks->index(1, 0).data(NetworkModel::KnownRole).toBool(), false);
        QCOMPARE(networks->index(2, 0).data(NetworkModel::TypeRole).toString(), u"open"_s);

        QCOMPARE(m_manager->knownNetworks()->rowCount(), 1);
    }

    void scanReordersIncrementally()
    {
        QTRY_COMPARE(networkNames().size(), 3);
        QSignalSpy resets(m_manager->networks(), &QAbstractItemModel::modelReset);
        QSignalSpy removed(m_manager->networks(), &QAbstractItemModel::rowsRemoved);

        m_fake->setOrder({u"Library"_s, u"Home"_s}, {-50, -60});
        m_manager->scan();
        QTRY_COMPARE(networkNames(), (QStringList{u"Library"_s, u"Home"_s}));
        QCOMPARE(resets.count(), 0);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(m_manager->networks()->index(0, 0).data(NetworkModel::SignalQualityRole).toInt(), 4);
    }

    void connectWithPassphrase()
    {
        m_manager->connectNetwork(m_fake->networkPath(u"Cafe"_s), u"espresso1"_s);
        QTRY_COMPARE(m_manager->state(), u"connected"_s);
        QCOMPARE(m_manager->connectedSsid(), u"Cafe"_s);
        QTRY_COMPARE(m_manager->knownNetworks()->rowCount(), 2);
        // Most recently connected first.
        QCOMPARE(m_manager->knownNetworks()->index(0, 0).data(KnownNetworkModel::NameRole).toString(), u"Cafe"_s);
    }

    void wrongPassphraseFails()
    {
        QSignalSpy failed(m_manager.get(), &IwdManager::connectFailed);
        const QString path = m_fake->networkPath(u"Cafe"_s);
        m_manager->connectNetwork(path, u"wrongpass"_s);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed[0][0].toString(), path);
        QCOMPARE(m_manager->state(), u"disconnected"_s);
    }

    void agentPromptsWhenNoPassphrase()
    {
        QSignalSpy requested(m_manager.get(), &IwdManager::passphraseRequested);
        const QString path = m_fake->networkPath(u"Cafe"_s);
        m_manager->connectNetwork(path);
        QTRY_COMPARE(requested.count(), 1);
        QCOMPARE(requested[0][0].toString(), path);
        QCOMPARE(m_manager->state(), u"connecting"_s);

        m_manager->providePassphrase(path, u"espresso1"_s);
        QTRY_COMPARE(m_manager->state(), u"connected"_s);
    }

    void agentCancelIsSilent()
    {
        QSignalSpy requested(m_manager.get(), &IwdManager::passphraseRequested);
        QSignalSpy failed(m_manager.get(), &IwdManager::connectFailed);
        const QString path = m_fake->networkPath(u"Cafe"_s);
        m_manager->connectNetwork(path);
        QTRY_COMPARE(requested.count(), 1);

        m_manager->cancelPassphrase(path);
        QTRY_COMPARE(m_manager->state(), u"disconnected"_s);
        QTest::qWait(50);
        QCOMPARE(failed.count(), 0);
    }

    void disconnect()
    {
        m_manager->connectNetwork(m_fake->networkPath(u"Home"_s));
        QTRY_COMPARE(m_manager->state(), u"connected"_s);
        m_manager->disconnectNetwork();
        QTRY_COMPARE(m_manager->state(), u"disconnected"_s);
        QCOMPARE(m_manager->connectedPath(), QString());
        QCOMPARE(m_manager->signalQuality(), 0);
    }

    void signalLevelAgent()
    {
        m_manager->connectNetwork(m_fake->networkPath(u"Home"_s));
        QTRY_COMPARE(m_manager->state(), u"connected"_s);
        // Before any agent report, falls back to the scan result (-55 dBm).
        QTRY_COMPARE(m_manager->signalQuality(), 4);
        m_fake->emitSignalLevel(3);
        QTRY_COMPARE(m_manager->signalQuality(), 1);
    }

    void forgetAndAutoConnect()
    {
        const QString known = m_fake->knownPath(u"Home"_s);
        m_manager->setAutoConnect(known, false);
        QTRY_COMPARE(m_manager->knownNetworks()->index(0, 0).data(KnownNetworkModel::AutoConnectRole).toBool(), false);

        QTRY_COMPARE(networkNames().size(), 3);
        m_manager->forget(known);
        QTRY_COMPARE(m_manager->knownNetworks()->rowCount(), 0);
        QTRY_COMPARE(m_manager->networks()->index(0, 0).data(NetworkModel::KnownRole).toBool(), false);
    }

    void powerCycle()
    {
        m_manager->setPowered(false);
        QTRY_COMPARE(m_manager->state(), u"off"_s);
        QVERIFY(!m_manager->isPowered());
        QTRY_COMPARE(m_manager->networks()->rowCount(), 0);

        m_manager->setPowered(true);
        QTRY_COMPARE(m_manager->state(), u"disconnected"_s);
        QTRY_COMPARE(m_manager->networks()->rowCount(), 3);
    }

    void powerOnWithAdapterOff()
    {
        m_manager->setPowered(false);
        QTRY_COMPARE(m_manager->state(), u"off"_s);
        // Simulate an rfkill'd adapter: the device can't power up until the adapter does.
        QDBusConnection::sessionBus().call(
            QDBusMessage::createMethodCall(u"net.connman.iwd"_s, u"/net/connman/iwd/0"_s, u"org.freedesktop.DBus.Properties"_s, u"Set"_s)
                << u"net.connman.iwd.Adapter"_s << u"Powered"_s << QVariant::fromValue(QDBusVariant(false)),
            QDBus::NoBlock);
        QTRY_VERIFY(!m_fake->property(QString::fromLatin1(FakeIwd::Adapter), u"net.connman.iwd.Adapter"_s, u"Powered"_s).toBool());

        m_manager->setPowered(true);
        QTRY_COMPARE(m_manager->state(), u"disconnected"_s);
        QVERIFY(m_manager->isPowered());
    }

    void connectHidden()
    {
        m_manager->connectHidden(u"Secret"_s, u"hiddenpass"_s);
        QTRY_COMPARE(m_manager->state(), u"connected"_s);
        QCOMPARE(m_manager->connectedSsid(), u"Secret"_s);
    }

    void connectHiddenWithoutPassphraseFails()
    {
        QSignalSpy failed(m_manager.get(), &IwdManager::connectFailed);
        QSignalSpy requested(m_manager.get(), &IwdManager::passphraseRequested);
        m_manager->connectHidden(u"Secret"_s);
        QTRY_COMPARE(failed.count(), 1);
        QCOMPARE(failed[0][0].toString(), QString());
        QCOMPARE(requested.count(), 0);
    }

    void iwdRestart()
    {
        QSignalSpy changed(m_manager.get(), &IwdManager::changed);
        m_fake->stop();
        QTRY_VERIFY(!m_manager->isAvailable());
        QCOMPARE(m_manager->state(), u"unavailable"_s);
        QTRY_COMPARE(m_manager->networks()->rowCount(), 0);
        QCOMPARE(m_manager->knownNetworks()->rowCount(), 0);

        m_fake->start();
        waitReady();
        QTRY_COMPARE(m_manager->state(), u"disconnected"_s);
        QTRY_COMPARE(m_manager->networks()->rowCount(), 3);
        QCOMPARE(m_manager->knownNetworks()->rowCount(), 1);
    }
};

QTEST_GUILESS_MAIN(TestIwdManager)
#include "tst_iwdmanager.moc"
