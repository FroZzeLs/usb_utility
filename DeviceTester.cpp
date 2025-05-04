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

void DeviceTester::performTests(const std::string& block_dev_path, std::ostream& out_stream) {
    out_stream << "\n--- Тестирование устройства: " << block_dev_path << " ---\n";
    syslog(LOG_INFO, "[Tester] Начало тестирования устройства: %s", block_dev_path.c_str());

    if (block_dev_path.empty()) {
        syslog(LOG_WARNING, "[Tester] Нет блочного устройства для тестирования.");
        out_stream << "ОШИБКА: Нет блочного устройства для тестирования.\n";
        out_stream << "--- Тестирование завершено с ошибкой ---\n";
        return;
    }

    int fd = open(block_dev_path.c_str(), O_RDWR | O_DSYNC);
    if (fd < 0) {
        std::string error_msg = strerror(errno);
        syslog(LOG_ERR, "[Tester] Не удалось открыть устройство %s: %s (%d)", block_dev_path.c_str(), error_msg.c_str(), errno);
        out_stream << "ОШИБКА: Не удалось открыть устройство " << block_dev_path << ": " << error_msg << "\n";
        out_stream << "Подсказка: Для прямого доступа могут требоваться права root.\n";
        out_stream << "--- Тестирование завершено с ошибкой ---\n";
        return;
    }

    const size_t block_size = 4096;
    const size_t total_size = 10 * 1024 * 1024;
    const size_t num_blocks = total_size / block_size;
    std::vector<char> write_buffer(block_size);
    std::vector<char> read_buffer(block_size);
    for (size_t i = 0; i < block_size; ++i) write_buffer[i] = static_cast<char>(i % 256);

    bool write_ok = true;
    double write_speed = 0;
    size_t bytes_written = 0;

    out_stream << "Тест записи:\n";
    long long start_time_write = current_time_ms();
    for (size_t i = 0; i < num_blocks; ++i) {
         ssize_t written = write(fd, write_buffer.data(), block_size);
         if (written != (ssize_t)block_size) {
             std::string error_msg = (written < 0) ? strerror(errno) : "Записано меньше, чем запрошено";
             syslog(LOG_ERR, "[Tester] Ошибка записи на %s (блок %zu): %s", block_dev_path.c_str(), i, error_msg.c_str());
             out_stream << "  ОШИБКА записи на блоке " << i << ": " << error_msg << "\n";
             write_ok = false;
             break;
         }
         bytes_written += written;
     }
    long long end_time_write = current_time_ms();
    double write_duration = (end_time_write - start_time_write) / 1000.0;

    if (write_ok) {
        write_speed = (write_duration > 0.0001) ? (bytes_written / (1024.0 * 1024.0)) / write_duration : 0;
        out_stream << std::fixed << std::setprecision(2);
        out_stream << "  Записано: " << (bytes_written / (1024.0 * 1024.0)) << " MB\n";
        out_stream << std::setprecision(3);
        out_stream << "  Время: " << write_duration << " сек\n";
        out_stream << std::setprecision(2);
        out_stream << "  Скорость: " << write_speed << " MB/s\n";
        syslog(LOG_INFO, "[Tester] Тест записи %s: %.2f MB/s", block_dev_path.c_str(), write_speed);
    } else {
        out_stream << "  Результат: НЕУДАЧА\n";
        syslog(LOG_ERR, "[Tester] Тест записи %s: НЕУДАЧА", block_dev_path.c_str());
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
         std::string error_msg = strerror(errno);
         syslog(LOG_ERR, "[Tester] Ошибка lseek на %s: %s (%d)", block_dev_path.c_str(), error_msg.c_str(), errno);
         out_stream << "ОШИБКА: Не удалось перемотать в начало устройства: " << error_msg << "\n";
         close(fd);
         out_stream << "--- Тестирование прервано ---";
         return;
     }

    out_stream << "Тест чтения и целостности данных:\n";
    bool read_ok = true;
    bool integrity_ok = true;
    double read_speed = 0;
    size_t bytes_read = 0;

    long long start_time_read = current_time_ms();
     for (size_t i = 0; i < num_blocks; ++i) {
         ssize_t was_read = read(fd, read_buffer.data(), block_size);
         if (was_read != (ssize_t)block_size) {
             std::string error_msg = (was_read < 0) ? strerror(errno) : "Прочитано меньше, чем запрошено";
             syslog(LOG_ERR, "[Tester] Ошибка чтения с %s (блок %zu): %s", block_dev_path.c_str(), i, error_msg.c_str());
             out_stream << "  ОШИБКА чтения на блоке " << i << ": " << error_msg << "\n";
             read_ok = false;
             break;
         }
         bytes_read += was_read;
         if (write_ok && integrity_ok && !check_data_integrity(write_buffer.data(), read_buffer.data(), block_size)) {
             integrity_ok = false;
             syslog(LOG_ERR, "[Tester] Ошибка целостности данных на %s (блок %zu)", block_dev_path.c_str(), i);
             out_stream << "  ОШИБКА целостности данных на блоке " << i << "\n";
         }
     }
    long long end_time_read = current_time_ms();
    double read_duration = (end_time_read - start_time_read) / 1000.0;

    if (read_ok) {
        read_speed = (read_duration > 0.0001) ? (bytes_read / (1024.0 * 1024.0)) / read_duration : 0;
        out_stream << std::fixed << std::setprecision(2);
        out_stream << "  Прочитано: " << (bytes_read / (1024.0 * 1024.0)) << " MB\n";
        out_stream << std::setprecision(3);
        out_stream << "  Время: " << read_duration << " сек\n";
        out_stream << std::setprecision(2);
        out_stream << "  Скорость: " << read_speed << " MB/s\n";
        syslog(LOG_INFO, "[Tester] Тест чтения %s: %.2f MB/s", block_dev_path.c_str(), read_speed);

        out_stream << "  Проверка целостности (пакетов):\n";
         if (write_ok) {
             if (integrity_ok) {
                 out_stream << "    Результат: УСПЕШНО\n";
                 syslog(LOG_INFO, "[Tester] Проверка целостности %s: УСПЕШНО", block_dev_path.c_str());
             } else {
                 out_stream << "    Результат: НЕУДАЧА (данные не совпадают)\n";
                 syslog(LOG_ERR, "[Tester] Проверка целостности %s: НЕУДАЧА", block_dev_path.c_str());
             }
        } else {
             out_stream << "    Результат: НЕ ПРОВОДИЛАСЬ (ошибка при записи)\n";
             syslog(LOG_WARNING, "[Tester] Проверка целостности %s: НЕ ПРОВОДИЛАСЬ", block_dev_path.c_str());
        }
    } else {
        out_stream << "  Тест чтения: НЕУДАЧА\n";
        syslog(LOG_ERR, "[Tester] Тест чтения %s: НЕУДАЧА", block_dev_path.c_str());
        out_stream << "  Проверка целостности (пакетов):\n";
        out_stream << "    Результат: НЕУДАЧА (ошибка чтения)\n";
        syslog(LOG_ERR, "[Tester] Проверка целостности %s: НЕУДАЧА (ошибка чтения)", block_dev_path.c_str());
     }

    close(fd);
    out_stream << "--- Тестирование завершено ---\n";
}