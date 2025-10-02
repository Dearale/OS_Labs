#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <math.h>

void divideNumbers(char * input, int32_t file) {
	char buf[4096];
	strncpy(buf, input, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	char * saveptr;
	char * token = strtok_r(buf, " ", &saveptr);
	float numbers[100];
	int count = 0;

	while (token != NULL && count < 100) {
		numbers[count++] = strtof(token, NULL);
		token = strtok_r(NULL, " ", &saveptr);
	}

	if (count < 2) {
		const char msg[] = "error: need at least two numbers\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		return;
	}

	float dividend = numbers[0];

	for (int i = 1; i < count; i++) {
		if (fabs(numbers[i]) < 1e-6) {
			const char msg[] = "error: division by zero\n";
			write(STDERR_FILENO, msg, sizeof(msg));

			exit(EXIT_FAILURE);
		}

		float result = dividend / numbers[i];

		char resultStr[50];
		int len = snprintf(resultStr, sizeof(resultStr), "%f ", result);

		if (write(file, resultStr, len) == -1) {
			const char msg[] = "error: failed to write to file\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(EXIT_FAILURE);
		}
	}
	if (write(file, "\n", 1) == -1) {
			const char msg[] = "error: failed to write to file\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(EXIT_FAILURE);
		}
}

int main(int argc, char **argv) {
	char buf[4096];
	ssize_t bytes;

	pid_t pid = getpid();

	// NOTE: `O_WRONLY` only enables file for writing
	// NOTE: `O_CREAT` creates the requested file if absent
	// NOTE: `O_TRUNC` empties the file prior to opening
	// NOTE: `O_APPEND` subsequent writes are being appended instead of overwritten
	int32_t file = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
	if (file == -1) {
		const char msg[] = "error: failed to open requested file\n";
		write(STDERR_FILENO, msg, sizeof(msg));
		exit(EXIT_FAILURE);
	}

	while (bytes = read(STDIN_FILENO, buf, sizeof(buf))) {
		if (bytes < 0) {
			const char msg[] = "error: failed to read from stdin\n";
			write(STDERR_FILENO, msg, sizeof(msg));
			exit(EXIT_FAILURE);
		}

		buf[bytes] = '\0';
		divideNumbers(buf, file);

		{
			{
				const char msg[] = "Server received: ";
				write(STDERR_FILENO, msg, sizeof(msg) - 1);
			}

			// NOTE: Echo back to client
			int32_t written = write(STDOUT_FILENO, buf, bytes);
			if (written != bytes) {
				const char msg[] = "error: failed to echo\n";
				write(STDERR_FILENO, msg, sizeof(msg));
				exit(EXIT_FAILURE);
			}
		}
	}

	close(file);
}