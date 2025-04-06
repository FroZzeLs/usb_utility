#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <optional> // Requires C++17

struct UsbDeviceInfo {
    std::string syspath;        // System path (e.g., /sys/devices/...)
    std::string devnode;        // Device node (e.g., /dev/bus/usb/001/005)
    std::string vendor_id;
    std::string product_id;
    std::optional<std::string> manufacturer;
    std::optional<std::string> product;
    std::optional<std::string> serial_number;

    UsbDeviceInfo() = default;
    UsbDeviceInfo(std::string sp, std::string dn, std::string vid, std::string pid)
        : syspath(std::move(sp)), devnode(std::move(dn)),
          vendor_id(std::move(vid)), product_id(std::move(pid)) {}
};

#endif // UTILS_H