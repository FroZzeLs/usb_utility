#include "ResultDisplay.h"
#include "DeviceTester.h" // РАСКОММЕНТИРОВАНО
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
#include <streambuf>

ResultDisplay::file_buf::file_buf(FILE* f) : fp(f) {}
int ResultDisplay::file_buf::overflow(int c) { return fputc(c, fp) == EOF ? EOF : c; }
int ResultDisplay::file_buf::sync() { return fflush(fp) == 0 ? 0 : -1; }


void ResultDisplay::prepareAndDisplay(const DeviceInfo& info, bool is_storage_device) {
    char log_filename_template[] = "/var/tmp/usb_monitor_XXXXXX";
    int fd = mkstemp(log_filename_template);

    if (fd < 0) { /* ... (обработка ошибки mkstemp) ... */ return; }

    FILE* log_file_c = fdopen(fd, "w");
    if (!log_file_c) { /* ... (обработка ошибки fdopen) ... */ return; }

    syslog(LOG_INFO, "[Display] Запись информации и результатов в %s", log_filename_template);

    fprintf(log_file_c, "========================================\n");
    fprintf(log_file_c, "Информация об устройстве:\n");
    fprintf(log_file_c, "  Путь sysfs (USB): %s\n", info.devpath.c_str());
    fprintf(log_file_c, "  VID: %s\n", info.vendor_id.empty() ? "N/A" : info.vendor_id.c_str());
    fprintf(log_file_c, "  PID: %s\n", info.product_id.empty() ? "N/A" : info.product_id.c_str());
    // ... (остальные fprintf для информации об устройстве) ...
    fprintf(log_file_c, "  Блочное устройство: %s\n", info.block_device.empty() ? "N/A" : info.block_device.c_str());
    if (is_storage_device && !info.capacity_gb.empty() && info.capacity_gb != "N/A") { // Добавлено поле capacity_gb в DeviceInfo
         fprintf(log_file_c, "  Объем накопителя: %s GB\n", info.capacity_gb.c_str());
    }
    fprintf(log_file_c, "  Является накопителем: %s\n", is_storage_device ? "Да" : "Нет");
    fprintf(log_file_c, "========================================\n");
    fflush(log_file_c);

    struct file_buf log_streambuf(log_file_c);
    std::ostream log_ostream(&log_streambuf);
    DeviceTester tester;

    if (is_storage_device) {
        syslog(LOG_INFO, "[Display] Устройство %s является накопителем. Запуск теста ЧТЕНИЯ.", info.devpath.c_str());
        // *** ВЫЗЫВАЕМ ТЕСТ ТОЛЬКО ЧТЕНИЯ ***
        tester.perform_tests_read_only(info.block_device, log_ostream);
    } else {
        syslog(LOG_INFO, "[Display] Устройство %s не является накопителем. Тесты не выполняются.", info.devpath.c_str());
        fprintf(log_file_c, "\nТесты производительности не выполнялись (устройство не является накопителем).\n");
    }

    fclose(log_file_c);
    syslog(LOG_INFO, "[Display] Информация и результаты для %s сохранены в %s.", info.devpath.c_str(), log_filename_template);

    // ... (код вызова zenity и unlink без изменений) ...
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
         else { syslog(LOG_WARNING, "[Display] Команда zenity завершилась с ошибкой (system ret: %d, exit status: %d).", ret, exit_status); }
         syslog(LOG_INFO, "[Display] Окно с результатами не показано или закрыто с ошибкой. Информация доступна в %s", log_filename_template);
     }
    unlink(log_filename_template);
    syslog(LOG_DEBUG, "[Display] Временный файл %s удален.", log_filename_template);
}