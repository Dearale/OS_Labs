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

#define EPSILON 1e-9

void solveWithGauss(double** matrix, int rows, int cols, double *b, double * outSolution) {
    double ** merged = (double **)malloc(sizeof(double *) * rows);
    cols++;
    for (size_t i = 0; i < rows; i++)
    {
        merged[i] = (double *)malloc(sizeof(double) * cols);
    }

    for (size_t row = 0; row < rows; row++)
    {
        for (size_t col = 0; col < cols - 1; col++)
        {
            merged[row][col] = matrix[row][col];
        }
    }

    for (size_t row = 0; row < rows; row++)
    {
        merged[row][cols - 1] = b[row];
    }
    
    size_t rowI = 0;
    size_t iterations = rows < cols ? rows : cols;
    for (size_t i = 0; i < iterations; i++)
    {
        double * pivot = NULL;
        for (size_t row = rowI; row < rows; row++)
        {
            if (fabs(merged[row][i]) >= EPSILON) {
                pivot = merged[row];
                double * tmp = merged[rowI];
                merged[rowI] = pivot;
                merged[row] = tmp;
                break;
            }
        }
        
        if (pivot == NULL) {
            continue;
        }

        for (size_t row = rowI + 1; row < rows; row++)
        {
            if (fabs(merged[row][i]) < EPSILON) {
                merged[row][i] = 0;
                continue;
            }
            double toSubtract = merged[row][i] / pivot[i];
            merged[row][i] = 0;
            for (size_t col = i + 1; col < cols; col++)
            {
                merged[row][col] -= pivot[col] * toSubtract;
            }
            
        }
        rowI++;
    }

    if (rowI == 0) {
        rowI = 1;
    }
    for (int i = rowI - 1; i >= 0; i--)
    {
        for (size_t col = 0; col < cols; col++)
        {
            if (fabs(merged[i][col]) >= EPSILON) {
                if (col == cols - 1) {
                    return; // NO SOLUTION
                }

                merged[i][cols - 1] /= merged[i][col];
                merged[i][col] = 1;

                for (size_t row = 0; row < i; row++)
                {
                    merged[row][cols - 1] -= merged[row][col] * merged[i][cols - 1];
                    merged[row][col] = 0;
                }
                break;
            }
        }
    }

    size_t curRow = 0;
    for (size_t i = 0; i < cols; i++)
    {
        if (curRow < rows) {
            if (merged[curRow][i] == 0)
                continue;
            
            outSolution[i] = merged[curRow][cols - 1];
            curRow++;
        } else {
            outSolution[i] = 0;
        }
    }


}


void printUsage() {
    const char msg[] = "Использование: ./main.out <входной файл> <файл для вывода решения>\n";
    write(STDERR_FILENO, msg, strlen(msg));
}


void quit(const char *msg) {
    write(STDERR_FILENO, msg, strlen(msg));
    exit(EXIT_FAILURE);
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printUsage();
        exit(EXIT_FAILURE);
    }

    int32_t file = open(argv[1], O_RDONLY);
	if (file == -1) {
		quit("Ошибка: не удалось открыть входной файл\n");
	}

    
    int32_t fileSol = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
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

    char *endptr;
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
    solveWithGauss(matrix, rows, cols, b, solution);
    
    const char msg[] = "solution:\n";
    write(fileSol, msg, strlen(msg));
    if (solution == NULL) {
        printf("No solution\n");
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
    
    close(fileSol);
}