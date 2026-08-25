/* conn_test.c - P2P Connection Diagnostic Utility */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8000

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s listen           (Run as server)\n", argv[0]);
        printf("Usage: %s connect <IP>    (Run as client)\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "listen") == 0) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(PORT);
        
        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("Bind failed"); return 1;
        }
        listen(fd, 3);
        printf("[SUCCESS] Listening on Port %d. Waiting for client...\n", PORT);
        
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        int client_fd = accept(fd, (struct sockaddr *)&client, &len);
        if (client_fd >= 0) {
            printf("[SUCCESS] Machine %s connected!\n", inet_ntoa(client.sin_addr));
            send(client_fd, "PING_ACK", 8, 0);
            close(client_fd);
        }
        close(fd);
    } 
    else if (strcmp(argv[1], "connect") == 0 && argc > 2) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        inet_pton(AF_INET, argv[2], &addr.sin_addr);

        printf("Attempting to connect to %s:%d...\n", argv[2], PORT);
        if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("[FAIL] Connection failed"); return 1;
        }
        
        char buf[16] = {0};
        recv(sock, buf, 15, 0);
        printf("[SUCCESS] Received from host: %s\n", buf);
        close(sock);
    }
    
    return 0;
}
