add_library(iwdcore STATIC
    agent.cpp
    iwdclient.cpp
    dbustypes.cpp
    iwdmanager.cpp
    knownnetworkmodel.cpp
    networkmodel.cpp
)
set_target_properties(iwdcore PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(iwdcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(iwdcore PUBLIC Qt6::Core Qt6::DBus Qt6::Qml)
# Lets the QML module register types declared in this library (QML_FOREIGN).
qt_extract_metatypes(iwdcore)

ecm_add_qml_module(iwdplugin URI io.radiation.plasmaiwd VERSION 1.0 GENERATE_PLUGIN_SOURCE DEPENDENCIES QtQuick)
target_sources(iwdplugin PRIVATE qmltypes.h)
target_link_libraries(iwdplugin PRIVATE iwdcore)
ecm_finalize_qml_module(iwdplugin DESTINATION ${KDE_INSTALL_QMLDIR})
