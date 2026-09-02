#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_NAME 256
#define SUBKEY_SIZE 8

pid_t child_pid = 0;

void handle_sigint(int sig) {
    printf("DEBUG: Received SIGINT\n");
    if (child_pid > 0) {
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
    }
    exit(0);
}

void handle_sigquit(int sig) {
    printf("DEBUG: Received SIGQUIT\n");
    if (child_pid > 0) {
        kill(child_pid, SIGTSTP);
        printf("Process %d hidden\n", child_pid);
        exit(0); // Exit immediately after suspending
    }
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <command> <proc_hash> <subkey>\n", argv[0]);
        return 1;
    }

    char* command = argv[1];
    char* proc_hash = argv[2];
    char subkey[SUBKEY_SIZE + 1];
    strncpy(subkey, argv[3], SUBKEY_SIZE);
    subkey[SUBKEY_SIZE] = '\0';

    struct sigaction sa_int, sa_quit;
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    sa_quit.sa_handler = handle_sigquit;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    sigaction(SIGQUIT, &sa_quit, NULL);

    child_pid = fork();
    if (child_pid == 0) {
        // Child process
        execl(command, command, (char*)NULL);
        perror("execl failed");
        exit(1);
    } else if (child_pid > 0) {
        // Parent process: Record process info
        char proc_file[MAX_NAME];
        snprintf(proc_file, MAX_NAME, "user.%s/procman.%s.txt", subkey, subkey);
        
        FILE* fp = fopen(proc_file, "a");
        if (fp) {
            fprintf(fp, "%s %d %s\n", proc_hash, child_pid, command);
            fclose(fp);
        }

        printf("DEBUG: Launched child PID %d\n", child_pid);

        // Manual input for testing
        char input[256];
        while (1) {
            printf("Type 'hide' to suspend, or wait for signals: ");
            fflush(stdout);
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                if (strcmp(input, "hide") == 0) {
                    kill(child_pid, SIGCONT); //  //SIGTSTP = pause
                    printf("Process %d hidden (manual trigger)\n", child_pid);
                    break; // Break and exit, don’t wait
                }
            }
        }

        // Don’t wait(NULL) here; exit immediately after suspension
        // Cleanup only happens on SIGINT (termination)
        return 0;
    } else {
        perror("fork failed");
        return 1;
    }

    return 0;
}
