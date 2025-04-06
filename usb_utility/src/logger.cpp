#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip> // For std::put_time
#include <ctime>   // For std::time_t, std::localtime

Logger::Logger(const std::string& filename) {
    // Open in append mode
    log_file.open(filename, std::ios::app);
    if (!log_file.is_open()) {
        std::cerr << "Error: Could not open log file: " << filename << std::endl;
        // Consider throwing an exception or exiting
    } else {
        log("--- Log Started ---");
    }
}

Logger::~Logger() {
     if (log_file.is_open()) {
        log("--- Log Ended ---");
        log_file.close();
    }
}

std::string Logger::get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void Logger::log(const std::string& message) {
    if (!log_file.is_open()) {
        return; // Or handle error differently
    }
    std::lock_guard<std::mutex> lock(log_mutex); // Lock for thread safety
    log_file << get_timestamp() << " | " << message << std::endl;
}