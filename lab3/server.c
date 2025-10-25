#include <fcntl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/fcntl.h>
#include <wait.h>
#include <semaphore.h>
#include <sys/mman.h>

#define SHM_SIZE 4096

char * get_shm_buf(char *SHM_NAME, int *shm_out) {
    shm_unlink(SHM_NAME);
	int shm = shm_open(SHM_NAME, O_RDWR, 0600);
	if (shm == -1 && errno != ENOENT) {
		const char msg[] = "error: failed to open SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
	
	shm = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
	if (shm == -1) {
		const char msg[] = "error: failed to create SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

	if (ftruncate(shm, SHM_SIZE) == -1) {
		const char msg[] = "error: failed to resize SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}

	char *shm_buf = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm, 0);
	if (shm_buf == MAP_FAILED) {
		const char msg[] = "error: failed to map SHM\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
    uint32_t *length = (uint32_t *)shm_buf;
    *length = 0;

    *shm_out = shm;
    return shm_buf;
}

sem_t *open_semaphore(char * SEM_NAME) {
	sem_t *sem = sem_open(SEM_NAME, O_RDWR | O_CREAT | O_TRUNC, 0600, 0);
	if (sem == SEM_FAILED) {
		const char msg[] = "error: failed to create semaphore\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
	return sem;
}

int main(int argc, char **argv)
{
    if (argc == 1) {
		char msg[1024];
		uint32_t len = snprintf(msg, sizeof(msg) - 1, "usage: %s filename\n", argv[0]);
		write(STDERR_FILENO, msg, len);
		exit(EXIT_SUCCESS);
	}

    char SHM_S2C_NAME[64];
    char SHM_C2S_NAME[64];
    char SEM_S2C_NAME[64];
    char SEM_C2S_NAME[64];

    snprintf(SHM_S2C_NAME, sizeof(SHM_S2C_NAME), "example-shm-s2c-%d", getpid());
    snprintf(SHM_C2S_NAME, sizeof(SHM_C2S_NAME), "example-shm-c2s-%d", getpid());
    snprintf(SEM_S2C_NAME, sizeof(SEM_S2C_NAME), "example-sem-s2c-%d", getpid());
    snprintf(SEM_C2S_NAME, sizeof(SEM_C2S_NAME), "example-sem-c2s-%d", getpid());

    sem_unlink(SEM_S2C_NAME);
    sem_unlink(SEM_C2S_NAME);
    
    int shm_s2c;
    int shm_c2s;
    char * shm_s2c_buf = get_shm_buf(SHM_S2C_NAME, &shm_s2c);
    char * shm_c2s_buf = get_shm_buf(SHM_C2S_NAME, &shm_c2s);
	

	sem_t *sem_s2c = open_semaphore(SEM_S2C_NAME);
	sem_t *sem_c2s = open_semaphore(SEM_C2S_NAME);

	pid_t client = fork();
    
	if (client == 0) {
		char *args[] = {"client.out", SHM_S2C_NAME, SHM_C2S_NAME, SEM_S2C_NAME, SEM_C2S_NAME, argv[1], NULL};
		execv("./client.out", args);

		const char msg[] = "error: failed to exec\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	} else if (client == -1) {
		const char msg[] = "error: failed to fork\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		_exit(EXIT_FAILURE);
	}
	bool running = true;
	while (running) {
        char buf[SHM_SIZE - sizeof(uint32_t)];
		ssize_t bytes = read(STDIN_FILENO, buf, sizeof(buf));

		if (bytes == -1) {
			const char msg[] = "error: failed to read from standard input\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			_exit(EXIT_FAILURE);
		}
		uint32_t *length = (uint32_t *)shm_s2c_buf;
		char *text = shm_s2c_buf + sizeof(uint32_t);

        if (buf[0] == '\n' || bytes <= 0) {
            running = false;
			*length = UINT32_MAX;
            sem_post(sem_s2c);
            break;
        } else {
			memset(text, 0, SHM_SIZE - sizeof(uint32_t));
            *length = bytes;
			memcpy(text, buf, bytes);
            text[bytes] = '\0';
		}
		sem_post(sem_s2c);
        
        sem_wait(sem_c2s);
        uint32_t *length_client = (uint32_t *)shm_c2s_buf;
		char *text_client = shm_c2s_buf + sizeof(uint32_t);
        if (*length_client == UINT32_MAX) { // division by zero
            running = false;
            sem_post(sem_c2s);
            break;
        } else if (*length_client > 0) {
            write(STDOUT_FILENO, text_client, *length_client);
        }
		*length_client = 0;
	}

	waitpid(client, NULL, 0);

	sem_unlink(SEM_S2C_NAME);
	sem_unlink(SEM_C2S_NAME);
	sem_close(sem_s2c);
	sem_close(sem_c2s);
	munmap(shm_s2c_buf, SHM_SIZE);
	munmap(shm_c2s_buf, SHM_SIZE);
	shm_unlink(SHM_C2S_NAME);
	shm_unlink(SHM_S2C_NAME);
	close(shm_s2c);
	close(shm_c2s);
}