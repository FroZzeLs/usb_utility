#include "Application.h"
#include <iostream>
#include <cstring> 

int main(int argc, char *argv[]) {
    bool run_as_daemon = false;
    if (argc > 1 && strcmp(argv[1], "-d") == 0) {
        run_as_daemon = true;
    }

    try {
        Application app(run_as_daemon);
        return app.run();
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка при создании Application: " << e.what() << std::endl;
        return 1;
    } catch (...) {
         std::cerr << "Неизвестная критическая ошибка при создании Application." << std::endl;
         return 1;
    }
}