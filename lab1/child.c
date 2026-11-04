#include <unistd.h>
#include <fcntl.h>

float parse_float(const char* str, int start, int end) {
    float res = 0.0f;
    float sign = 1.0f;
    int i = start;

    if (str[i] == '-') {
        sign = -1.0f;
        i++;
    } else if (str[i] == '+') {
        i++;
    }

    while (i < end && str[i] >= '0' && str[i] <= '9') {
        res = res * 10.0f + (str[i] - '0');
        i++;
    }

    if (i < end && str[i] == '.') {
        i++;
        float depth = 0.1f;
        while (i < end && str[i] >= '0' && str[i] <= '9') {
            res += (str[i] - '0') * depth;
            depth *= 0.1f;
            i++;
        }
    }
    return res * sign;
}

int float_to_string(float num, char* buf) {
    int idx = 0;
    if (num < 0) {
        buf[idx++] = '-';
        num = -num;
    }

    int int_part = (int)num;
    float frac_part = num - (float)int_part;

    char temp[20];
    int t_idx = 0;
    if (int_part == 0) {
        temp[t_idx++] = '0';
    } else {
        while (int_part > 0) {
            temp[t_idx++] = (int_part % 10) + '0';
            int_part /= 10;
        }
    }

    for (int i = t_idx - 1; i >= 0; i--) {
        buf[idx++] = temp[i];
    }

    buf[idx++] = '.';
    for (int i = 0; i < 4; i++) {
        frac_part *= 10;
        int digit = (int)frac_part;
        buf[idx++] = digit + '0';
        frac_part -= digit;
    }

    buf[idx++] = '\n';
    return idx;
}

int main(int argc, char* argv[]) {
    if (argc < 2) return 1;

    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) return 1;

    char buffer[4096];
    int bytes_read;

    while ((bytes_read = read(0, buffer, sizeof(buffer))) > 0) {
        int line_start = 0;
        for (int i = 0; i < bytes_read; i++) {
            if (buffer[i] == '\n') {
                float sum = 0.0f;
                int token_start = -1;

                for (int j = line_start; j <= i; j++) {
                    if (buffer[j] == ' ' || buffer[j] == '\n') {
                        if (token_start != -1) {
                            sum += parse_float(buffer, token_start, j);
                            token_start = -1;
                        }
                    } else {
                        if (token_start == -1) token_start = j;
                    }
                }
                char out_buf[32];
                int out_len = float_to_string(sum, out_buf);
                write(file_fd, out_buf, out_len);
                line_start = i + 1;
            }
        }
    }
    close(file_fd);
    return 0;
}