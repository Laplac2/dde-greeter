cmake_minimum_required(VERSION 3.13.4)
project(dde-session-shell C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}  -g -Wall -pthread -Wl,--as-need -fPIE -Wl,-E")
SET(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -O0 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -O3")
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/modules)

# 增加安全编译参数
set(SECURITY_COMPILE, "-fstack-protector-strong -D_FORTITY_SOURCE=1 -z noexecstack -pie -fPIC -z lazy")
set(CMAKE_CXX_FLAGS, "${CMAKE_CXX_FLAGS} ${SECURITY_COMPILE}")
set(CMAKE_C_FLAGS, "${CMAKE_C_FLAGS} ${SECURITY_COMPILE}")

# coverage option
# cmake -DENABLE_COVERAGE=ON ..
OPTION (ENABLE_COVERAGE "Use gcov" OFF)
MESSAGE(STATUS ENABLE_COVERAGE=${ENABLE_COVERAGE})
if (ENABLE_COVERAGE)
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage")
    #SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips64")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -ftree-vectorize -march=loongson3a -mhard-float -mno-micromips -mno-mips16 -flax-vector-conversions -mloongson-ext2 -mloongson-mmi")
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pie")

if (NOT (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Ofast")

    # generate qm
    execute_process(COMMAND bash "translate_generation.sh"
                    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif ()

# Find the library
find_package(PAM REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(DtkWidget REQUIRED)
find_package(DtkCMake  REQUIRED)
find_package(DtkCore REQUIRED)
find_package(Qt5 COMPONENTS
    Core
    Widgets
    Concurrent
    X11Extras
    DBus
    Xml
    Svg
    Network
REQUIRED)

pkg_check_modules(XCB_EWMH REQUIRED xcb-ewmh x11 xi xcursor xfixes xrandr xext xtst)
pkg_check_modules(DFrameworkDBus REQUIRED dframeworkdbus)
pkg_check_modules(QGSettings REQUIRED gsettings-qt)
pkg_check_modules(Greeter REQUIRED liblightdm-qt5-3)

set(Qt_LIBS
    Qt5::Core
    Qt5::Gui
    Qt5::DBus
    Qt5::Widgets
    Qt5::X11Extras
    Qt5::Xml
    Qt5::Svg
)

function(generation_dbus_interface xml class_name class_file option)
    execute_process(COMMAND qdbusxml2cpp ${option} -p ${class_file} -c ${class_name} ${xml}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
endfunction(generation_dbus_interface)

generation_dbus_interface(
    ${CMAKE_SOURCE_DIR}/xml/com.deepin.daemon.Authenticate.xml
    AuthenticateInterface
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface
    -N
)
set(authority_DBUS_SCRS
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface.h
    ${CMAKE_CURRENT_BINARY_DIR}/authenticate_interface.cpp
)

generation_dbus_interface(
    ${CMAKE_SOURCE_DIR}/xml/com.huawei.switchos.xml
    HuaWeiSwitchOSInterface
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface
    -N
)
set(haweiswitchos_DBUS_SCRS
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface.h
    ${CMAKE_CURRENT_BINARY_DIR}/switchos_interface.cpp
)

include_directories(${PROJECT_SOURCE_DIR}/shared/auth)
include_directories(${PROJECT_SOURCE_DIR}/shared/global_util)
include_directories(${PROJECT_SOURCE_DIR}/shared/global_util/dbus)
include_directories(${PROJECT_SOURCE_DIR}/shared/widgets)
include_directories(${PROJECT_SOURCE_DIR}/shared/session-widgets)
include_directories(${PROJECT_SOURCE_DIR}/interface)
include_directories(${PROJECT_SOURCE_DIR}/dde-lock)
include_directories(${PROJECT_SOURCE_DIR}/dde-lock/dbus)
include_directories(${PROJECT_SOURCE_DIR}/dde-login)

aux_source_directory(${PROJECT_SOURCE_DIR}/shared/auth AUTH)
aux_source_directory(${PROJECT_SOURCE_DIR}/shared/global_util GLOBAL_UTILS)
aux_source_directory(${PROJECT_SOURCE_DIR}/shared/global_util/dbus GLOBAL_UTILS_DBUS)
aux_source_directory(${PROJECT_SOURCE_DIR}/shared/widgets WIDGETS)
aux_source_directory(${PROJECT_SOURCE_DIR}/shared/session-widgets SESSION_WIDGETS)
aux_source_directory(${PROJECT_SOURCE_DIR}/interface INTERFACE)
aux_source_directory(${PROJECT_SOURCE_DIR}/dde-lock LOCK)
aux_source_directory(${PROJECT_SOURCE_DIR}/dde-lock/dbus LOCK_DBUS)
aux_source_directory(${PROJECT_SOURCE_DIR}/dde-login LOGIN)

set(QRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/resources.qrc
    ${PROJECT_SOURCE_DIR}/shared/widgets/widgetsimages.qrc
    ${PROJECT_SOURCE_DIR}/shared/widgets/widgetstheme.qrc
)

set(LOCK_SRCS
    ${authority_DBUS_SCRS}
    ${transaction_DBUS_SCRS}
    ${GLOBAL_UTILS}
    ${GLOBAL_UTILS_DBUS}
    ${WIDGETS}
    ${SESSION_WIDGETS}
    ${haweiswitchos_DBUS_SCRS}
    ${INTERFACE}
    ${LOCK}
    ${LOCK_DBUS}
    ${AUTH}
)
add_executable(dde-lock
    ${LOCK_SRCS}
    ${QRCS}
)
target_include_directories(dde-lock PUBLIC
    ${PAM_INCLUDE_DIR}
    ${DTKWIDGET_INCLUDE_DIR}
    ${DTKCORE_INCLUDE_DIR}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${DFrameworkDBus_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${QGSettings_INCLUDE_DIRS}
    ${Qt5X11Extras_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/src/dde-lock
    ${PROJECT_SOURCE_DIR}/src/dde-lock/dbus
)
target_link_libraries(dde-lock PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    ${DFrameworkDBus_LIBRARIES}
    ${DtkWidget_LIBRARIES}
    ${DtkCore_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
    ${Qt5X11Extras_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${Qt5Network_LIBRARIES}
    ${QGSettings_LIBRARIES}
)

set(GREETER_SRCS
    ${authority_DBUS_SCRS}
    ${transaction_DBUS_SCRS}
    ${GLOBAL_UTILS}
    ${GLOBAL_UTILS_DBUS}
    ${WIDGETS}
    ${SESSION_WIDGETS}
    ${haweiswitchos_DBUS_SCRS}
    ${INTERFACE}
    ${LOGIN}
    ${AUTH}
)
add_executable(dde-login
    ${GREETER_SRCS}
    ${QRCS}
)
target_include_directories(dde-login PUBLIC
    ${PAM_INCLUDE_DIR}
    ${DTKWIDGET_INCLUDE_DIR}
    ${DTKCORE_INCLUDE_DIR}
    ${XCB_EWMH_INCLUDE_DIRS}
    ${DFrameworkDBus_INCLUDE_DIRS}
    ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
    ${PROJECT_BINARY_DIR}
    ${QGSettings_INCLUDE_DIRS}
    ${Qt5X11Extras_INCLUDE_DIRS}
    ${Greeter_INCLUDE_DIRS}
)
target_compile_definitions(dde-login
    PRIVATE ENABLE_SESSION
)
target_link_libraries(dde-login PRIVATE
    ${Qt_LIBS}
    ${PAM_LIBRARIES}
    ${XCB_EWMH_LIBRARIES}
    ${DFrameworkDBus_LIBRARIES}
    ${DtkWidget_LIBRARIES}
    ${DtkCore_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Concurrent_LIBRARIES}
    ${Qt5X11Extras_LIBRARIES}
    ${Qt5DBus_LIBRARIES}
    ${Qt5Network_LIBRARIES}
    ${QGSettings_LIBRARIES}
    ${Greeter_LIBRARIES}
)

# debug模式下增加示例demo的编译
if ((${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    add_subdirectory(modules)
endif ()

add_subdirectory(tests)

# Install settings
if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX /usr)
endif ()

# bin
install(TARGETS dde-lock dde-login DESTINATION bin)
install(FILES files/deepin-greeter DESTINATION bin)

file(GLOB SCRIPTS  scripts/*)
install(PROGRAMS ${SCRIPTS} DESTINATION /etc/deepin/greeters.d)

file(GLOB QM_FILES "translations/*.qm")
install(FILES ${QM_FILES} DESTINATION share/dde-session-shell/translations)
install(FILES qt-theme.ini DESTINATION /etc/lightdm/deepin)

install(FILES files/lightdm-deepin-greeter.desktop DESTINATION share/xgreeters/)
install(FILES files/dde-lock.desktop DESTINATION share/applications/)
install(FILES files/dde-session-shell.conf DESTINATION share/dde-session-shell/)
install(FILES files/lightdm-deepin-greeter.conf DESTINATION share/deepin-authentication/privileges/)

# services
file(GLOB SERVICES files/*.service)
install(FILES ${SERVICES} DESTINATION share/dbus-1/services)

#schemas
install(FILES xml/com.deepin.dde.session-shell.gschema.xml DESTINATION share/glib-2.0/schemas)
install(CODE "execute_process(COMMAND glib-compile-schemas ${CMAKE_INSTALL_PREFIX}/share/glib-2.0/schemas)")

# dev
file(GLOB INTERFACE interface/*.h)
install(FILES ${INTERFACE} DESTINATION include/dde-session-shell)

file(GLOB CMAKE_CONFIG cmake/*.cmake)
install(FILES ${CMAKE_CONFIG} DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/DdeSessionShell)

# config
dconfig_meta_files(APPID dde-lock BASE configs/dde-lock FILES configs/dde-lock/default.json)
dconfig_meta_files(APPID dde-login BASE configs/dde-login FILES configs/dde-login/default.json)
