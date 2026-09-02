#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#include <sys/wait.h>

#define PORT 8080
#define BUFFER_SIZE 8192
#define MAX_CLIENTS 10

// Function to execute the chatbot and return the response
char* execute_chatbot(const char* curriculum, const char* prompt, int responseLength, float temperature) {
    static char command[BUFFER_SIZE];
    static char result[4096];

    // Create the command to execute the chatbot
    snprintf(command, sizeof(command), "./+x/chatbot_moe_v1.+x \"%s\" \"%s\" %d %f", curriculum, prompt, responseLength, temperature);

    // Execute the command and capture output
    FILE* pipe = popen(command, "r");
    if (!pipe) {
        snprintf(result, sizeof(result), "{\"error\": \"Failed to execute chatbot\"}");
        return result;
    }

    size_t len = 0;
    char buffer[1024];
    result[0] = '\0';

    // Read the output from the command
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        strncat(result, buffer, sizeof(result) - strlen(result) - 1);
    }

    pclose(pipe);

    // Extract response from output if needed
    char* response_start = strstr(result, "Response:");
    if (response_start) {
        response_start += 9; // Skip "Response:"
        while (*response_start == ' ' || *response_start == '\t') response_start++;
        strcpy(result, response_start);
    }

    // Clean up and return the result
    return result;
}

// Function to get list of curriculum files
char* list_curriculum_files() {
    static char response[2048];
    strcpy(response, "{\"files\": [");

    FILE* pipe = popen("ls *.txt 2>/dev/null | grep -E 'curriculum|vocab|model'", "r");
    if (!pipe) {
        strcpy(response, "{\"files\": []}");
        return response;
    }

    char filename[256];
    int first = 1;

    while (fgets(filename, sizeof(filename), pipe) != NULL) {
        // Remove newline
        filename[strcspn(filename, "\n")] = 0;

        if (!first) strcat(response, ",");
        first = 0;

        strcat(response, "\"");
        strcat(response, filename);
        strcat(response, "\"");
    }

    strcat(response, "]}");
    pclose(pipe);

    return response;
}

// Function to get debug information
char* get_debug_info() {
    static char response[4096];

    // Try to read debug_chain.txt if it exists
    FILE* f = fopen("debug_chain.txt", "r");
    if (!f) {
        snprintf(response, sizeof(response), "{\"debug\": \"No debug information available\"}");
        return response;
    }

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len > sizeof(response) - 100) len = sizeof(response) - 100;

    fread(response, 1, len, f);
    response[len] = '\0';

    // Wrap in JSON
    char* temp = malloc(strlen(response) + 20);
    snprintf(temp, strlen(response) + 20, "{\"debug\": \"%s\"}", response);
    strcpy(response, temp);
    free(temp);

    fclose(f);
    return response;
}

// Function to serve a file
void serve_file(int client_socket, const char* filename) {
    // Prevent directory traversal attacks
    if (strstr(filename, "..")) {
        const char* bad_request = "HTTP/1.1 400 Bad Request\r\n\r\n";
        send(client_socket, bad_request, strlen(bad_request), 0);
        return;
    }

    char filepath[512];
    // Route web-interface files correctly
    if (strcmp(filename, "/") == 0 || strcmp(filename, "/index.html") == 0) {
        strcpy(filepath, "web-interface/index.php");
    } else if (strncmp(filename, "/css/", 5) == 0) {
        snprintf(filepath, sizeof(filepath), "web-interface%s", filename);
    } else if (strncmp(filename, "/js/", 4) == 0) {
        snprintf(filepath, sizeof(filepath), "web-interface%s", filename);
    } else if (strcmp(filename, "/api/curricula") == 0 ||
               strcmp(filename, "/api/chat") == 0 ||
               strcmp(filename, "/api/debug") == 0) {
        // These are API endpoints, not files
        return;
    } else {
        snprintf(filepath, sizeof(filepath), "web-interface%s", filename);
    }

    // Open the file
    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        const char* not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client_socket, not_found, strlen(not_found), 0);
        return;
    }

    // Get file stats
    struct stat file_stat;
    fstat(file_fd, &file_stat);

    // Determine content type based on file extension
    const char* content_type = "application/octet-stream";
    if (strstr(filepath, ".html") || strstr(filepath, ".php")) content_type = "text/html";
    else if (strstr(filepath, ".css")) content_type = "text/css";
    else if (strstr(filepath, ".js")) content_type = "application/javascript";
    else if (strstr(filepath, ".json")) content_type = "application/json";
    else if (strstr(filepath, ".png")) content_type = "image/png";
    else if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg")) content_type = "image/jpeg";
    else if (strstr(filepath, ".gif")) content_type = "image/gif";

    // Send HTTP headers
    char headers[512];
    snprintf(headers, sizeof(headers),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n\r\n", 
             content_type, file_stat.st_size);
    send(client_socket, headers, strlen(headers), 0);

    // Send file content
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(client_socket, buffer, bytes_read, 0);
    }

    close(file_fd);
}

// Function to handle API requests
void handle_api_request(int client_socket, const char* method, const char* path, const char* body) {
    if (strcmp(path, "/api/curricula") == 0 && strcmp(method, "GET") == 0) {
        char response[2048];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n"
                 "%s", list_curriculum_files());
        send(client_socket, response, strlen(response), 0);
    }
    else if (strcmp(path, "/api/chat") == 0 && strcmp(method, "POST") == 0) {
        // Parse JSON from body to extract parameters
        char curriculum[256] = "vocab_model.txt";
        char prompt[512] = "Hello";
        int responseLength = 10;
        float temperature = 1.0;

        // Simple JSON parsing - find values in the body
        char* temp_curriculum = strstr(body, "\"curriculum\"");
        if (temp_curriculum) {
            temp_curriculum = strchr(temp_curriculum, ':');
            if (temp_curriculum) {
                temp_curriculum++;
                while (*temp_curriculum == ' ' || *temp_curriculum == '\t') temp_curriculum++;
                if (*temp_curriculum == '"') {
                    temp_curriculum++;
                    char* end = strchr(temp_curriculum, '"'); 
                    if (end) {
                        int len = end - temp_curriculum;
                        if (len < sizeof(curriculum) - 1) {
                            strncpy(curriculum, temp_curriculum, len);
                            curriculum[len] = '\0';
                        }
                    }
                }
            }
        }

        char* temp_prompt = strstr(body, "\"prompt\"");
        if (temp_prompt) {
            temp_prompt = strchr(temp_prompt, ':');
            if (temp_prompt) {
                temp_prompt++;
                while (*temp_prompt == ' ' || *temp_prompt == '\t') temp_prompt++;
                if (*temp_prompt == '"') {
                    temp_prompt++;
                    char* end = strchr(temp_prompt, '"'); 
                    if (end) {
                        int len = end - temp_prompt;
                        if (len < sizeof(prompt) - 1) {
                            strncpy(prompt, temp_prompt, len);
                            prompt[len] = '\0';
                        }
                    }
                }
            }
        }

        char* temp_length = strstr(body, "\"responseLength\"");
        if (temp_length) {
            temp_length = strchr(temp_length, ':');
            if (temp_length) {
                temp_length++;
                while (*temp_length == ' ' || *temp_length == '\t') temp_length++;
                responseLength = atoi(temp_length);
            }
        }

        char* temp_temp = strstr(body, "\"temperature\"");
        if (temp_temp) {
            temp_temp = strchr(temp_temp, ':');
            if (temp_temp) {
                temp_temp++;
                while (*temp_temp == ' ' || *temp_temp == '\t') temp_temp++;
                temperature = atof(temp_temp);
            }
        }

        // Execute the chatbot
        char* chatbot_response = execute_chatbot(curriculum, prompt, responseLength, temperature);

        // Create JSON response
        char response[4096];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n"
                 "{\"prompt\":\"%s\",\"response\":\"%s\",\"curriculum\":\"%s\",\"length\":%d,\"temperature\":%f}",
                 prompt, chatbot_response, curriculum, responseLength, temperature);

        send(client_socket, response, strlen(response), 0);
    }
    else if (strcmp(path, "/api/debug") == 0 && strcmp(method, "GET") == 0) {
        char response[4096];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: application/json\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Connection: close\r\n\r\n"
                 "%s", get_debug_info());
        send(client_socket, response, strlen(response), 0);
    }
    else {
        const char* not_found = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client_socket, not_found, strlen(not_found), 0);
    }
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to reuse address
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind the socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Chatbot Web Interface Server listening on port %d\n", PORT);
    printf("Open your browser and go to http://localhost:%d\n", PORT);

    while (1) {
        // Accept a client connection
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        // Read the request
        int valread = read(client_socket, buffer, BUFFER_SIZE-1);
        if (valread <= 0) {
            close(client_socket);
            continue;
        }
        buffer[valread] = '\0';

        // Parse the request line (first line of HTTP request)
        char* method = strtok(buffer, " ");
        char* path = strtok(NULL, " ");
        char* protocol = strtok(NULL, "\r\n");

        if (!method || !path) {
            close(client_socket);
            continue;
        }

        // Check if it's an API request
        if (strncmp(path, "/api/", 5) == 0) {
            // Find the body of the request (after headers)
            char* body = strstr(buffer, "\r\n\r\n");
            if (body) {
                body += 4; // Skip the \r\n\r\n
            } else {
                body = "";
            }

            handle_api_request(client_socket, method, path, body);
        } else {
            // Serve static files
            serve_file(client_socket, path);
        }

        // Close the client socket
        close(client_socket);
    }

    return 0;
}
