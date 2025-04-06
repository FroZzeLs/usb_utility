#ifndef USB_DETECTOR_H
#define USB_DETECTOR_H

#include "utils.h"
#include <libudev.h> // Include udev header
#include <functional> // For std::function

class UsbDetector {
public:
    // Callback function type: void(const UsbDeviceInfo&)
    using DeviceCallback = std::function<void(const UsbDeviceInfo&)>;

    UsbDetector();
    ~UsbDetector();

    bool monitor_for_devices(DeviceCallback on_device_added);

    // Helper to get udev device details
    static std::optional<UsbDeviceInfo> get_device_details(struct udev_device* dev);


    // Disable copy and assignment
    UsbDetector(const UsbDetector&) = delete;
    UsbDetector& operator=(const UsbDetector&) = delete;

private:
    struct udev* udev_context = nullptr;
    struct udev_monitor* udev_monitor = nullptr;

    bool setup_monitor();
};

#endif // USB_DETECTOR_H