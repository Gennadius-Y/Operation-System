#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

#define RUN 64

// Структура для передачи нескольких аргументов в поток через void*
typedef struct {
    int* arr;
    int left;
    int right;
} ThreadArg;

//Кастомные функции вывода (замена запрещенной stdio.h)

void print_str(const char* str) {
    int len = 0;
    while (str[len] != '\0') len++;
    write(1, str, len); // Системный вызов write в stdout
}

void print_int(long long num) {
    if (num == 0) {
        write(1, "0", 1);
        return;
    }
    char buf[32];
    int i = 0;
    int is_negative = 0;
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    while (num > 0) {
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }
    if (is_negative) {
        write(1, "-", 1);
    }
    while (i > 0) {
        write(1, &buf[--i], 1);
    }
}

int custom_atoi(const char* str) {
    int res = 0;
    int i = 0;
    while (str[i] >= '0' && str[i] <= '9') {
        res = res * 10 + (str[i] - '0');
        i++;
    }
    return res;
}

// Алгоритмическая часть (TimSort)

// Локальная сортировка вставками для блоков RUN
void insertion_sort(int* arr, int left, int right) {
    for (int i = left + 1; i <= right; i++) {
        int temp = arr[i];
        int j = i - 1;
        while (j >= left && arr[j] > temp) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = temp;
    }
}

// Слияние двух отсортированных частей подмассива
void merge(int* arr, int l, int m, int r) {
    int len1 = m - l + 1;
    int len2 = r - m;

    int* left = (int*)malloc(sizeof(int) * len1);
    int* right = (int*)malloc(sizeof(int) * len2);

    for (int i = 0; i < len1; i++) left[i] = arr[l + i];
    for (int i = 0; i < len2; i++) right[i] = arr[m + 1 + i];

    int i = 0, j = 0, k = l;
    while (i < len1 && j < len2) {
        if (left[i] <= right[j]) {
            arr[k] = left[i]; i++;
        } else {
            arr[k] = right[j]; j++;
        }
        k++;
    }

    while (i < len1) { arr[k] = left[i]; i++; k++; }
    while (j < len2) { arr[k] = right[j]; j++; k++; }

    free(left);
    free(right);
}

// Функция воркера: параллельный Insertion Sort для выделенных блоков
void* parallel_insertion_worker(void* arg) {
    ThreadArg* t_arg = (ThreadArg*)arg;
    for (int i = t_arg->left; i <= t_arg->right; i += RUN) {
        int end = (i + RUN - 1 < t_arg->right) ? (i + RUN - 1) : t_arg->right;
        insertion_sort(t_arg->arr, i, end);
    }
    return NULL;
}

void tim_sort(int* arr, int n, int max_threads) {
    if (max_threads < 1) max_threads = 1;

    int total_runs = (n + RUN - 1) / RUN;
    if (max_threads > total_runs) max_threads = total_runs;

    pthread_t* threads = (pthread_t*)malloc(sizeof(pthread_t) * max_threads);
    ThreadArg* args = (ThreadArg*)malloc(sizeof(ThreadArg) * max_threads);

    int runs_per_thread = total_runs / max_threads;
    int remainder_runs = total_runs % max_threads;

    int current_run = 0;
    // Разделяем массив на независимые куски и запускаем потоки
    for (int i = 0; i < max_threads; i++) {
        int thread_runs = runs_per_thread + (i < remainder_runs ? 1 : 0);

        args[i].arr = arr;
        args[i].left = current_run * RUN;
        current_run += thread_runs;
        args[i].right = (current_run * RUN - 1 < n - 1) ? (current_run * RUN - 1) : (n - 1);

        pthread_create(&threads[i], NULL, parallel_insertion_worker, &args[i]);
    }

    // Барьер синхронизации: ждем завершения первой фазы всеми потоками
    for (int i = 0; i < max_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    free(args);

    // Вторая фаза: последовательное слияние блоков RUN главным потоком
    for (int size = RUN; size < n; size = 2 * size) {
        for (int left = 0; left < n; left += 2 * size) {
            int mid = left + size - 1;
            int right = (left + 2 * size - 1 < n - 1) ? (left + 2 * size - 1) : (n - 1);

            if (mid < right) {
                merge(arr, left, mid, right);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    // Проверка ключа запуска (количество потоков)
    if (argc < 2) {
        print_str("Использование: ./timsort_parallel <количество_потоков>\n");
        return 1;
    }

    int max_threads = custom_atoi(argv[1]);
    int n = 500000;
    int* arr = (int*)malloc(sizeof(int) * n);

    // Заполнение в обратном порядке (worst case)
    for (int i = 0; i < n; i++) {
        arr[i] = n - i;
    }

    // Высокоточный замер времени средствами ОС
    struct timespec start, end;
    clock_gettime(CLOCK_MONOTONIC, &start);

    tim_sort(arr, n, max_threads);

    clock_gettime(CLOCK_MONOTONIC, &end);
    long long elapsed_ms = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_nsec - start.tv_nsec) / 1000000;

    // Проверка корректности работы алгоритма
    int sorted = 1;
    for (int i = 0; i < n - 1; i++) {
        if (arr[i] > arr[i + 1]) {
            sorted = 0;
            break;
        }
    }

    print_str("Потоков: "); print_int(max_threads);
    print_str(" | Время: "); print_int(elapsed_ms);
    print_str(" мс | Статус сортировки: ");
    if (sorted) {
        print_str("УСПЕШНО\n");
    } else {
        print_str("ОШИБКА\n");
    }

    free(arr);
    return 0;
}
