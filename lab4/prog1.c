#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void print(char msg[]) { write(STDOUT_FILENO, msg, strlen(msg)); }

void print_error(char msg[]) { write(STDERR_FILENO, msg, strlen(msg)); }

int read_float(char **token, char *arg_name, char **endptr, float *out_value) {
    *token = strtok(NULL, " ");
    if (*token == NULL) {
        char output[256];
        snprintf(output, sizeof(output), "error: missing argument '%s'\n",
                 arg_name);
        print_error(output);
        return 0;
    }
    float value = strtof(*token, endptr);
    if (*endptr == *token) {
        char output[256];
        snprintf(output, sizeof(output), "error: invalid argument '%s'\n",
                 arg_name);
        print_error(output);
        return 0;
    }
    *out_value = value;
    return 1;
}

int main() {
    print("Программа 1 (линковка на этапе компиляции)\n");
    print("Используется реализация 1:\n");
    print("  cos_derivative(a, dx) = (cos(a+dx) - cos(a)) / dx\n");
    print("  area(a, b) - площадь прямоугольника\n\n");
    print("Формат ввода:\n");
    print("  1 a dx  -> вычислить производную cos(x) в точке a с шагом dx\n");
    print("  2 a b   -> вычислить площадь фигуры по сторонам a и b\n");
    print("Ctrl+D для выхода.\n\n");

    while (1) {
        char buffer[1024];
        ssize_t bytes = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        if (bytes == -1) {
            print_error("error: failed to read from standard input\n");
            return -1;
        }
        if (bytes == 0) {
            print("Завершение работы программы.\n");
            break;
        }

        buffer[bytes] = '\0';

        char *token;
        token = strtok(buffer, " ");

        char *endptr;
        int cmd = strtol(token, &endptr, 10);
        if (endptr == token) {
            print_error("error: invalid command\n");
            continue;
        }

        if (cmd == 1) {
            float a;
            if (!read_float(&token, "a", &endptr, &a)) {
                continue;
            }
            float dx;
            if (!read_float(&token, "dx", &endptr, &dx)) {
                continue;
            }

            float result = cos_derivative(a, dx);
            char output[256];
            int len = snprintf(output, sizeof(output),
                               "cos_derivative(%f, %f) = %f\n", a, dx, result);
            write(STDOUT_FILENO, output, len);
        } else if (cmd == 2) {
            float a;
			if (!read_float(&token, "a", &endptr, &a)) {
				continue;
			}

            float b;
            if (!read_float(&token, "b", &endptr, &b)) {
                continue;
			}

            float result = area(a, b);
            char output[256];
            int len = snprintf(output, sizeof(output), "area(%f, %f) = %f\n", a,
                               b, result);
            write(STDOUT_FILENO, output, len);
        } else {
            print_error("error: unknown command\n");
        }
    }
}