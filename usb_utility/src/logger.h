#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <fstream>
#include <mutex> // For thread safety if needed later

class Logger {
public:
    Logger(const std::string& filename);
    ~Logger();

    void log(const std::string& message);

    // Disable copy and assignment
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    std::ofstream log_file;
    std::mutex log_mutex; // Ensures thread-safe writes

    std::string get_timestamp();
};

#endif // LOGGER_H