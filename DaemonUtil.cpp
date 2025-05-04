#include "DaemonUtil.h"
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <csignal>
#include <cstdlib>
#include <syslog.h>
#include <cstring> 
#include <cerrno>  

namespace DaemonUtil {

void daemonize() {
    pid_t pid = fork();
    if (pid < 0) { syslog(LOG_CRIT, "Ошибка fork() при демонизации: %s", strerror(errno)); exit(EXIT_FAILURE); }
    if (pid > 0) { exit(EXIT_SUCCESS); }

    if (setsid() < 0) { syslog(LOG_CRIT, "Ошибка setsid() при демонизации: %s", strerror(errno)); exit(EXIT_FAILURE); }

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    pid = fork();
    if (pid < 0) { syslog(LOG_CRIT, "Ошибка второго fork() при демонизации: %s", strerror(errno)); exit(EXIT_FAILURE); }
    if (pid > 0) { exit(EXIT_SUCCESS); }

    umask(0);

    if (chdir("/") < 0) { syslog(LOG_CRIT, "Ошибка chdir(\"/\") при демонизации: %s", strerror(errno)); }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    int fd0 = open("/dev/null", O_RDONLY);
    int fd1 = open("/dev/null", O_WRONLY);
    int fd2 = open("/dev/null", O_WRONLY);
    if (fd0 < 0 || fd1 < 0 || fd2 < 0) { exit(EXIT_FAILURE); } 
    if (fd0 != STDIN_FILENO || fd1 != STDOUT_FILENO || fd2 != STDERR_FILENO) {
        if (dup2(fd0, STDIN_FILENO) < 0) { syslog(LOG_WARNING, "Ошибка dup2 для stdin: %s", strerror(errno)); }
        if (dup2(fd1, STDOUT_FILENO) < 0) { syslog(LOG_WARNING, "Ошибка dup2 для stdout: %s", strerror(errno)); }
        if (dup2(fd2, STDERR_FILENO) < 0) { syslog(LOG_WARNING, "Ошибка dup2 для stderr: %s", strerror(errno)); }
        if (fd0 > 2) close(fd0);
        if (fd1 > 2) close(fd1);
        if (fd2 > 2) close(fd2);
    }
}
}