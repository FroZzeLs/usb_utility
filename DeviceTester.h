#pragma once

#include <string>
#include <ostream>

class DeviceTester {
public:
    static void performTests(const std::string& block_dev_path, std::ostream& out_stream);

private:
    static long long current_time_ms();
    static bool check_data_integrity(const char* buffer_written, const char* buffer_read, size_t size);
};