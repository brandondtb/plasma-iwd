#include "dbustypes.h"

#include <QDBusMetaType>

int Iwd::qualityFromDbm(int dbm)
{
    for (int i = 0; i < SignalThresholds.size(); ++i) {
        if (dbm >= SignalThresholds[i]) {
            return qualityFromLevel(i);
        }
    }
    return 0;
}

int Iwd::qualityFromLevel(int level)
{
    return qBound(0, int(SignalThresholds.size()) - level, int(SignalThresholds.size()));
}

QDBusArgument &operator<<(QDBusArgument &arg, const OrderedNetwork &n)
{
    arg.beginStructure();
    arg << n.path << n.signal;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, OrderedNetwork &n)
{
    arg.beginStructure();
    arg >> n.path >> n.signal;
    arg.endStructure();
    return arg;
}

void registerIwdDBusTypes()
{
    static const bool once = [] {
        qDBusRegisterMetaType<InterfaceMap>();
        qDBusRegisterMetaType<ManagedObjects>();
        qDBusRegisterMetaType<OrderedNetwork>();
        qDBusRegisterMetaType<OrderedNetworkList>();
        qDBusRegisterMetaType<QList<qint16>>();
        return true;
    }();
    Q_UNUSED(once);
}
