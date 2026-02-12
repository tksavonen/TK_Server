// src/main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>

#include "include/main.h"

#define MAX_CONNECTIONS     5
#define PORT                8080
#define RESPONSE_SIZE       8192

int server_fd, client_fd;
struct sockaddr_in address;
socklen_t addrlen = sizeof(address);

void sigint_handler(int sig) {
    int t = 2;
    char input[2];

    while (t >= 1) {
        printf("\nTerminate? [q]\nShutting down in ... %d\n", t);
        sleep(1);
        t--;
    }

    signal(SIGINT, SIG_DFL);
    exit(0);
}

int create_server_socket(int port) {
    // create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("ERROR: socket init failed");
        return 1;
    }
    printf("Socket created\n");

    // configure address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // bind socket to port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("ERROR: bind failed");
        return 1;
    }
    printf("Bind successful\n");

    // listen
    if (listen(server_fd, MAX_CONNECTIONS) < 0) {
        perror("ERROR: listen");
        return 1;
    }

    printf("Server running on http://localhost:%d\n", PORT);
    return 0;
}

void handle_response(int client_fd) {
    char buffer[3000] = {0};
    read(client_fd, buffer, sizeof(buffer));

    char method[16], path[16];
    sscanf(buffer, "%s %s", method, path);

    if (path[0] == '/')
        memmove(path, path + 1, strlen(path));

    if (strlen(path) == 0)
        strcpy(path, "index.html");

    printf("%s\n", buffer);

    const char *mime_type = "application/octet-stream";

    if (strstr(path, ".html")) mime_type = "text/html";
        else if (strstr(path, ".jpg")) mime_type = "image/jpg";
        else if (strstr(path, ".png")) mime_type = "image/png";

    char fullpath[16];
    snprintf(fullpath, sizeof(fullpath), "www/%s", path);
    
    FILE *fp = fopen(fullpath, "rb");
    if (!fp) {
        char *notfound = "HTTP/1.1 404 Not Found\r\n"
                         "Content-Length: 0\r\n"
                         "Connection: close\r\n\r\n";
        send(client_fd, notfound, strlen(notfound), 0);
        close(client_fd);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    char *filebuf = malloc(filesize);
    fread(filebuf, 1, filesize, fp);
    fclose(fp);

    char response[RESPONSE_SIZE];
    int n = snprintf(response, RESPONSE_SIZE,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
        mime_type, filesize
    );

    send(client_fd, response, n, 0);
    send(client_fd, filebuf, filesize, 0);

    free(filebuf);
    close(client_fd);
}

void shutdown_server() {
    signal(SIGINT, sigint_handler);
    printf("Shutting down server in PORT %d...", PORT);
    raise(SIGINT);
    close(server_fd);
}

int main() {
    create_server_socket(PORT);
    signal(SIGINT, sigint_handler);

    while (1) {
        printf("Waiting for new connection...\n");
        fflush(stdout);

        struct sockaddr_in client_address;
        socklen_t client_addrlen = sizeof(client_address);

        client_fd = accept(server_fd, (struct sockaddr *)&client_address, &client_addrlen);
        if (client_fd < 0) {
            perror("ERROR: accept");
            continue;
        }
        printf("Client connected: %s:%d\n",
            inet_ntoa(client_address.sin_addr),
            ntohs(client_address.sin_port));

        handle_response(client_fd);
    }

    return 0;
}