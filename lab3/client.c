#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <sys/mman.h>
#include <semaphore.h>

#define SHM_SIZE 4096

int divideNumbers(char * input, int32_t file) {
	char buf[4096];
	strncpy(buf, input, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';
	char * saveptr;
	char * token = strtok_r(buf, " ", &saveptr);
	
	float numbers[100];
	int count = 0;
	
	while (token != NULL && count < 100) {
		char *endptr;
		float val = strtof(token, &endptr);
		if (endptr != token) {
			numbers[count++] = val;
		}
		token = strtok_r(NULL, " ", &saveptr);
	}
	

	if (count < 2) {
		const char msg[] = "error: need at least two numbers\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		return 0;
	}

	float dividend = numbers[0];

	for (int i = 1; i < count; i++) {
		if (fabs(numbers[i]) < 1e-6) {
			const char msg[] = "error: division by zero\n";
			write(STDERR_FILENO, msg, sizeof(msg));

            return 1;
		}

		float result = dividend / numbers[i];

		char resultStr[50];
		int len = snprintf(resultStr, sizeof(resultStr), "%f ", result);

        if (write(file, resultStr, len) == -1) {
			const char msg[] = "error: failed to write to file\n";
			write(STDERR_FILENO, msg, sizeof(msg));
            return 1;
		}
	}
	if (write(file, "\n", 1) == -1) {
        const char msg[] = "error: failed to write to file\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        return 1;
    }
    return 0;
}

char * get_shm_buf(char *SHM_NAME, int *shm_out) {
    int shm = shm_open(SHM_NAME, O_RDWR, 0);
	if (shm == -1) {
		const char msg[] = "error: failed to open SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

	char *shm_buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
	if (shm_buf == MAP_FAILED) {
		const char msg[] = "error: failed to map SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
    *shm_out = shm;
    return shm_buf;
}

sem_t *open_semaphore(char *SEM_NAME) {
	sem_t *sem = sem_open(SEM_NAME, O_RDWR);
	if (sem == SEM_FAILED) {
		const char msg[] = "error: failed to open semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
	return sem;
}

int main(int argc, char *argv[])
{
    char *SHM_S2C_NAME = argv[1];
    char *SHM_C2S_NAME = argv[2];
    char *SEM_S2C_NAME = argv[3];
    char *SEM_C2S_NAME = argv[4];
    // NOTE: `O_WRONLY` only enables file for writing
	// NOTE: `O_CREAT` creates the requested file if absent
	// NOTE: `O_TRUNC` empties the file prior to opening
	// NOTE: `O_APPEND` subsequent writes are being appended instead of overwritten
	int32_t file = open(argv[5], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
	if (file == -1) {
		const char msg[] = "error: failed to open requested file\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	}

    int shm_s2c;
    int shm_c2s;
    char *shm_s2c_buf = get_shm_buf(SHM_S2C_NAME, &shm_s2c);
    char *shm_c2s_buf = get_shm_buf(SHM_C2S_NAME, &shm_c2s);

	sem_t *sem_s2c = open_semaphore(SEM_S2C_NAME);
	sem_t *sem_c2s = open_semaphore(SEM_C2S_NAME);

	bool running = true;
	while (running) {
		sem_wait(sem_s2c);
		uint32_t *length = (uint32_t *)shm_s2c_buf;
		char *text = shm_s2c_buf + sizeof(uint32_t);
		if (*length == UINT32_MAX) {
			running = false;
		} else if (*length > 0) {
            text[*length] = '\0';
            int status = divideNumbers(text, file);

            uint32_t *length_to_server = (uint32_t *)shm_c2s_buf;
		    char *text_to_server = shm_c2s_buf + sizeof(uint32_t);
            if (status != 0) {
                *length_to_server = UINT32_MAX;
                sem_post(sem_c2s);
                exit(EXIT_FAILURE);
            } else {
				memset(text_to_server, 0, SHM_SIZE - sizeof(uint32_t));
                *length_to_server = *length;
			    memcpy(text_to_server, text, *length);
            	text_to_server[*length] = '\0';
            }

            const char msg[] = "CLIENT RECEIVED: ";
            write(STDOUT_FILENO, msg, sizeof(msg) - 1);

            *length = 0;
		}

		sem_post(sem_c2s);
	}

	sem_close(sem_s2c);
	sem_close(sem_c2s);
	munmap(shm_s2c_buf, SHM_SIZE);
	munmap(shm_c2s_buf, SHM_SIZE);
	close(shm_s2c);
	close(shm_c2s);
	close(file);
}