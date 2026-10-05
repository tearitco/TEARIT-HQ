#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#define MAX_THREADS 10
#define MAX_LINE 256

volatile sig_atomic_t keep_running = 1; // Flag to control thread termination
int killer_thread_indices[MAX_THREADS]; // Indices of threads running flagged (^) binaries
int num_killer_threads = 0; // Number of killer threads
char* commands[MAX_THREADS]; // Store commands for cleanup tracking

// Signal handler for main thread
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        fprintf(stderr, "Signal %d received in orchestrator (pid %d)\n", sig, getpid());
        keep_running = 0; // Signal threads to stop
    }
}

// Thread function that executes the system command
void* thread_func(void* data) {
    // Block SIGINT in this thread
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    char* command = (char*)data;
    int ret = system(command);
    fprintf(stderr, "Thread command '%s' exited with status %d (pid %d)\n", command, ret, getpid());
    // Do not free command here; defer to main thread to avoid double-free
    pthread_exit(NULL);
}

int main() {
    // Block SIGINT in main thread initially
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // Set up signal handler for SIGTERM, ignore SIGINT
    signal(SIGTERM, signal_handler);
    signal(SIGINT, SIG_IGN);

    printf("test1\n");

    // Truncate or create the output file
    FILE* fp = fopen("gl_cli_out.txt", "w");
    if (fp) {
        fclose(fp);
    } else {
        perror("Failed to open gl_cli_out.txt");
        return 1;
    }

    // Open and read locations.txt
    FILE* file = fopen("locations.txt", "r");
    if (!file) {
        perror("Failed to open locations.txt");
        return 1;
    }

    if (chdir("../") != 0) {
        perror("chdir failed");
        return 1;
    }

    char line[MAX_LINE];
    int num_threads = 0;

    // Initialize commands and killer_thread_indices arrays
    for (int i = 0; i < MAX_THREADS; i++) {
        commands[i] = NULL;
        killer_thread_indices[i] = -1;
    }

    // Read commands from file
    while (fgets(line, MAX_LINE, file) && num_threads < MAX_THREADS) {
        line[strcspn(line, "\n")] = 0;
        if (strlen(line) > 0 && line[0] != '#') {
            int is_killer = 0;
            char* command_part = line;
            if (line[0] == '^') {
                is_killer = 1;
                command_part = line + 1; // Skip the '^'
                while (*command_part == ' ') command_part++; // Skip spaces after '^'
            }
            char* redirect_flag = strtok(command_part, " \t");
            redirect_flag = strtok(NULL, " \t");
            const char* redirect;
            if (redirect_flag && strcmp(redirect_flag, "&") == 0) {
                redirect = " > /dev/null 2>&1";
            } else {
                redirect = " 2>&1 | tee -a gl_cli_out.txt";
            }

            size_t len = (redirect_flag && strcmp(redirect_flag, "&") == 0 ? 0 : strlen("stdbuf -oL ")) 
                       + strlen(command_part) + strlen(redirect) + 1;
            commands[num_threads] = malloc(len);
            if (!commands[num_threads]) {
                perror("Failed to allocate memory for command string");
                fclose(file);
                for (int i = 0; i < num_threads; i++) {
                    free(commands[i]);
                }
                return 1;
            }
            if (redirect_flag && strcmp(redirect_flag, "&") == 0) {
                snprintf(commands[num_threads], len, "%s%s", command_part, redirect);
            } else {
                snprintf(commands[num_threads], len, "stdbuf -oL %s%s", command_part, redirect);
            }
            // Track killer threads
            if (is_killer) {
                killer_thread_indices[num_killer_threads++] = num_threads;
            }
            num_threads++;
        }
    }
    fclose(file);

    if (num_threads == 0) {
        printf("No valid commands found in locations.txt\n");
        return 1;
    }

    // Array to store thread IDs
    pthread_t threads[MAX_THREADS];

    // Create threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&threads[i], NULL, thread_func, commands[i])) {
            perror("Failed to create thread");
            for (int j = 0; j < num_threads; j++) {
                free(commands[j]);
            }
            return 1;
        }
    }

    // Unblock SIGINT in main thread
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    // Wait for threads to complete or for SIGINT/SIGTERM
    while (keep_running) {
        int all_done = 1;
        for (int i = 0; i < num_threads; i++) {
            void* status;
            if (pthread_tryjoin_np(threads[i], &status) == 0) {
                // Thread has finished
                for (int j = 0; j < num_killer_threads; j++) {
                    if (i == killer_thread_indices[j]) {
                        // A killer thread has exited, terminate orchestrator
                        fprintf(stderr, "Killer thread (index %d, command %s) exited, shutting down orchestrator\n", i, commands[i]);
                        keep_running = 0;
                        break;
                    }
                }
                continue;
            }
            all_done = 0;
        }
        if (all_done) break;
        usleep(100000);
    }

    // Clean up: join any remaining threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_tryjoin_np(threads[i], NULL) != 0) {
            // Thread still running, attempt to join with timeout
            struct timespec timeout;
            clock_gettime(CLOCK_REALTIME, &timeout);
            timeout.tv_sec += 1; // Wait up to 1 second
            if (pthread_timedjoin_np(threads[i], NULL, &timeout) != 0) {
                fprintf(stderr, "Thread %d (command %s) did not terminate, forcing cleanup\n", i, commands[i]);
            }
        }
    }

    // Free command memory
    for (int i = 0; i < num_threads; i++) {
        if (commands[i]) {
            free(commands[i]);
            commands[i] = NULL;
        }
    }

    return 0;
}
