#pragma once

#include <QAbstractListModel>

class IwdClient;

// Networks visible to the active station, in iwd's ranking order.
class NetworkModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        NameRole,
        TypeRole,
        SignalQualityRole,
        ConnectedRole,
        KnownRole,
        KnownPathRole,
    };
    Q_ENUM(Role)

    explicit NetworkModel(IwdClient *client, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setStation(const QString &stationPath);
    void refresh();

private:
    struct Entry {
        QString path;
        int dbm;
    };

    void apply(const QList<Entry> &entries);
    void onPropertiesChanged(const QString &path, const QString &iface);

    IwdClient *m_client;
    QString m_station;
    QList<Entry> m_entries;
    quint64 m_generation = 0;
};
