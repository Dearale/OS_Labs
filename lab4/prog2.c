#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dlfcn.h>  // dlopen, dlsym, dlclose, RTLD_*
#include <unistd.h> // write

#define sizeof_array(a) (sizeof(a) / sizeof((a)[0]))

typedef float (*cos_derivative_func)(float a, float dx);
typedef float (*area_func)(float a, float b);

static float func_impl_stub(float x, float y) {
    (void)x;
    (void)y;
    return 0.0f;
}

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

typedef struct {
    char *path;
    void *handle;
    cos_derivative_func cos_derivative;
    area_func area;
    char *description;
} Library;

int load_library(Library *lib) {
    lib->handle = dlopen(lib->path, RTLD_NOW | RTLD_LOCAL);
    if (!lib->handle) {
        print_error("error: failed to load library\n");
        return -1;
    }

    lib->cos_derivative =
        (cos_derivative_func)dlsym(lib->handle, "cos_derivative");
    if (!lib->cos_derivative) {
        print_error(
            "warning: failed to find cos_derivative function implementation\n");
        lib->cos_derivative = func_impl_stub;
    }
    lib->area = (area_func)dlsym(lib->handle, "area");
    if (!lib->area) {
        print_error("warning: failed to find area function implementation\n");
        lib->area = func_impl_stub;
    }

    return 0;
}

void print_current_library_info(int idx, const Library *lib) {
    printf("Текущая реализация: %d (%s)\n", idx + 1, lib->path);
    printf("%s\n", lib->description);
    printf("\n");
}

int populate_and_load_libs(Library *libs) {
    libs[0].path = "./library_1.so";
    libs[0].handle = NULL;
    libs[0].cos_derivative = NULL;
    libs[0].area = NULL;
    libs[0].description =
        "  cos_derivative(a, dx) = (cos(a+dx) - cos(a)) / dx\n"
        "  area(a, b) - площадь прямоугольника";
    libs[1].path = "./library_2.so";
    libs[1].handle = NULL;
    libs[1].cos_derivative = NULL;
    libs[1].area = NULL;
    libs[1].description =
        "  cos_derivative(a, dx) = (cos(a+dx) - cos(a-dx)) / (2*dx)\n"
        "  area(a, b) - площадь прямоугольного треугольника";

    for (size_t i = 0; i < 2; i++) {
        if (load_library(&libs[i]) != 0) {
            print_error("error: failed to load one of the libraries\n");
            for (size_t j = 0; j < i; j++) {
                if (libs[j].handle) {
                    dlclose(libs[j].handle);
                }
            }
            return -1;
        }
    }
    return 0;
}

int main() {
    Library libs[2];
    if (populate_and_load_libs(libs) != 0) {
        return -1;
    }

    int current = 0;

    print(
        "Программа 2 (динамическая загрузка библиотек во время выполнения)\n");
    print_current_library_info(current, &libs[current]);
    print("Формат ввода:\n");
    print("  0       -> переключить реализацию контрактов (между 1 и 2)\n");
    print("  1 a dx  -> вычислить производную cos(x) в точке a с шагом dx\n");
    print("  2 a b   -> вычислить площадь фигуры по сторонам a и b\n");
    print("EOF (Ctrl+D) для выхода.\n\n");

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

        if (cmd == 0) {
            current = (current + 1) % (int)sizeof_array(libs);
            print_current_library_info(current, &libs[current]);
        } else if (cmd == 1) {
            float a;
            if (!read_float(&token, "a", &endptr, &a)) {
                continue;
            }
            float dx;
            if (!read_float(&token, "dx", &endptr, &dx)) {
                continue;
            }

            float result = libs[current].cos_derivative(a, dx);
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

            float result = libs[current].area(a, b);
            char output[256];
            int len = snprintf(output, sizeof(output), "area(%f, %f) = %f\n", a,
                               b, result);
            write(STDOUT_FILENO, output, len);
        } else {
            print_error("error: unknown command\n");
        }
    }

    for (size_t i = 0; i < sizeof_array(libs); i++) {
        dlclose(libs[i].handle);
    }
}