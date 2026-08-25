#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define MAX_THREADS 10

struct termios original_term; // Store original terminal settings

void save_term() {
    tcgetattr(0, &original_term); // Save terminal settings
}

void restore_term() {
    tcsetattr(0, TCSANOW, &original_term); // Restore terminal settings
}

// Thread function that executes the system command
void* thread_func(void* data) {
    char* command = (char*)data;
    save_term(); // Save terminal settings before running command
    system(command);
    restore_term(); // Restore terminal settings after command
    free(command);  // Free the duplicated string
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
    // Check if we have enough arguments
    if (argc < 2) {
        printf("Usage: %s <command1> [command2] [command3] ...\n", argv[0]);
        printf("Up to %d commands can be specified\n", MAX_THREADS);
        return 1;
    }

    // Truncate or create the output file
    FILE* fp = fopen("gl_cli_out.txt", "w");
    if (fp) {
        fclose(fp);
    } else {
        perror("Failed to open gl_cli_out.txt");
        exit(1);
    }

    // Calculate number of threads (limited to MAX_THREADS)
    int num_threads = argc - 1;  // Subtract 1 for program name
    if (num_threads > MAX_THREADS) {
        num_threads = MAX_THREADS;
        printf("Warning: Limited to %d threads. Additional arguments ignored.\n", MAX_THREADS);
    }

    // Array to store thread IDs and command strings
    pthread_t threads[MAX_THREADS];
    char* thread_commands[MAX_THREADS];

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        // Create command string with redirection using tee -a
        char* original_cmd = argv[i + 1];
        const char* redirect = " 2>&1 | tee -a gl_cli_out.txt";
        size_t len = strlen(original_cmd) + strlen(redirect) + 1;
        thread_commands[i] = malloc(len);
        if (!thread_commands[i]) {
            perror("Failed to allocate memory for command string");
            exit(1);
        }
        snprintf(thread_commands[i], len, "%s%s", original_cmd, redirect);

        // Create thread
        if (pthread_create(&threads[i], NULL, thread_func, thread_commands[i])) {
            perror("Failed to create thread");
            free(thread_commands[i]);
            exit(1);
        }
    }

    // Wait for all threads to complete
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
