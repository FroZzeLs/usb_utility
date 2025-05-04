#pragma once

#include "DeviceInfo.h"
#include <string>
#include <map>
#include <streambuf> 

class ResultDisplay {
public:
    static void prepareAndDisplay(const DeviceInfo& info, bool is_storage_device);

private:
    struct file_buf : std::streambuf {
        FILE* fp;
        file_buf(FILE* f);
        int overflow(int c) override;
        int sync() override;
    };
};