#include "Application.h"
#include "DaemonUtil.h"
#include "ResultDisplay.h" 
#include <iostream>
#include <syslog.h>
#include <stdexcept>
#include <cstdlib>      
#include <cstring>      
#include <libudev.h>    
#include <fstream>      
#include <sstream>      
#include <iomanip>      


std::atomic<Application*> Application::instance_{nullptr};
std::atomic<bool> Application::running_{false};

Application::Application(bool as_daemon)
 : is_daemon_(as_daemon), udev_monitor_(running_) 
{
    instance_.store(this); 
    if (is_daemon_) {
        DaemonUtil::daemonize(); 
        
    } else {
        
        openlog("usb_monitor_fg", LOG_PID | LOG_PERROR, LOG_USER);
        syslog(LOG_INFO, "USB Monitor запущен в foreground режиме.");
        std::cout << "USB Monitor запущен в foreground режиме. Нажмите Ctrl+C для выхода." << std::endl;
        std::cout << "Логи также пишутся в syslog (и дублируются в stderr)." << std::endl;
    }
}

Application::~Application() {
    cleanup();
    instance_.store(nullptr);
}

bool Application::initialize() {
    syslog(LOG_DEBUG, "[App::initialize] Начало инициализации...");
    if (!udev_monitor_.initialize()) {
        syslog(LOG_CRIT, "[App::initialize] Ошибка инициализации UdevMonitor.");
        return false;
    }
    setupSignalHandlers();
    syslog(LOG_DEBUG, "[App::initialize] Инициализация завершена успешно.");
    return true;
}

void Application::cleanup() {
    syslog(LOG_INFO, "[App] Завершение работы USB Monitor.");
    closelog();
    if (!is_daemon_) {
        std::cout << "\nUSB Monitor остановлен." << std::endl;
    }
}

void Application::setupSignalHandlers() {
    signal(SIGINT, Application::staticSignalHandler);
    signal(SIGTERM, Application::staticSignalHandler);
}

void Application::staticSignalHandler(int signum) {
    Application* app = instance_.load();
    if (app) {
        app->handleSignal(signum);
    } else {
        syslog(LOG_WARNING, "[Signal] Получен сигнал %d, но экземпляр Application не найден.", signum);
        running_.store(false); 
    }
}

void Application::handleSignal(int signum) {
    syslog(LOG_INFO, "[App] Получен сигнал %d, инициирую остановку...", signum);
    running_.store(false); 
    udev_monitor_.stop(); 
}

int Application::run() {
    syslog(LOG_INFO, "[App::run] Попытка инициализации...");
    if (!initialize()) {
        syslog(LOG_ERR, "[App::run] Инициализация не удалась.");
        cleanup();
        return 1;
    }

    syslog(LOG_INFO, "[App::run] Инициализация успешна. Запуск UdevMonitor::run...");
    
    udev_monitor_.run([this](struct udev_device* dev){
        this->onDeviceEvent(dev);
    });

    
    syslog(LOG_INFO, "[App::run] UdevMonitor::run завершен. Вызов cleanup...");
    cleanup();
    syslog(LOG_INFO, "[App::run] Приложение завершает работу.");
    return 0;
}


bool Application::hasMassStorageInterface(struct udev_device* usb_dev) {
    if (!usb_dev) return false;
    const char* bNumConfigurations_str = udev_device_get_sysattr_value(usb_dev, "bNumConfigurations");
    if (!bNumConfigurations_str) return false;
    int num_configs = std::atoi(bNumConfigurations_str);
    if (num_configs <= 0) return false;

    struct udev* udev_ctx = udev_device_get_udev(usb_dev);
    if (!udev_ctx) return false;

    struct udev_enumerate* enumerate = udev_enumerate_new(udev_ctx);
    if (!enumerate) return false;

    bool found_mass_storage = false;
    try {
        udev_enumerate_add_match_parent(enumerate, usb_dev);
        udev_enumerate_add_match_subsystem(enumerate, "usb");

        udev_enumerate_scan_devices(enumerate);
        struct udev_list_entry* devices = udev_enumerate_get_list_entry(enumerate);
        struct udev_list_entry* entry;

        udev_list_entry_foreach(entry, devices) {
            const char* path = udev_list_entry_get_name(entry);
            struct udev_device* interface_dev = udev_device_new_from_syspath(udev_ctx, path);
            if (interface_dev) {
                const char* bInterfaceClass_str = udev_device_get_sysattr_value(interface_dev, "bInterfaceClass");
                 syslog(LOG_DEBUG, "[App::hasMassStorage] --> Проверка интерфейса %s: bInterfaceClass=%s", path, bInterfaceClass_str ? bInterfaceClass_str : "N/A");
                if (bInterfaceClass_str && strcmp(bInterfaceClass_str, "08") == 0) {
                    found_mass_storage = true;
                    udev_device_unref(interface_dev);
                    break;
                }
                udev_device_unref(interface_dev);
            }
        }
    } catch(...) {
         syslog(LOG_ERR, "[App::hasMassStorage] Исключение при проверке интерфейсов.");
    }
    udev_enumerate_unref(enumerate);
    return found_mass_storage;
}



void Application::onDeviceEvent(struct udev_device* dev) {
    const char* action = udev_device_get_action(dev);
    if (!action || (strcmp(action, "add") != 0 && strcmp(action, "remove") != 0)) {
        return;
    }

    const char* subsystem = udev_device_get_subsystem(dev);
    const char* devpath = udev_device_get_devpath(dev);
    const char* devtype = udev_device_get_devtype(dev);

    if (!subsystem || !devpath) {
        syslog(LOG_WARNING, "[App::onDeviceEvent] Получено событие '%s' без подсистемы или пути.", action ? action : "unknown");
        return;
    }

    
    if (strcmp(action, "remove") == 0 && strcmp(subsystem, "usb") == 0) {
        auto it = active_devices_map_.find(devpath);
        if (it != active_devices_map_.end()) {
            syslog(LOG_INFO, "[App] USB устройство отключено: %s (Произв: %s, Устр: %s)",
                    devpath, it->second.manufacturer.c_str(), it->second.product_name.c_str());
            active_devices_map_.erase(it);
        }
        return;
    }

    
    if (strcmp(action, "add") == 0 && strcmp(subsystem, "usb") == 0 && devtype && strcmp(devtype, "usb_device") == 0) {
        if (active_devices_map_.count(devpath)) {
             syslog(LOG_DEBUG, "[App] Повторное событие USB add для %s, игнорируем базовую обработку.", devpath);
             
             if (!active_devices_map_[devpath].results_displayed && !active_devices_map_[devpath].is_likely_storage) {
                 syslog(LOG_WARNING, "[App] Повторное USB add для %s (не накопитель), но окно еще не было показано. Показываем базовую информацию.", devpath);
                 ResultDisplay::prepareAndDisplay(active_devices_map_[devpath], false);
                 active_devices_map_[devpath].results_displayed = true; 
             }
             return;
        }

        DeviceInfo info;
        info.devpath = devpath;
        info.results_displayed = false;
        info.block_device = "";
        info.capacity_gb = "N/A"; 

        const char* vid = udev_device_get_sysattr_value(dev, "idVendor");
        const char* pid = udev_device_get_sysattr_value(dev, "idProduct");
        const char* manuf = udev_device_get_sysattr_value(dev, "manufacturer");
        const char* prod = udev_device_get_sysattr_value(dev, "product");
        info.vendor_id = vid ? vid : ""; info.product_id = pid ? pid : "";
        info.manufacturer = manuf ? manuf : ""; info.product_name = prod ? prod : "";

        syslog(LOG_INFO, "[App] Обработка USB add: VID=%s, PID=%s, Manuf='%s', Prod='%s', Path=%s",
               info.vendor_id.c_str(), info.product_id.c_str(), info.manufacturer.c_str(), info.product_name.c_str(),
               info.devpath.c_str());

        info.is_likely_storage = hasMassStorageInterface(dev);
        syslog(LOG_INFO, "[App] Устройство %s %s содержать Mass Storage интерфейс.",
               info.devpath.c_str(), info.is_likely_storage ? "похоже, что" : "НЕ похоже, что");

        active_devices_map_[info.devpath] = info; 

        if (!info.is_likely_storage) {
             syslog(LOG_INFO, "[App] Вызов отображения базовой информации для НЕ-накопителя %s", info.devpath.c_str());
             ResultDisplay::prepareAndDisplay(info, false);
             active_devices_map_[info.devpath].results_displayed = true; 
        } else {
             syslog(LOG_DEBUG, "[App] Устройство %s похоже на накопитель, ожидаем событие block add.", info.devpath.c_str());
        }
        return;
    }

    
    if (strcmp(action, "add") == 0 && strcmp(subsystem, "block") == 0) {
        const char* id_bus = udev_device_get_property_value(dev, "ID_BUS");
        const char* id_type = udev_device_get_property_value(dev, "ID_TYPE");
        const char* devnode = udev_device_get_devnode(dev);
        const char* block_devtype = udev_device_get_devtype(dev);

        syslog(LOG_DEBUG, "[App] Обработка block add: devpath=%s, devnode=%s, devtype=%s, ID_BUS=%s, ID_TYPE=%s",
               devpath ? devpath : "N/A", devnode ? devnode : "N/A", block_devtype ? block_devtype : "N/A",
               id_bus ? id_bus : "N/A", id_type ? id_type : "N/A");

        if (devnode && block_devtype && strcmp(block_devtype, "disk") == 0 &&
            id_bus && strcmp(id_bus, "usb") == 0 && id_type && strcmp(id_type, "disk") == 0)
        {
            syslog(LOG_INFO, "[App] Найдено блочное USB-устройство: %s", devnode);

            struct udev_device* parent_usb_dev = udev_device_get_parent_with_subsystem_devtype(
                                                    dev, "usb", "usb_device");

            if (parent_usb_dev) {
                const char* parent_devpath = udev_device_get_devpath(parent_usb_dev);
                syslog(LOG_DEBUG, "[App] Найден родительский USB путь: %s для блочного устройства %s",
                       parent_devpath ? parent_devpath : "N/A", devnode);

                if (parent_devpath) {
                    auto it = active_devices_map_.find(parent_devpath);
                    if (it != active_devices_map_.end()) {
                        DeviceInfo& stored_info = it->second;
                        if (!stored_info.results_displayed) { 
                            stored_info.block_device = devnode;

                            
                            stored_info.capacity_gb = "N/A"; 
                            const char* device_name = strrchr(devnode, '/'); 
                            if (device_name) {
                                device_name++;
                                std::string size_path = "/sys/block/";
                                size_path += device_name;
                                size_path += "/size";
                                std::ifstream size_file(size_path);
                                if (size_file.is_open()) {
                                    unsigned long long sectors = 0;
                                    if (size_file >> sectors) {
                                        unsigned long long total_bytes = sectors * 512;
                                        double capacity_gb_double = static_cast<double>(total_bytes) / (1024.0 * 1024.0 * 1024.0);
                                        std::stringstream ss;
                                        ss << std::fixed << std::setprecision(1) << capacity_gb_double;
                                        stored_info.capacity_gb = ss.str();
                                        syslog(LOG_INFO, "[App] Объем %s: %llu секторов = %s GB", devnode, sectors, stored_info.capacity_gb.c_str());
                                    } else {
                                        syslog(LOG_WARNING, "[App] Не удалось прочитать число секторов из %s", size_path.c_str());
                                    }
                                    size_file.close();
                                } else {
                                     syslog(LOG_WARNING, "[App] Не удалось открыть файл sysfs для получения размера: %s", size_path.c_str());
                                }
                            } else {
                                 syslog(LOG_WARNING, "[App] Не удалось извлечь имя устройства из %s", devnode);
                            }
                            

                            stored_info.results_displayed = true; 
                            syslog(LOG_INFO, "[App] Связь установлена. ВЫЗОВ отображения/тестов для накопителя %s (%s)",
                                   parent_devpath, devnode);
                            ResultDisplay::prepareAndDisplay(stored_info, true); 
                        } else {
                             syslog(LOG_DEBUG, "[App] Окно для USB %s уже было показано, игнорируем событие block add для %s.", parent_devpath, devnode);
                        }
                    } else {
                        syslog(LOG_WARNING, "[App] !!! Не найдена информация о USB-родителе %s в карте для %s при событии block add.", parent_devpath, devnode);
                         
                    }
                } else { syslog(LOG_WARNING, "[App] Не удалось получить devpath для родительского USB устройства %s", devnode); }
            } else {
                syslog(LOG_WARNING, "[App] Не удалось найти родительское USB устройство для %s при событии block add.", devnode);
            }
        }
        return;
    }
     if (strcmp(action, "add") == 0) {
        syslog(LOG_DEBUG, "[App] Игнорируется событие add: subsystem=%s, devtype=%s, devpath=%s",
            subsystem, devtype ? devtype : "N/A", devpath);
    }
}