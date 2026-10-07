find_program(DBUS_RUN_SESSION dbus-run-session REQUIRED)
# Nix's dbus-run-session has no system-wide session.conf; point it at its own.
get_filename_component(_dbus_prefix "${DBUS_RUN_SESSION}" DIRECTORY)
get_filename_component(_dbus_prefix "${_dbus_prefix}" DIRECTORY)
set(DBUS_SESSION_CONF "${_dbus_prefix}/share/dbus-1/session.conf")
if(EXISTS "${DBUS_SESSION_CONF}")
    set(DBUS_RUN_SESSION_ARGS "--config-file=${DBUS_SESSION_CONF}")
endif()

add_executable(tst_iwdmanager tst_iwdmanager.cpp fakeiwd.cpp)
target_link_libraries(tst_iwdmanager PRIVATE iwdcore Qt6::Test)
add_test(NAME iwdmanager COMMAND ${DBUS_RUN_SESSION} ${DBUS_RUN_SESSION_ARGS} -- $<TARGET_FILE:tst_iwdmanager>)
