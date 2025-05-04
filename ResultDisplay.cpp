#include "ResultDisplay.h"
#include "DeviceTester.h" 
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdlib>   
#include <unistd.h>  
#include <cstdio>    
#include <syslog.h>
#include <cstring> 
#include <cerrno>  
#include <sys/wait.h> 

ResultDisplay::file_buf::file_buf(FILE* f) : fp(f) {}
int ResultDisplay::file_buf::overflow(int c) { return fputc(c, fp) == EOF ? EOF : c; }
int ResultDisplay::file_buf::sync() { return fflush(fp) == 0 ? 0 : -1; }


void ResultDisplay::prepareAndDisplay(const DeviceInfo& info, bool is_storage_device) {
    char log_filename_template[] = "/var/tmp/usb_monitor_XXXXXX";
    int fd = mkstemp(log_filename_template);

    if (fd < 0) {
        syslog(LOG_ERR, "[Display] Не удалось создать временный файл (%s): %s", log_filename_template, strerror(errno));
        return;
    }

    FILE* log_file_c = fdopen(fd, "w");
    if (!log_file_c) {
        syslog(LOG_ERR, "[Display] Не удалось открыть FILE* для временного файла (fd=%d): %s", fd, strerror(errno));
        close(fd);
        unlink(log_filename_template);
        return;
    }

    syslog(LOG_INFO, "[Display] Запись информации и результатов в %s", log_filename_template);

    fprintf(log_file_c, "========================================\n");
    fprintf(log_file_c, "Информация об устройстве:\n");
    fprintf(log_file_c, "  Путь sysfs (USB): %s\n", info.devpath.c_str());
    fprintf(log_file_c, "  VID: %s\n", info.vendor_id.empty() ? "N/A" : info.vendor_id.c_str());
    fprintf(log_file_c, "  PID: %s\n", info.product_id.empty() ? "N/A" : info.product_id.c_str());
    fprintf(log_file_c, "  Производитель: %s\n", info.manufacturer.empty() ? "N/A" : info.manufacturer.c_str());
    fprintf(log_file_c, "  Устройство: %s\n", info.product_name.empty() ? "N/A" : info.product_name.c_str());
    fprintf(log_file_c, "  Блочное устройство: %s\n", info.block_device.empty() ? "N/A" : info.block_device.c_str());
    fprintf(log_file_c, "  Является накопителем: %s\n", is_storage_device ? "Да" : "Нет");
    fprintf(log_file_c, "========================================\n");
    fflush(log_file_c);

    file_buf log_streambuf(log_file_c);
    std::ostream log_ostream(&log_streambuf);

    if (is_storage_device) {
        syslog(LOG_INFO, "[Display] Устройство %s является накопителем. Запуск тестов.", info.devpath.c_str());
        DeviceTester::performTests(info.block_device, log_ostream); // Используем статический метод
    } else {
        syslog(LOG_INFO, "[Display] Устройство %s не является накопителем. Тесты не выполняются.", info.devpath.c_str());
        fprintf(log_file_c, "\nТесты производительности и целостности не выполнялись (устройство не является накопителем).\n");
    }

    fclose(log_file_c);
    syslog(LOG_INFO, "[Display] Информация и результаты для %s сохранены в %s.", info.devpath.c_str(), log_filename_template);

    std::string command = "zenity --text-info --title=\"Информация об USB: ";
    std::string device_label = info.product_name.empty() ? (info.vendor_id + ":" + info.product_id) : info.product_name;
    size_t pos = 0;
    while ((pos = device_label.find('"', pos)) != std::string::npos) { device_label.replace(pos, 1, "\\\""); pos += 2; }
    command += device_label;
    command += "\" --filename='";
    command += log_filename_template;
    command += "' --width=800 --height=600";

    syslog(LOG_INFO, "[Display] ПОПЫТКА ВЫПОЛНЕНИЯ КОМАНДЫ: %s", command.c_str());
    int ret = system(command.c_str());
    syslog(LOG_INFO, "[Display] КОМАНДА ЗАВЕРШЕНА. Код возврата system(): %d", ret);

    int exit_status = -1;
    if (WIFEXITED(ret)) { exit_status = WEXITSTATUS(ret); }

     if (ret == 0 || exit_status == 1) { syslog(LOG_INFO, "[Display] Команда zenity предположительно выполнена успешно."); }
    else {
         if (exit_status == 127) { syslog(LOG_WARNING, "[Display] Команда zenity не найдена (exit status 127). Установите пакет 'zenity'."); }
         else { syslog(LOG_WARNING, "[Display] Команда zenity завершилась с ошибкой (system ret: %d, exit status: %d). Проблемы с доступом к GUI сессии?", ret, exit_status); }
         syslog(LOG_INFO, "[Display] Окно с результатами не показано или закрыто с ошибкой. Информация доступна в %s", log_filename_template);
     }

    unlink(log_filename_template);
    syslog(LOG_DEBUG, "[Display] Временный файл %s удален.", log_filename_template);
}