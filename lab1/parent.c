#include <unistd.h>
#include <fcntl.h>

void print_str(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    write(1, str, len);
}

int main() {
    print_str("Введите имя файла: ");
    char filename[256];
    int n = read(0, filename, sizeof(filename) - 1);
    if (n <= 0) return 1;

    if (filename[n - 1] == '\n') filename[n - 1] = '\0';
    else filename[n] = '\0';

    int pipe1[2];
    int pipe2[2];
    if (pipe(pipe1) < 0 || pipe(pipe2) < 0) {
        print_str("Ошибка pipe\n");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        print_str("Ошибка fork\n");
        return 1;
    }

    if (pid == 0) {
        close(pipe1[1]);
        close(pipe2[0]);

        dup2(pipe1[0], 0);
        close(pipe1[0]);

        dup2(pipe2[1], 1);
        close(pipe2[1]);

        char* args[] = {"./child", filename, NULL};
        execve("./child", args, NULL);
        return 1;
    }
    else {
        close(pipe1[0]);
        close(pipe2[1]);

        print_str("Введите числа (Enter для отправки, Ctrl+D для выхода):\n");

        char buffer[1024];
        int bytes_read;

        while ((bytes_read = read(0, buffer, sizeof(buffer))) > 0) {
            write(pipe1[1], buffer, bytes_read);
        }

        close(pipe1[1]);
        close(pipe2[0]);

        waitpid(pid, NULL, 0);
        print_str("Работа завершена.\n");
    }

    return 0;
}
