#include "DeviceTester.h"
#include <vector>
#include <chrono>
#include <iomanip>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring> 
#include <cerrno>  

long long DeviceTester::current_time_ms() {
     return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::high_resolution_clock::now().time_since_epoch())
        .count();
}

bool DeviceTester::check_data_integrity(const char* buffer_written, const char* buffer_read, size_t size) {
     return memcmp(buffer_written, buffer_read, size) == 0;
}

void DeviceTester::perform_tests_read_only(const std::string& block_dev_path, std::ostream& out_stream) {
    out_stream << "\n--- Тестирование чтения с устройства: " << block_dev_path << " ---\n";
    syslog(LOG_INFO, "[TesterRO] Начало тестирования ЧТЕНИЯ устройства: %s", block_dev_path.c_str());

    if (block_dev_path.empty()) {
        syslog(LOG_WARNING, "[TesterRO] Нет блочного устройства для тестирования чтения.");
        out_stream << "ОШИБКА: Нет блочного устройства для тестирования чтения.\n";
        out_stream << "--- Тестирование чтения завершено с ошибкой ---\n";
        return;
    }

    // Открываем устройство ТОЛЬКО для чтения
    int fd = open(block_dev_path.c_str(), O_RDONLY);
    if (fd < 0) {
        std::string error_msg = strerror(errno);
        syslog(LOG_ERR, "[TesterRO] Не удалось открыть устройство %s для чтения: %s (%d)", block_dev_path.c_str(), error_msg.c_str(), errno);
        out_stream << "ОШИБКА: Не удалось открыть устройство " << block_dev_path << " для чтения: " << error_msg << "\n";
        out_stream << "--- Тестирование чтения завершено с ошибкой ---\n";
        return;
    }

    const size_t block_size = 4096;             // Размер блока для чтения
    const size_t total_size_to_read = 100 * 1024 * 1024; // Попытаемся прочитать 100 MB
    // Мы не знаем реальный размер устройства, поэтому будем читать, пока не достигнем total_size_to_read или конца файла
    std::vector<char> read_buffer(block_size);  // Буфер для чтения

    out_stream << "Тест чтения:\n";
    long long start_time_read = current_time_ms(); // Используем локальную или глобальную current_time_ms
    size_t bytes_read_total = 0;
    bool read_ok = true;
    ssize_t current_read_bytes;

    while (bytes_read_total < total_size_to_read) {
        current_read_bytes = read(fd, read_buffer.data(), block_size);

        if (current_read_bytes < 0) { // Ошибка чтения
            std::string error_msg = strerror(errno);
            syslog(LOG_ERR, "[TesterRO] Ошибка чтения с %s (прочитано %zu байт): %s",
                   block_dev_path.c_str(), bytes_read_total, error_msg.c_str());
            out_stream << "  ОШИБКА чтения (прочитано " << (bytes_read_total / (1024.0 * 1024.0))
                       << " MB): " << error_msg << "\n";
            read_ok = false;
            break;
        }

        if (current_read_bytes == 0) { // Достигнут конец файла (устройства)
            syslog(LOG_INFO, "[TesterRO] Достигнут конец устройства %s после прочтения %zu байт.",
                   block_dev_path.c_str(), bytes_read_total);
            break;
        }
        bytes_read_total += current_read_bytes;
    }
    long long end_time_read = current_time_ms();
    double read_duration = (end_time_read - start_time_read) / 1000.0;

    if (read_ok && bytes_read_total > 0) {
        double read_speed = (read_duration > 0.0001) ? (bytes_read_total / (1024.0 * 1024.0)) / read_duration : 0;
        out_stream << std::fixed << std::setprecision(2);
        out_stream << "  Прочитано: " << (bytes_read_total / (1024.0 * 1024.0)) << " MB\n";
        out_stream << std::setprecision(3);
        out_stream << "  Время: " << read_duration << " сек\n";
        out_stream << std::setprecision(2);
        out_stream << "  Скорость: " << read_speed << " MB/s\n";
        syslog(LOG_INFO, "[TesterRO] Тест чтения %s: %.2f MB/s (прочитано %.2f MB)",
               block_dev_path.c_str(), read_speed, (bytes_read_total / (1024.0 * 1024.0)));
    } else if (!read_ok) {
        out_stream << "  Тест чтения: НЕУДАЧА (ошибка во время чтения)\n";
    } else { // bytes_read_total == 0
        out_stream << "  Тест чтения: Не удалось прочитать данные (0 байт).\n";
        syslog(LOG_WARNING, "[TesterRO] Тест чтения %s: 0 байт прочитано.", block_dev_path.c_str());
    }
    out_stream << "--- Тестирование чтения завершено ---\n";
    close(fd);
}