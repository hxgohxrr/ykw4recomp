#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include "common/utilities/common.h"
#include "launcher_desktop.h"

int launcher_linux(int argc, char** argv) {
    if (argc != 1) return -1;
    char* executable = snx_executable(argv[0]);
    char* directory = executable ? snx_dirname(executable) : NULL;
    char* helper = directory ? snx_path_join(directory, "tools/desktop_launcher.py") : NULL;
    if (!helper || !snx_is_file(helper)) {
        fputs("Falta tools/desktop_launcher.py. Reinstala el launcher completo y Python 3.10+ con Tk.\n", stderr);
        free(helper); free(directory); free(executable);
        return 1;
    }
    pid_t child = fork();
    if (!child) {
        execlp("python3", "python3", helper, "--runtime", executable, (char*)NULL);
        perror("No se pudo abrir el launcher: instala Python 3.10+ con Tk");
        _exit(127);
    }
    int status = 0, result = 1;
    if (child > 0) {
        pid_t waited;
        do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
        if (waited == child && WIFEXITED(status)) result = WEXITSTATUS(status);
    }
    free(helper); free(directory); free(executable);
    return result;
}
