# CMakeLists.txt для usb_monitor

cmake_minimum_required(VERSION 3.5)
project(usb_monitor_daemon LANGUAGES CXX)

# --- Настройки компилятора ---
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Поиск зависимостей ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(GLIB REQUIRED glib-2.0)
# GObject и Libnotify больше не используются напрямую, но могут быть косвенными
# зависимостями или полезны для zenity - оставим их поиск
pkg_check_modules(GOBJECT REQUIRED gobject-2.0)
pkg_check_modules(LIBNOTIFY REQUIRED libnotify>=0.7)
find_package(Threads REQUIRED)

# --- Определение исполняемого файла ---
add_executable(usb_monitor
    main.cpp
    Application.cpp
    UdevMonitor.cpp
    ResultDisplay.cpp
    DeviceTester.cpp
    DaemonUtil.cpp
)

# --- Подключение зависимостей к цели ---
target_include_directories(usb_monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR} # Чтобы находить .h файлы
    ${UDEV_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GOBJECT_INCLUDE_DIRS}
    ${LIBNOTIFY_INCLUDE_DIRS}
)

target_link_libraries(usb_monitor PRIVATE
    ${UDEV_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GOBJECT_LIBRARIES}
    ${LIBNOTIFY_LIBRARIES}
    Threads::Threads
)

# --- Опционально: Правила установки ---
include(GNUInstallDirs)
install(TARGETS usb_monitor
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# --- Вывод информации при конфигурации ---
message(STATUS "Конфигурация сборки usb_monitor_daemon:")
message(STATUS " - Источники: main.cpp, Application.cpp, UdevMonitor.cpp, ResultDisplay.cpp, DeviceTester.cpp, DaemonUtil.cpp")
message(STATUS " - Зависимости: libudev, glib-2.0, gobject-2.0, libnotify, Threads")
message(STATUS " - Используется C++ стандарт: ${CMAKE_CXX_STANDARD}")