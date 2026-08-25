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
int pipe_fd[2] = {-1, -1};
int tty_fd = -1;
int show_output = 0;

void handle_sigint(int sig) {
    printf("DEBUG: Received SIGINT\n");
    if (child_pid > 0) {
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        printf("Child process %d terminated.\n", child_pid);
    }
    if (pipe_fd[0] >= 0) close(pipe_fd[0]);
    if (pipe_fd[1] >= 0) close(pipe_fd[1]);
    if (tty_fd >= 0) close(tty_fd);
    exit(0);
}

void handle_sigquit(int sig) {
    printf("DEBUG: Received SIGQUIT\n");
    if (child_pid > 0) {
        kill(child_pid, SIGTSTP);
        printf("Process %d hidden\n", child_pid);
        if (pipe_fd[0] >= 0) close(pipe_fd[0]);
        if (pipe_fd[1] >= 0) close(pipe_fd[1]);
        if (tty_fd >= 0) close(tty_fd);
        exit(0);
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

    // Set up signal handlers
    struct sigaction sa_int, sa_quit;
    sa_int.sa_handler = handle_sigint;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, NULL);

    sa_quit.sa_handler = handle_sigquit;
    sigemptyset(&sa_quit.sa_mask);
    sa_quit.sa_flags = 0;
    sigaction(SIGQUIT, &sa_quit, NULL);

    // Create pipe for child's stdout/stderr
    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    // Fork to create child process
    child_pid = fork();
    if (child_pid < 0) {
        perror("Fork failed");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return 1;
    }

    if (child_pid == 0) {
        // Child process
        close(pipe_fd[0]); // Close read end
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);
        close(STDIN_FILENO); // Prevent input

        execl(command, command, (char*)NULL);
        perror("execl failed");
        exit(1);
    }

    // Parent process
    close(pipe_fd[1]); // Close write end

    // Log process info
    char proc_file[MAX_NAME];
    snprintf(proc_file, MAX_NAME, "user.%s/procman.%s.txt", subkey, subkey);
    FILE* fp = fopen(proc_file, "a");
    if (fp) {
        fprintf(fp, "%s %d %s\n", proc_hash, child_pid, command);
        fclose(fp);
    }

    printf("DEBUG: Launched child PID %d\n", child_pid);

    // Open /dev/tty for output
    tty_fd = open("/dev/tty", O_WRONLY);
    if (tty_fd == -1) {
        perror("Failed to open /dev/tty");
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        close(pipe_fd[0]);
        return 1;
    }

    // Set pipe to non-blocking
    fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);

    char input[256];
    char buffer[4096];
    ssize_t bytes_read;

    // Main loop: handle input and child output
    while (1) {
        printf("Type 'show', 'hide', or wait for signals: ");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) != NULL) {
            input[strcspn(input, "\n")] = 0;
            if (strcmp(input, "show") == 0) {
                if (!show_output) {
                    system("clear");
                    printf("Showing child output on console...\n");
                    show_output = 1;
                }
            } else if (strcmp(input, "hide") == 0) {
                if (show_output) {
                    system("clear");
                    printf("Hiding child output.\n");
                    show_output = 0;
                }
            }
        }

        // Read child output
        while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
            if (show_output) {
                write(tty_fd, buffer, bytes_read);
            }
        }

        usleep(10000); // Prevent busy-waiting
    }

    // Cleanup (unreachable in normal flow)
    close(pipe_fd[0]);
    if (tty_fd >= 0) close(tty_fd);
    return 0;
}
