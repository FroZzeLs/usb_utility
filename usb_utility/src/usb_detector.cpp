#include "usb_detector.h"
#include <iostream>
#include <poll.h> // For poll()
#include <unistd.h> // For read() - though not strictly needed with udev_monitor_receive_device
#include <optional>
#include <cstring>

UsbDetector::UsbDetector() {
    udev_context = udev_new();
    if (!udev_context) {
        std::cerr << "Error: Cannot create udev context." << std::endl;
        // Optionally throw
    }
}

UsbDetector::~UsbDetector() {
    if (udev_monitor) {
        udev_monitor_unref(udev_monitor);
    }
    if (udev_context) {
        udev_unref(udev_context);
    }
}

bool UsbDetector::setup_monitor() {
    if (!udev_context) return false;

    udev_monitor = udev_monitor_new_from_netlink(udev_context, "udev");
    if (!udev_monitor) {
        std::cerr << "Error: Cannot create udev monitor." << std::endl;
        return false;
    }

    // Filter for USB subsystem and specifically usb_device type
    // (Avoids catching interfaces, hubs as the primary event)
    if (udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "usb", "usb_device") < 0) {
        std::cerr << "Error: Cannot add udev monitor filter for usb/usb_device." << std::endl;
        return false;
    }

    // Start receiving events
    if (udev_monitor_enable_receiving(udev_monitor) < 0) {
        std::cerr << "Error: Cannot enable udev monitor receiving." << std::endl;
        return false;
    }

    std::cout << "USB Detector: Monitoring for new device connections..." << std::endl;
    return true;
}

std::optional<UsbDeviceInfo> UsbDetector::get_device_details(struct udev_device* dev) {
     if (!dev) return std::nullopt;

     const char* action = udev_device_get_action(dev);
     // We are primarily interested in 'add' events from the monitor setup
     if (!action || strcmp(action, "add") != 0) {
         return std::nullopt; // Not an 'add' event we filtered for
     }

     const char* syspath = udev_device_get_syspath(dev);
     const char* devnode = udev_device_get_devnode(dev); // May be null if node not created yet/relevant
     const char* vid = udev_device_get_sysattr_value(dev, "idVendor");
     const char* pid = udev_device_get_sysattr_value(dev, "idProduct");

     if (!syspath || !vid || !pid) {
         // Essential info missing
         std::cerr << "Warning: Received udev event with missing essential info (syspath/vid/pid)." << std::endl;
         return std::nullopt;
     }

     UsbDeviceInfo info(syspath, (devnode ? devnode : "N/A"), vid, pid);

     // Get optional string attributes
     const char* manufacturer = udev_device_get_sysattr_value(dev, "manufacturer");
     const char* product = udev_device_get_sysattr_value(dev, "product");
     const char* serial = udev_device_get_sysattr_value(dev, "serial");

     if (manufacturer) info.manufacturer = manufacturer;
     if (product) info.product = product;
     if (serial) info.serial_number = serial;

     return info;
}


bool UsbDetector::monitor_for_devices(DeviceCallback on_device_added) {
    if (!udev_monitor && !setup_monitor()) {
        return false; // Setup failed
    }
    if (!udev_monitor) return false; // Should not happen if setup succeeded

    int udev_fd = udev_monitor_get_fd(udev_monitor);
    if (udev_fd < 0) {
        std::cerr << "Error: Cannot get udev monitor file descriptor." << std::endl;
        return false;
    }

    // Use poll to wait for events on the udev monitor fd
    struct pollfd fds[1];
    fds[0].fd = udev_fd;
    fds[0].events = POLLIN; // Wait for data to read

    // Blocking wait for an event
    int poll_ret = poll(fds, 1, -1); // -1 timeout = wait indefinitely

    if (poll_ret < 0) {
        perror("poll error"); // Print system error (e.g., interrupted by signal)
        return false; // Indicate an error occurred
    }

    if (poll_ret > 0 && (fds[0].revents & POLLIN)) {
        // Event received
        struct udev_device* dev = udev_monitor_receive_device(udev_monitor);
        if (dev) {
            std::optional<UsbDeviceInfo> dev_info = get_device_details(dev);

            if (dev_info) {
                // Call the callback function with the details
                on_device_added(*dev_info);
            }

            udev_device_unref(dev); // Release the device object
        } else {
            std::cerr << "Warning: udev_monitor_receive_device returned null." << std::endl;
        }
    }
    
    return true; // Indicate monitoring loop should continue
}