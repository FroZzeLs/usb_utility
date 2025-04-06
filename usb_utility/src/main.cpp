#include <iostream>
#include <vector>
#include <csignal> // For signal handling (Ctrl+C)

#include "logger.h"
#include "usb_lister.h"
#include "usb_detector.h"
#include "utils.h"

// Global flag to signal termination
volatile sig_atomic_t keep_running = 1;

// Signal handler function
void sigint_handler(int signal) {
    std::cout << "\nCtrl+C detected. Shutting down..." << std::endl;
    keep_running = 0;
}

// Callback function for the detector
void handle_new_device(const UsbDeviceInfo& dev_info, Logger& logger) {
    std::string log_message = "Device Added: VID=" + dev_info.vendor_id +
                              ", PID=" + dev_info.product_id;
    if (dev_info.manufacturer) log_message += ", Manuf=" + *dev_info.manufacturer;
    if (dev_info.product) log_message += ", Prod=" + *dev_info.product;
    if (dev_info.serial_number) log_message += ", Serial=" + *dev_info.serial_number;
    log_message += " (SysPath: " + dev_info.syspath + ")";
    if (dev_info.devnode != "N/A") log_message += " (DevNode: " + dev_info.devnode + ")";


    std::cout << "--- New USB Device Detected ---" << std::endl;
    std::cout << "  VID: " << dev_info.vendor_id
              << ", PID: " << dev_info.product_id << std::endl;
    if (dev_info.manufacturer) std::cout << "  Manufacturer: " << *dev_info.manufacturer << std::endl;
    if (dev_info.product) std::cout << "  Product: " << *dev_info.product << std::endl;
    if (dev_info.serial_number) std::cout << "  Serial No: " << *dev_info.serial_number << std::endl;
    std::cout << "  SysPath: " << dev_info.syspath << std::endl;
    if (dev_info.devnode != "N/A") std::cout << "  DevNode: " << dev_info.devnode << std::endl;
    std::cout << "-------------------------------" << std::endl;


    // Log the event
    logger.log(log_message);

    std::cout << "Info: Placeholder for testing function for device "
              << dev_info.vendor_id << ":" << dev_info.product_id << std::endl;
    // --- End Placeholder ---
}


int main() {
    // Register signal handler for graceful shutdown
    signal(SIGINT, sigint_handler);

    std::cout << "Starting USB Device Utility..." << std::endl;

    // Initialize Logger
    Logger logger("usb_events.log");
    logger.log("Application started.");

    // List initially connected devices using libusb
    UsbLister lister; // RAII handles libusb init/exit
    std::vector<UsbDeviceInfo> initial_devices = lister.list_connected_devices();
    logger.log("Initial device scan complete. Found " + std::to_string(initial_devices.size()) + " devices.");
     for(const auto& dev : initial_devices) {
         std::string msg = "Initial Device: VID=" + dev.vendor_id + ", PID=" + dev.product_id;
         if(dev.manufacturer) msg += ", Manuf=" + *dev.manufacturer;
         if(dev.product) msg += ", Prod=" + *dev.product;
         if(dev.serial_number) msg += ", Serial=" + *dev.serial_number;
         msg += " (Node: " + dev.devnode + ")";
         logger.log(msg);
     }


    // Initialize Device Detector using libudev
    UsbDetector detector; // RAII handles udev init/exit

    // Main event loop - monitor for new devices
    while (keep_running) {
        // Create the callback lambda, capturing the logger by reference
        auto device_callback = [&](const UsbDeviceInfo& dev_info) {
            handle_new_device(dev_info, logger);
        };

        // monitor_for_devices blocks until an event or error
        if (!detector.monitor_for_devices(device_callback)) {
             // An error occurred in poll() or setting up the monitor fd
             if (keep_running) { // Check if error wasn't due to shutdown signal
                 logger.log("Error encountered during device monitoring. Exiting.");
                 std::cerr << "Error during device monitoring. Check logs. Exiting." << std::endl;
             }
             break; // Exit the loop on error
        }
        // Loop continues if keep_running is true and monitor_for_devices returned true
    }

    logger.log("Application shutting down.");
    std::cout << "USB Device Utility finished." << std::endl;

    return 0;
}