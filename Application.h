#pragma once

#include "UdevMonitor.h"
#include "DeviceInfo.h"
#include <map>
#include <string>
#include <atomic>
#include <csignal> 

struct udev_device;

class Application {
public:
    Application(bool as_daemon);
    ~Application();

    int run(); 

private:
    bool initialize();
    void cleanup();

    void setupSignalHandlers();
    static void staticSignalHandler(int signum); 
    void handleSignal(int signum);        

    void onDeviceEvent(struct udev_device* dev);

    // Проверка интерфейса Mass Storage
    bool hasMassStorageInterface(struct udev_device* usb_dev);

    bool is_daemon_;
    UdevMonitor udev_monitor_;
    std::map<std::string, DeviceInfo> active_devices_map_;

    // Статические члены для обработки сигналов
    static std::atomic<Application*> instance_; // Указатель на текущий экземпляр
    static std::atomic<bool> running_;      // Глобальный флаг работы
};