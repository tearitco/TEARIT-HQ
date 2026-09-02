#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <errno.h>

#define MAX_NAME 256
#define SUBKEY_SIZE 8

void list_processes(const char* subkey) {
    char proc_file[MAX_NAME];
    snprintf(proc_file, MAX_NAME, "user.%s/procman.%s.txt", subkey, subkey);

    FILE* fp = fopen(proc_file, "r");
    if (!fp) {
        printf("No running processes found.\n");
        return;
    }

    char temp_file[MAX_NAME];
    snprintf(temp_file, MAX_NAME, "user.%s/procman.%s.tmp", subkey, subkey);
    FILE* temp = fopen(temp_file, "w");
    if (!temp) {
        fclose(fp);
        printf("Failed to create temp file for cleanup.\n");
        return;
    }

    char line[MAX_NAME * 2];
    int found_alive = 0;
    printf("\nRunning Processes:\n");
    while (fgets(line, sizeof(line), fp) != NULL) {
        char proc_hash[9];
        pid_t pid;
        char command[MAX_NAME * 2];
        if (sscanf(line, "%8s %d %[^\n]", proc_hash, &pid, command) == 3) {
            // Check if the process is still alive
            if (kill(pid, 0) == 0) {
                printf("PID: %d | Hash: %s | Command: %s\n", pid, proc_hash, command);
                fprintf(temp, "%s", line); // Keep alive processes in temp file
                found_alive = 1;
            } else if (errno == ESRCH) {
                printf("DEBUG: PID %d is dead, removing from list\n", pid);
                // Skip writing dead process to temp file
            } else {
                perror("Error checking process status");
            }
        }
    }
    fclose(fp);
    fclose(temp);

    // Replace original file with updated one
    if (found_alive) {
        rename(temp_file, proc_file);
    } else {
        remove(proc_file); // No alive processes, delete the file
        remove(temp_file);
    }

    if (!found_alive) {
        printf("No running processes found after cleanup.\n");
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <subkey>\n", argv[0]);
        return 1;
    }

    char subkey[SUBKEY_SIZE + 1];
    strncpy(subkey, argv[1], SUBKEY_SIZE);
    subkey[SUBKEY_SIZE] = '\0';

    while (1) {
        printf("\nThread Manager\n");
        list_processes(subkey);
        printf("Options: 'f <pid>' to foreground, 'b <pid>' to background, 'q' to quit: ");
        fflush(stdout);

        char input[20];
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "q") == 0) {
            break;
        }

        char action[2];
        pid_t pid;
        if (sscanf(input, "%1s %d", action, &pid) == 2) {
            if (action[0] == 'f') {
                if (kill(pid, SIGCONT) == 0) {
                    printf("Brought PID %d to foreground\n", pid);
                    tcsetpgrp(STDIN_FILENO, getpgid(pid));
                    waitpid(pid, NULL, WUNTRACED);
                    tcsetpgrp(STDIN_FILENO, getpgrp());
                } else {
                    perror("Failed to bring to foreground");
                }
            } else if (action[0] == 'b') {
                if (kill(pid, SIGCONT) == 0) { //SIGTSTP = pause
                    printf("Sent PID %d to background\n", pid);
                } else {
                    perror("Failed to send to background");
                }
            }
        } else {
            printf("Invalid command. Use 'f <pid>', 'b <pid>', or 'q'\n");
        }
    }

    return 0;
}
