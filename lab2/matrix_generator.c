#include <stdlib.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage: %s <n> <output.txt> <solution.txt>\n", argv[0]);
        return 1;
    }

    int n = atoi(argv[1]);
    FILE *f = fopen(argv[2], "w");
    if (!f) { perror("fopen"); return 2; }

    FILE *fsol = fopen(argv[3], "w");
    if (!fsol) { perror("fopen"); return 2; }

    srand(time(NULL));

    double *A = malloc(n * n * sizeof(double));
    double *x = malloc(n * sizeof(double));
    double *b = calloc(n, sizeof(double));

    for (int i = 0; i < n; i++) {
        double rowsum = 0.0;
        for (int j = 0; j < n; j++) {
            if (i == j) continue;
            double val = (rand() % 100) / 10.0;
            A[i * n + j] = val;
            rowsum += val;
        }
        A[i * n + i] = rowsum + 10.0;
    }

    fprintf(fsol, "generated solution: \n");
    for (int i = 0; i < n; i++)
    {
        x[i] = (rand() % 100) / 10.0;
        fprintf(fsol, "%lf ", x[i]);

        if ((i + 1) % 5 == 0) {
            fprintf(fsol, "\n");
        }
    }


    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            b[i] += A[i * n + j] * x[j];

    fprintf(f, "%d %d\n", n, n);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++)
            fprintf(f, "%10.4f ", A[i * n + j]);
        fprintf(f, " %10.4f\n", b[i]);
    }

    fclose(f);
    fclose(fsol);
    free(A); free(x); free(b);
    return 0;
}
