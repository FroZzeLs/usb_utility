#pragma once 

#include <string>

struct DeviceInfo {
    std::string devpath;        
    std::string vendor_id;
    std::string product_id;
    std::string manufacturer;
    std::string product_name;
    std::string block_device;  
    bool results_displayed;     
    bool is_likely_storage; 
};