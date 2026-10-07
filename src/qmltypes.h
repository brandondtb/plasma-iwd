#pragma once

#include "iwdclient.h"
#include "iwdmanager.h"

#include <QJSEngine>
#include <QQmlEngine>

// The manager owns a D-Bus agent at a fixed object path, so plasmashell gets exactly
// one per process even if several applet instances or QML engines exist.
struct IwdManagerForeign {
    Q_GADGET
    QML_FOREIGN(IwdManager)
    QML_NAMED_ELEMENT(Iwd)
    QML_SINGLETON

public:
    static IwdManager *create(QQmlEngine *, QJSEngine *)
    {
        static IwdManager *instance = new IwdManager(IwdClient::defaultBus());
        QJSEngine::setObjectOwnership(instance, QJSEngine::CppOwnership);
        return instance;
    }
};
