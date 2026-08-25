#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <string.h>

// Global variable to store the child PID
pid_t child_pid = 0;
// File descriptor for the pipe (child's stdout/stderr)
int pipe_fd[2];
// File descriptor for /dev/tty
int tty_fd = -1;
// Flag to control whether to show output
int show_output = 0;

// Signal handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    if (child_pid > 0) {
        // Send SIGTERM to the child
        kill(child_pid, SIGTERM);
        // Wait for the child to terminate
        waitpid(child_pid, NULL, 0);
        printf("\nChild process %d terminated.\n", child_pid);
    }
    // Close file descriptors
    if (pipe_fd[0] >= 0) close(pipe_fd[0]);
    if (tty_fd >= 0) close(tty_fd);
    exit(0);
}

int main() {
    // Path to the binary
    const char *binary_path = "./+x/test.app1.+x";

    // Set up the SIGINT handler
    signal(SIGINT, handle_sigint);

    // Create a pipe for the child's stdout/stderr
    if (pipe(pipe_fd) == -1) {
        perror("Pipe creation failed");
        return 1;
    }

    // Fork to create a child process
    child_pid = fork();

    if (child_pid < 0) {
        perror("Fork failed");
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return 1;
    }

    if (child_pid == 0) {
        // Child process

        // Close read end of the pipe (child only writes)
        close(pipe_fd[0]);

        // Redirect stdout and stderr to the pipe's write end
        dup2(pipe_fd[1], STDOUT_FILENO);
        dup2(pipe_fd[1], STDERR_FILENO);
        close(pipe_fd[1]);

        // Close stdin to prevent input
        close(STDIN_FILENO);

        // Prepare arguments for execvp
        char *args[] = {(char *)binary_path, NULL};

        // Execute the binary
        execvp(binary_path, args);

        // If execvp fails
        perror("execvp failed");
        exit(1);
    }

    // Parent process

    // Close write end of the pipe (parent only reads)
    close(pipe_fd[1]);

    printf("Child process launched with PID %d.\n", child_pid);

    // Open /dev/tty for output when "show" is typed
    tty_fd = open("/dev/tty", O_WRONLY);
    if (tty_fd == -1) {
        perror("Failed to open /dev/tty");
        kill(child_pid, SIGTERM);
        waitpid(child_pid, NULL, 0);
        close(pipe_fd[0]);
        return 1;
    }

    // Buffer for reading user input
    char input[256];
    // Buffer for reading child output
    char buffer[4096];
    ssize_t bytes_read;

    // Set pipe to non-blocking mode to avoid hanging
    fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);

    // Main loop: read user input and handle child output
    while (1) {
        // Read from stdin (user input)
        if (fgets(input, sizeof(input), stdin) != NULL) {
            // Remove trailing newline
            input[strcspn(input, "\n")] = 0;

            // Check for commands
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

        // Read from the pipe (child output)
        while ((bytes_read = read(pipe_fd[0], buffer, sizeof(buffer))) > 0) {
            if (show_output) {
                // Write to /dev/tty
                write(tty_fd, buffer, bytes_read);
            }
            // Otherwise, discard (output remains hidden)
        }

        // Avoid busy-waiting
        usleep(10000); // Sleep for 10ms
    }

    // Unreachable, but clean up just in case
    close(pipe_fd[0]);
    if (tty_fd >= 0) close(tty_fd);
    return 0;
}
