#pragma once

#include <libudev.h>
#include <functional>
#include <atomic>
#include <string>

struct udev_device;

class UdevMonitor {
public:
    using DeviceEventCallback = std::function<void(struct udev_device* dev)>;

    UdevMonitor();
    UdevMonitor(std::atomic<bool> &running_flag);
    ~UdevMonitor();

    bool initialize();

    void run(DeviceEventCallback callback);

    void stop();

private:
    struct udev* udev_context_ = nullptr;
    struct udev_monitor* udev_monitor_ = nullptr;
    int udev_fd_ = -1;
    std::atomic<bool>& running_flag_;
};