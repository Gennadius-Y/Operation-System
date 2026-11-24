#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <string.h>

#define SHM_SIZE 4096

void print_str(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    write(1, str, len);
}

// Простая функция для формирования уникальных имён вида "/prefix_12345" без stdio.h
void make_unique_name(char* dest, const char* prefix, pid_t pid) {
    int i = 0;
    while (prefix[i] != '\0') {
        dest[i] = prefix[i];
        i++;
    }
    char num[16];
    int n_idx = 0;
    int p = (int)pid;
    if (p == 0) {
        num[n_idx++] = '0';
    } else {
        while (p > 0) {
            num[n_idx++] = (p % 10) + '0';
            p /= 10;
        }
    }
    for (int j = n_idx - 1; j >= 0; j--) {
        dest[i++] = num[j];
    }
    dest[i] = '\0';
}

int main() {
    print_str("Введите имя файла: ");
    char filename[256];
    int n = read(0, filename, sizeof(filename) - 1);
    if (n <= 0) return 1;

    if (filename[n - 1] == '\n') filename[n - 1] = '\0';
    else filename[n] = '\0';

    // Генерируем уникальные имена для shared memory и семафоров
    pid_t my_pid = getpid();
    char shm_name[64];
    char sem_empty_name[64];
    char sem_full_name[64];

    make_unique_name(shm_name, "/shm_", my_pid);
    make_unique_name(sem_empty_name, "/sem_empty_", my_pid);
    make_unique_name(sem_full_name, "/sem_full_", my_pid);

    // Создаём объект разделяемой памяти в RAM
    int shm_fd = shm_open(shm_name, O_RDWR | O_CREAT | O_TRUNC, 0600);
    if (shm_fd == -1) {
        print_str("Ошибка shm_open\n");
        return 1;
    }

    // Задаём размер области памяти
    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        print_str("Ошибка ftruncate\n");
        shm_unlink(shm_name);
        return 1;
    }

    // Отображаем разделяемую память в адресное пространство процесса
    char* shm_buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_buf == MAP_FAILED) {
        print_str("Ошибка mmap\n");
        shm_unlink(shm_name);
        return 1;
    }

    // Создаём семафоры для синхронизации
    sem_t* sem_empty = sem_open(sem_empty_name, O_CREAT | O_EXCL, 0600, 1);
    sem_t* sem_full = sem_open(sem_full_name, O_CREAT | O_EXCL, 0600, 0);
    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED) {
        print_str("Ошибка sem_open\n");
        shm_unlink(shm_name);
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        print_str("Ошибка fork\n");
        return 1;
    }

    if (pid == 0) {
        // Передаём имя файла и имена IPC-объектов в дочерний процесс
        char* args[] = {"./child", filename, shm_name, sem_empty_name, sem_full_name, NULL};
        execve("./child", args, NULL);
        print_str("Ошибка execve\n");
        return 1;
    } else {
        print_str("Введите числа (Enter для отправки, Ctrl+D для выхода):\n");

        int* data_len = (int*)shm_buf;
        char* data_text = shm_buf + sizeof(int);
        int max_text_size = SHM_SIZE - sizeof(int);

        char buffer[1024];
        int bytes_read;

        while ((bytes_read = read(0, buffer, sizeof(buffer))) > 0) {
            if (bytes_read > max_text_size) {
                bytes_read = max_text_size;
            }

            sem_wait(sem_empty); // Ждём, пока ребёнок освободит буфер
            *data_len = bytes_read;
            memcpy(data_text, buffer, bytes_read);
            sem_post(sem_full);  // Сигнализируем ребёнку, что данные готовы
        }

        // Отправляем сигнал завершения работы (length = -1)
        sem_wait(sem_empty);
        *data_len = -1;
        sem_post(sem_full);

        waitpid(pid, NULL, 0);

        // Очищаем ресурсы
        sem_close(sem_empty);
        sem_close(sem_full);
        sem_unlink(sem_empty_name);
        sem_unlink(sem_full_name);

        munmap(shm_buf, SHM_SIZE);
        close(shm_fd);
        shm_unlink(shm_name);

        print_str("Работа завершена.\n");
    }

    return 0;
}
