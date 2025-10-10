#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>


#define EPSILON 1e-9
#define MAX_THREAD_NUM 5000


static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
    pthread_mutex_t mutex; // Мьютекс для защиты данных барьера
    pthread_cond_t cond;   // Условная переменная для ожидания
    int count;             // Текущий счётчик достигших потоков
    int total;             // Общее количество потоков для барьера
} Barrier;

typedef struct {
    double **matrix;
    int rows;
    int cols;
    double *b;
    double *solution;
    int threadNum;
    int threadID;
    double **mergedMatrix;
    bool *pivotIsFound;
    int *colForReverse;
    Barrier *barrier;
} ThreadArgs;



void barrier_wait(Barrier *barrier) {
    pthread_mutex_lock(&barrier->mutex);
    barrier->count++;
    
    if (barrier->count < barrier->total) {
        // Поток ждёт, пока все не достигнут барьера
        pthread_cond_wait(&barrier->cond, &barrier->mutex);
    } else {
        // Последний поток уведомляет все ждущие
        barrier->count = 0; // Сбрасываем счётчик для следующего использования
        pthread_cond_broadcast(&barrier->cond);
    }
    
    pthread_mutex_unlock(&barrier->mutex);
}


void *solveWithGauss(void * _args) {
    ThreadArgs *args = (ThreadArgs *)_args;
    double **matrix = args->matrix;
    int rows = args->rows;
    int cols = args->cols;
    double *b = args->b;
    double *solution = args->solution;
    double **merged = args->mergedMatrix;
    cols++;

    size_t rowI = 0;
    size_t iterations = rows < cols ? rows : cols;
    for (size_t i = 0; i < iterations; i++)
    {
        barrier_wait(args->barrier);

        pthread_mutex_lock(&mutex);
        
        if (args->threadID == 0) {
            *(args->pivotIsFound) = false;
            for (size_t row = rowI; row < rows; row++)
            {
                if (fabs(merged[row][i]) >= EPSILON) {
                    double * tmp = merged[rowI];
                    merged[rowI] = merged[row];
                    merged[row] = tmp;
                    *(args->pivotIsFound) = true;
                    break;
                }
            }
        }
        
        pthread_mutex_unlock(&mutex);
        barrier_wait(args->barrier);

        if (!(*(args->pivotIsFound))) {
            continue;
        }

        for (size_t row = rowI + 1 + args->threadID; row < rows; row += args->threadNum)
        {
            if (fabs(merged[row][i]) < EPSILON) {
                merged[row][i] = 0;
                continue;
            }
            double toSubtract = merged[row][i] / merged[rowI][i];
            merged[row][i] = 0;
            for (size_t col = i + 1; col < cols; col++)
            {
                merged[row][col] -= merged[rowI][col] * toSubtract;
            }
            
        }
        rowI++;

        barrier_wait(args->barrier);
    }

    if (rowI == 0) {
        rowI = 1;
    }
    for (int i = rowI - 1; i >= 0; i--)
    {
        pthread_mutex_lock(&mutex);

        if (args->threadID == 0) {
            for (size_t col = 0; col < cols; col++)
            {
                if (fabs(merged[i][col]) >= EPSILON) {
                    *(args->colForReverse) = col;

                    break;
                }
            }
        }

        pthread_mutex_unlock(&mutex);

        barrier_wait(args->barrier);

        int col = *(args->colForReverse);
        if (col == cols - 1) {
            return NULL; // NO SOLUTION
        }

        pthread_mutex_lock(&mutex);
        if (args->threadID == 0) {
            merged[i][cols - 1] /= merged[i][col];
            merged[i][col] = 1;
        }
        pthread_mutex_unlock(&mutex);

        barrier_wait(args->barrier);

        for (size_t row = args->threadID; row < i; row += args->threadNum)
        {
            merged[row][cols - 1] -= merged[row][col] * merged[i][cols - 1];
            merged[row][col] = 0;
        }

        barrier_wait(args->barrier);
    }
    
    for (size_t i = args->threadID; i < cols - 1; i += args->threadNum)
    {
        bool foundRow = false;
        for (int row = i; row >= 0; row--)
        {
            if (merged[row][i] != 0) {
                solution[i] = merged[row][cols - 1];
                foundRow = true;
                break;
            }
        }

        if (!foundRow) {
            solution[i] = 0;
        }
    }
}

void initMergedMatrix(ThreadArgs *args) {
    int rows = args->rows;
    int cols = args->cols;
    double **matrix = args->matrix;
    double *b = args->b;

    args->mergedMatrix = (double **)malloc(sizeof(double *) * rows);
    cols++;

    for (size_t i = 0; i < rows; i++)
    {
        args->mergedMatrix[i] = (double *)malloc(sizeof(double) * cols);
    }

    for (size_t row = 0; row < rows; row++)
    {
        for (size_t col = 0; col < cols - 1; col++)
        {
            args->mergedMatrix[row][col] = matrix[row][col];
        }
    }

    
    for (size_t row = 0; row < rows; row++)
    {
        args->mergedMatrix[row][cols - 1] = b[row];
    }
}

double solve(ThreadArgs *args) {
    double **matrix = args->matrix;
    int rows = args->rows;
    int cols = args->cols;
    double *b = args->b;
    double *solution = args->solution;
    int threadNum = args->threadNum;
    initMergedMatrix(args);
    double **mergedMatrix = args->mergedMatrix;
    
    bool pivotIsFound = false;
    int colForReverse = -1;

    pthread_t *threads = (pthread_t *)malloc(threadNum * sizeof(pthread_t));
    ThreadArgs *threadArgs = (ThreadArgs *)malloc(threadNum * sizeof(ThreadArgs));
    Barrier barrier;
    pthread_mutex_init(&barrier.mutex, NULL);
    pthread_cond_init(&barrier.cond, NULL);
    barrier.count = 0;
    barrier.total = threadNum;

    struct timeval start, end;

    gettimeofday(&start, NULL);
    for (size_t i = 0; i < threadNum; i++)
    {
        threadArgs[i] = (ThreadArgs) {
            .matrix = matrix,
            .rows = rows,
            .cols = cols,
            .b = b,
            .solution = solution,
            .threadNum = threadNum,
            .threadID = i,
            .mergedMatrix = mergedMatrix,
            .pivotIsFound = &pivotIsFound,
            .colForReverse = &colForReverse,
            .barrier = &barrier,
        };

        pthread_create(&threads[i], NULL, solveWithGauss, &threadArgs[i]);
    }

    for (size_t i = 0; i < threadNum; i++)
    {
        pthread_join(threads[i], NULL);
    }

    gettimeofday(&end, NULL);

    pthread_mutex_destroy(&barrier.mutex);
    pthread_cond_destroy(&barrier.cond);
    free(threadArgs);
    free(threads);
    for (size_t k = 0; k < rows; k++)
    {
        free(mergedMatrix[k]);
    }
    free(mergedMatrix);
    double elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1e6;

    return elapsed;
}

void printUsage() {
    const char msg[] = "Использование: ./main.out <количество потоков> <входной файл> <файл для вывода решения>\n";
    write(STDERR_FILENO, msg, strlen(msg));
}

void writeToConsole(const char *msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}


void quit(const char *msg) {
    write(STDERR_FILENO, msg, strlen(msg));
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    if (argc != 4) {
        printUsage();
        exit(EXIT_FAILURE);
    }

    char *endptr;
    long _threadNum = strtol(argv[1], &endptr, 10);

    if (*endptr != '\0') {
        printUsage();
        exit(EXIT_FAILURE);
    }

    if (_threadNum < 1 || _threadNum > MAX_THREAD_NUM) {
        quit("Количество потоков должно быть в диапазоне от 1 до 5000 включительно\n");
    }

    int threadNum = (int)_threadNum;

    int32_t file = open(argv[2], O_RDONLY);
	if (file == -1) {
		quit("Ошибка: не удалось открыть входной файл\n");
	}

    
    int32_t fileSol = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
	if (fileSol == -1) {
		quit("Ошибка: не удалось открыть файл для вывода решения\n");
	}

    struct stat st;
    fstat(file, &st);

    off_t fsize = st.st_size;


    char *buffer = malloc((size_t)fsize + 1);

    size_t offset = 0;
    while (offset < fsize) {
        ssize_t bytes_read = read(file, buffer + offset, fsize - offset);
        if (bytes_read < 0) {
            close(file);
            free(buffer);
            quit("Ошибка: не удалось прочитать входной файл\n");
        }
        if (bytes_read == 0) break;
        offset += (size_t)bytes_read;
    }
    close(file);
    buffer[offset] = '\0';

    long _rows = strtol(buffer, &endptr, 10);

    long _cols = strtol(endptr, &endptr, 10);

    int rows = (int)_rows;
    int cols = (int)_cols;
    double ** matrix = (double **)malloc(sizeof(double *) * rows);
    for (size_t i = 0; i < rows; i++)
    {
        matrix[i] = (double *)malloc(sizeof(double) * cols);
    }

    double *b = (double *)malloc(sizeof(double) * rows);
    double *solution = (double *)malloc(sizeof(double) * cols);

    char * p = endptr;
    for (int i = 0; i < rows; i++)
    {
        for (int j = 0; j < cols + 1; j++)
        {
            double val = strtod(p, &endptr);
            if (p == endptr) {
                quit("Ошибка: недостаточно чисел в файле");
            }
            if (j == cols)
                b[i] = val;
            else                
                matrix[i][j] = val;
            p = endptr;
        }
    }
    ThreadArgs args;
    args.matrix = matrix;
    args.b = b;
    args.rows = rows;
    args.cols = cols;
    args.solution = solution;
    args.threadNum = threadNum;
    double time = solve(&args);
    
    const char msg[] = "solution:\n";
    write(fileSol, msg, strlen(msg));
    if (solution == NULL) {
        writeToConsole("No solution\n");
    } else {
        char buf[64];

        for (size_t i = 0; i < cols; i++)
        {
            int len = snprintf(buf, sizeof(buf), "%lf ", solution[i]);
            write(fileSol, buf, len);

            if ((i + 1) % 5 == 0) {
                write(fileSol, "\n", 1);
            }
        }
    }

    char buf[1024];
    int len = snprintf(buf, sizeof(buf), "Количество потоков: %d\n", threadNum);
    write(STDOUT_FILENO, buf, len);

    len = snprintf(buf, sizeof(buf), "Размер массива: %dx%d\n", rows, cols);
    write(STDOUT_FILENO, buf, len);
    
    len = snprintf(buf, sizeof(buf), "Затраченное время: %lfс\n", time);
    write(STDOUT_FILENO, buf, len);

    close(fileSol);
}