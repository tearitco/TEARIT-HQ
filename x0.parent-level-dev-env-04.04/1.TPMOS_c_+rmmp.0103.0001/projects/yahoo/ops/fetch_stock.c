// File: fetch_stock.c🗃️🔬️
// Compile: gcc -o /+x/fetch_stock.+x fetch_stock.c
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define ISVALIDSOCKET(s) ((s) >= 0)
#define CLOSESOCKET(s) close(s)
#define SOCKET int
#define GETSOCKETERRNO() (errno)
#define TIMEOUT 5.0
#define INITIAL_RESPONSE_SIZE 131072 // Increased from 32768 to 128KB

void parse_url(char *url, char **hostname, char **port, char **path) {
    printf("URL: %s\n", url);
    char *p = strstr(url, "://");
    char *protocol = 0;
    if (p) {
        protocol = url;
        *p = 0;
        p += 3;
    } else {
        p = url;
    }
    if (protocol && strcmp(protocol, "http")) {
        fprintf(stderr, "Unknown protocol '%s'. Only 'http' is supported.\n", protocol);
        exit(1);
    }
    *hostname = p;
    while (*p && *p != ':' && *p != '/' && *p != '#') ++p;
    *port = "80";
    if (*p == ':') {
        *p++ = 0;
        *port = p;
    }
    while (*p && *p != '/' && *p != '#') ++p;
    *path = p;
    if (*p == '/') *path = p + 1;
    *p = 0;
    while (*p && *p != '#') ++p;
    if (*p == '#') *p = 0;
    printf("hostname: %s\n", *hostname);
    printf("port: %s\n", *port);
    printf("path: %s\n", *path);
}

void send_request(SOCKET s, char *hostname, char *port, char *path) {
    char buffer[2048];
    sprintf(buffer, "GET /%s HTTP/1.1\r\n", path);
    sprintf(buffer + strlen(buffer), "Host: %s:%s\r\n", hostname, port);
    sprintf(buffer + strlen(buffer), "Connection: close\r\n");
    sprintf(buffer + strlen(buffer), "User-Agent: honpwc web_get 1.0\r\n");
    sprintf(buffer + strlen(buffer), "\r\n");
    send(s, buffer, strlen(buffer), 0);
    printf("Sent Headers:\n%s", buffer);
}

SOCKET connect_to_host(char *hostname, char *port) {
    printf("Configuring remote address...\n");
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo *peer_address;
    if (getaddrinfo(hostname, port, &hints, &peer_address)) {
        fprintf(stderr, "getaddrinfo() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    printf("Remote address is: ");
    char address_buffer[100];
    char service_buffer[100];
    getnameinfo(peer_address->ai_addr, peer_address->ai_addrlen,
                address_buffer, sizeof(address_buffer),
                service_buffer, sizeof(service_buffer),
                NI_NUMERICHOST);
    printf("%s %s\n", address_buffer, service_buffer);
    printf("Creating socket...\n");
    SOCKET server = socket(peer_address->ai_family, peer_address->ai_socktype, peer_address->ai_protocol);
    if (!ISVALIDSOCKET(server)) {
        fprintf(stderr, "socket() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    printf("Connecting...\n");
    if (connect(server, peer_address->ai_addr, peer_address->ai_addrlen)) {
        fprintf(stderr, "connect() failed. (%d)\n", GETSOCKETERRNO());
        exit(1);
    }
    freeaddrinfo(peer_address);
    printf("Connected.\n\n");
    return server;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: fetch_stock.+x symbol\n");
        return 1;
    }
    char *symbol = argv[1];
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.txt", symbol);
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open file %s\n", filename);
        return 1;
    }
    char url[512];
    snprintf(url, sizeof(url), "http://query2.finance.yahoo.com/v8/finance/chart/%s", symbol);
    char *hostname, *port, *path;
    parse_url(url, &hostname, &port, &path);
    SOCKET server = connect_to_host(hostname, port);
    send_request(server, hostname, port, path);
    const clock_t start_time = clock();
    size_t response_size = INITIAL_RESPONSE_SIZE;
    char *response = malloc(response_size + 1);
    if (!response) {
        fprintf(stderr, "Failed to allocate memory\n");
        fclose(fp);
        return 1;
    }
    char *p = response, *q;
    char *end = response + response_size;
    char *body = 0;
    int encoding = 0;
    int remaining = 0;
    while (1) {
        if ((clock() - start_time) / CLOCKS_PER_SEC > TIMEOUT) {
            fprintf(stderr, "timeout after %.2f seconds\n", TIMEOUT);
            free(response);
            fclose(fp);
            return 1;
        }
        if (p >= end - 1) {
            size_t new_size = response_size * 2;
            char *new_response = realloc(response, new_size + 1);
            if (!new_response) {
                fprintf(stderr, "Failed to reallocate buffer to %zu bytes\n", new_size);
                free(response);
                fclose(fp);
                return 1;
            }
            p = new_response + (p - response);
            end = new_response + new_size;
            response = new_response;
            response_size = new_size;
            printf("Resized buffer to %zu bytes\n", response_size);
        }
        fd_set reads;
        FD_ZERO(&reads);
        FD_SET(server, &reads);
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 200000;
        if (select(server + 1, &reads, 0, 0, &timeout) < 0) {
            fprintf(stderr, "select() failed. (%d)\n", GETSOCKETERRNO());
            free(response);
            fclose(fp);
            return 1;
        }
        if (FD_ISSET(server, &reads)) {
            int bytes_received = recv(server, p, end - p - 1, 0);
            if (bytes_received < 1) {
                if (encoding == 2 && body) {
                    fprintf(fp, "%.*s", (int)(p - body), body);
                }
                break;
            }
            p += bytes_received;
            *p = 0;
            if (!body && (body = strstr(response, "\r\n\r\n"))) {
                *body = 0;
                body += 4;
                q = strstr(response, "\nContent-Length: ");
                if (q) {
                    encoding = 1;
                    q = strchr(q, ' ');
                    q += 1;
                    remaining = strtol(q, 0, 10);
                } else {
                    q = strstr(response, "\nTransfer-Encoding: chunked");
                    encoding = q ? 2 : 0;
                    remaining = 0;
                }
            }
            if (body && encoding == 2) {
                while (remaining == 0) {
                    if ((q = strstr(body, "\r\n"))) {
                        remaining = strtol(body, 0, 16);
                        if (!remaining) {
                            fprintf(fp, "%.*s", (int)(p - body), body);
                            break;
                        }
                        body = q + 2;
                    } else {
                        break;
                    }
                    if (remaining && p - body >= remaining) {
                        fprintf(fp, "%.*s", remaining, body);
                        body += remaining + 2;
                        remaining = 0;
                    }
                }
            }
        }
    }
    CLOSESOCKET(server);
    free(response);
    fclose(fp);
    printf("Data written to %s\n", filename);
    return 0;
}
