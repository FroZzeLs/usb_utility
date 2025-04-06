#include "usb_lister.h"
#include <iostream>
#include <iomanip> // For std::setw, std::hex
#include <sstream> // For formatting IDs

std::string to_hex_string(uint16_t value) {
    std::stringstream ss;
    ss << std::hex << std::setw(4) << std::setfill('0') << value;
    return ss.str();
}

UsbLister::UsbLister() {
    int ret = libusb_init(&usb_context);
    if (ret < 0) {
        std::cerr << "Error initializing libusb: " << libusb_error_name(ret) << std::endl;
        usb_context = nullptr; 
    }
}

UsbLister::~UsbLister() {
    if (usb_context) {
        libusb_exit(usb_context);
    }
}

std::optional<std::string> UsbLister::get_string_descriptor(libusb_device_handle *handle, uint8_t descriptor_index) {
    if (!handle || descriptor_index == 0) {
        return std::nullopt;
    }

    unsigned char buffer[256]; // Max descriptor length
    int ret = libusb_get_string_descriptor_ascii(handle, descriptor_index, buffer, sizeof(buffer));

    if (ret < 0) {
        return std::nullopt;
    }
    return std::string(reinterpret_cast<char*>(buffer), ret);
}

std::vector<UsbDeviceInfo> UsbLister::list_connected_devices() {
    std::vector<UsbDeviceInfo> devices;
    if (!usb_context) {
        std::cerr << "Error: libusb context not initialized." << std::endl;
        return devices;
    }

    libusb_device **list;
    ssize_t count = libusb_get_device_list(usb_context, &list);
    if (count < 0) {
        std::cerr << "Error getting USB device list: " << libusb_error_name(count) << std::endl;
        return devices;
    }

    std::cout << "--- Currently Connected USB Devices ---" << std::endl;

    for (ssize_t i = 0; i < count; ++i) {
        libusb_device *device = list[i];
        libusb_device_descriptor desc;
        int ret = libusb_get_device_descriptor(device, &desc);
        if (ret < 0) {
            std::cerr << "Warning: Failed to get descriptor for a device: " << libusb_error_name(ret) << std::endl;
            continue;
        }

        UsbDeviceInfo dev_info;
        dev_info.vendor_id = to_hex_string(desc.idVendor);
        dev_info.product_id = to_hex_string(desc.idProduct);
        dev_info.syspath = "N/A (use udev for path)";
        dev_info.devnode = "/dev/bus/usb/" + std::to_string(libusb_get_bus_number(device))
                         + "/" + std::to_string(libusb_get_device_address(device));


        libusb_device_handle *handle = nullptr;
        ret = libusb_open(device, &handle);

        if (ret == 0) {
             dev_info.manufacturer = get_string_descriptor(handle, desc.iManufacturer);
             dev_info.product = get_string_descriptor(handle, desc.iProduct);
             dev_info.serial_number = get_string_descriptor(handle, desc.iSerialNumber);
             libusb_close(handle); // Close the handle immediately after getting info
        } else if (ret != LIBUSB_ERROR_ACCESS) {
             
        } else {
             std::cout << "Info: Insufficient permissions to open device " << dev_info.vendor_id << ":" << dev_info.product_id << std::endl;
        }


        devices.push_back(dev_info);
        print_device_info(dev_info); // Print to console
    }

    std::cout << "---------------------------------------" << std::endl;


    libusb_free_device_list(list, 1); // Free the list and unref the devices
    return devices;
}

// Helper to print device info neatly
void UsbLister::print_device_info(const UsbDeviceInfo& dev_info) {
    std::cout << "  VID: " << dev_info.vendor_id
              << ", PID: " << dev_info.product_id;
    if (dev_info.manufacturer) std::cout << ", Manuf: " << *dev_info.manufacturer;
    if (dev_info.product) std::cout << ", Prod: " << *dev_info.product;
    if (dev_info.serial_number) std::cout << ", Serial: " << *dev_info.serial_number;
    std::cout << " (Node: " << dev_info.devnode << ")" << std::endl;
 
}