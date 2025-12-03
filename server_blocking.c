#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_PORT 8080
#define BACKLOG 10
#define RECV_BUF_SIZE 2048

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char recv_buf[RECV_BUF_SIZE];

    // 1. Create socket (IPv4, TCP)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Allow quick reuse of the address:port
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 2. Bind to 0.0.0.0:8080
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Listen for incoming connections
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", SERVER_PORT);

    // 4. Accept a single connection (blocking)
    client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
    if (client_fd < 0) {
        perror("accept");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Client connected.\n");

    // 5. Read request (blocking)
    ssize_t n = read(client_fd, recv_buf, sizeof(recv_buf) - 1);
    if (n < 0) {
        perror("read");
        close(client_fd);
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    recv_buf[n] = '\0';
    printf("Received request:\n%s\n", recv_buf);

    // 6. Send a fixed HTTP response
    const char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/plain\r\n"
        "Content-Length: 20\r\n"
        "\r\n"
        "Hello from C server\n";

    ssize_t total = 0;
    ssize_t len = (ssize_t)strlen(response);
    while (total < len) {
        ssize_t sent = write(client_fd, response + total, len - total);
        if (sent < 0) {
            perror("write");
            break;
        }
        total += sent;
    }

    printf("Response sent, closing connection.\n");

    // 7. Clean up
    close(client_fd);
    close(server_fd);

    return 0;
}

