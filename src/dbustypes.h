#pragma once

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QList>
#include <QMap>
#include <QString>
#include <QVariantMap>

namespace Iwd
{
inline constexpr auto Service = "net.connman.iwd";
inline constexpr auto ManagerPath = "/net/connman/iwd";
inline constexpr auto AdapterIface = "net.connman.iwd.Adapter";
inline constexpr auto DeviceIface = "net.connman.iwd.Device";
inline constexpr auto StationIface = "net.connman.iwd.Station";
inline constexpr auto NetworkIface = "net.connman.iwd.Network";
inline constexpr auto KnownNetworkIface = "net.connman.iwd.KnownNetwork";
inline constexpr auto AgentManagerIface = "net.connman.iwd.AgentManager";
inline constexpr auto AgentIface = "net.connman.iwd.Agent";
inline constexpr auto SignalLevelAgentIface = "net.connman.iwd.SignalLevelAgent";
inline constexpr auto PropertiesIface = "org.freedesktop.DBus.Properties";
inline constexpr auto ObjectManagerIface = "org.freedesktop.DBus.ObjectManager";

// Thresholds in dBm, strongest first; shared by the SignalLevelAgent and scan results.
inline const QList<qint16> SignalThresholds{-60, -67, -74, -81};

// 4 = excellent ... 0 = very weak.
int qualityFromDbm(int dbm);
int qualityFromLevel(int level);
}

using InterfaceMap = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceMap>;

struct OrderedNetwork {
    QDBusObjectPath path;
    qint16 signal = 0; // 100 * dBm
};
using OrderedNetworkList = QList<OrderedNetwork>;

QDBusArgument &operator<<(QDBusArgument &arg, const OrderedNetwork &n);
const QDBusArgument &operator>>(const QDBusArgument &arg, OrderedNetwork &n);

void registerIwdDBusTypes();

Q_DECLARE_METATYPE(OrderedNetwork)
