#include "UdevMonitor.h"
#include <stdexcept>
#include <syslog.h>
#include <sys/select.h>
#include <cerrno> 
#include <cstring>

UdevMonitor::UdevMonitor(std::atomic<bool>& running_flag)
 : running_flag_(running_flag) {}


UdevMonitor::~UdevMonitor() {
    if (udev_monitor_) {
        udev_monitor_unref(udev_monitor_);
        syslog(LOG_DEBUG,"[UdevMonitor] Монитор udev освобожден.");
    }
    if (udev_context_) {
        udev_unref(udev_context_);
        syslog(LOG_DEBUG,"[UdevMonitor] Контекст udev освобожден.");
    }
}

bool UdevMonitor::initialize() {
    udev_context_ = udev_new();
    if (!udev_context_) {
        syslog(LOG_CRIT, "[UdevMonitor] Не удалось создать объект udev.");
        return false;
    }
    syslog(LOG_INFO, "[UdevMonitor] Контекст udev инициализирован.");

    udev_monitor_ = udev_monitor_new_from_netlink(udev_context_, "udev");
    if (!udev_monitor_) {
        syslog(LOG_CRIT, "[UdevMonitor] Не удалось создать udev monitor.");
        return false;
    }

    if (udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_, "usb", "usb_device") < 0) {
         syslog(LOG_ERR, "[UdevMonitor] Не удалось добавить фильтр udev 'usb/usb_device'.");
    }
    if (udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_, "block", NULL) < 0) {
         syslog(LOG_ERR, "[UdevMonitor] Не удалось добавить фильтр udev 'block'.");
    }

    if (udev_monitor_enable_receiving(udev_monitor_) < 0) {
         syslog(LOG_CRIT, "[UdevMonitor] Не удалось включить получение событий udev monitor.");
         return false;
    }

    udev_fd_ = udev_monitor_get_fd(udev_monitor_);
    if (udev_fd_ < 0) {
         syslog(LOG_CRIT, "[UdevMonitor] Не удалось получить файловый дескриптор udev monitor.");
         return false;
    }

    syslog(LOG_INFO, "[UdevMonitor] Монитор udev настроен и запущен (fd=%d). Слушаем события usb и block.", udev_fd_);
    return true;
}

void UdevMonitor::run(DeviceEventCallback callback) {
    if (udev_fd_ < 0 || !callback) {
         syslog(LOG_ERR, "[UdevMonitor] Монитор не инициализирован или callback не задан. Выход из run().");
         running_flag_.store(false); 
         return;
    }

    running_flag_.store(true); 
    syslog(LOG_DEBUG, "[UdevMonitor] Вход в главный цикл ожидания событий udev.");

    while (running_flag_.load()) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(udev_fd_, &fds);
        struct timeval tv;
        tv.tv_sec = 1; 
        tv.tv_usec = 0;

        int ret = select(udev_fd_ + 1, &fds, NULL, NULL, &tv);

        if (!running_flag_.load()) {
            break;
        }

        if (ret < 0) {
            if (errno == EINTR) {
                syslog(LOG_DEBUG, "[UdevMonitor] select() прерван сигналом (EINTR).");
                continue;
            } else {
                syslog(LOG_ERR, "[UdevMonitor] Ошибка select(): %s", strerror(errno));
                running_flag_.store(false); 
                break;
            }
        } else if (ret > 0 && FD_ISSET(udev_fd_, &fds)) {
            while (running_flag_.load()) {
                 struct udev_device* dev = udev_monitor_receive_device(udev_monitor_);
                 if (!dev) {
                     break; 
                 }
                 syslog(LOG_DEBUG, "[UdevMonitor] Получено событие от udev (devpath=%s)", udev_device_get_devpath(dev));
                 try {
                     callback(dev); 
                 } catch (const std::exception& e) {
                    syslog(LOG_ERR, "[UdevMonitor] Исключение в callback: %s", e.what());
                 } catch (...) {
                    syslog(LOG_ERR, "[UdevMonitor] Неизвестное исключение в callback");
                 }
                 udev_device_unref(dev);
            }
        }
    }
     syslog(LOG_DEBUG, "[UdevMonitor] Выход из главного цикла.");
}

void UdevMonitor::stop() {
    syslog(LOG_INFO, "[UdevMonitor] Получен запрос на остановку.");
    running_flag_.store(false);
}