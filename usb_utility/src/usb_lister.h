#ifndef USB_LISTER_H
#define USB_LISTER_H

#include "utils.h"
#include <vector>
#include <libusb-1.0/libusb.h> // Include libusb header

class UsbLister {
public:
    UsbLister();
    ~UsbLister();

    std::vector<UsbDeviceInfo> list_connected_devices();

    static std::optional<std::string> get_string_descriptor(libusb_device_handle *handle, uint8_t descriptor_index);

    UsbLister(const UsbLister&) = delete;
    UsbLister& operator=(const UsbLister&) = delete;

private:
    libusb_context* usb_context = nullptr; // libusb session context

    void print_device_info(const UsbDeviceInfo& dev_info); // Helper for printing
};

#endif // USB_LISTER_H