#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define SERVER_PORT 8080
#define BACKLOG 10
#define RECV_BUF_SIZE 2048
#define MAX_PATH 256
#define MAX_CLIENTS FD_SETSIZE  // simple upper bound

const char *get_content_type(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    return "application/octet-stream";
}

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
    return 0;
}

void handle_client_request(int client_fd, const char *www_root) {
    char recv_buf[RECV_BUF_SIZE];

    ssize_t n = read(client_fd, recv_buf, sizeof(recv_buf) - 1);
    if (n <= 0) {
        return; // error or closed; caller will close fd
    }
    recv_buf[n] = '\0';
    printf("Received from %d:\n%s\n", client_fd, recv_buf);

    char *get = strstr(recv_buf, "GET ");
    if (!get) return;
    char *start = get + 4;
    char *end   = strstr(start, " HTTP");
    if (!end) return;
    size_t path_len = end - start;
    if (path_len >= MAX_PATH - 5) return;

    char path[MAX_PATH];
    strncpy(path, start, path_len);
    path[path_len] = '\0';

    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    char filepath[MAX_PATH];
    snprintf(filepath, sizeof(filepath), "%s%s", www_root, path + 1);

    struct stat st;
    if (stat(filepath, &st) != 0 || S_ISDIR(st.st_mode)) {
        const char *response404 =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "File not found";
        ssize_t sent = write(client_fd, response404, strlen(response404));
        if (sent < 0) { perror("write"); }
	printf("Sent 404 for %s\n", filepath);
    } else {
        FILE *file = fopen(filepath, "rb");
        if (!file) return;
        const char *content_type = get_content_type(filepath);

        char header[512];
        snprintf(header, sizeof(header),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %ld\r\n"
                 "\r\n",
                 content_type, (long)st.st_size);
        ssize_t sent = write(client_fd, header, strlen(header));
	if (sent < 0) { perror("write"); }
	
        char buf[4096];
        size_t bytes_read;
        while ((bytes_read = fread(buf, 1, sizeof(buf), file)) > 0) {
            ssize_t sent = write(client_fd, buf, bytes_read);
	    if (sent < 0) { perror("write"); }
	}
        fclose(file);
        printf("Served %s (%ld bytes) to %d\n", filepath, (long)st.st_size, client_fd);
    }
}

int main(void) {
    int listen_fd;
    struct sockaddr_in addr;
    char www_root[] = "www/";

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind"); close(listen_fd); exit(EXIT_FAILURE);
    }
    if (listen(listen_fd, BACKLOG) < 0) {
        perror("listen"); close(listen_fd); exit(EXIT_FAILURE);
    }

    if (set_nonblocking(listen_fd) < 0) {
        perror("set_nonblocking listen_fd");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    printf("Non-blocking select() server on port %d...\n", SERVER_PORT);

    int client_fds[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) client_fds[i] = -1;

    fd_set read_fds;
    int max_fd = listen_fd;

    while (1) {
        FD_ZERO(&read_fds);
        FD_SET(listen_fd, &read_fds);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i] != -1) {
                FD_SET(client_fds[i], &read_fds);
                if (client_fds[i] > max_fd) max_fd = client_fds[i];
            }
        }

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(listen_fd, &read_fds)) {
            struct sockaddr_in caddr;
            socklen_t clen = sizeof(caddr);
            int client_fd = accept(listen_fd, (struct sockaddr *)&caddr, &clen);
            if (client_fd >= 0) {
                if (set_nonblocking(client_fd) < 0) {
                    perror("set_nonblocking client");
                    close(client_fd);
                } else {
                    int placed = 0;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (client_fds[i] == -1) {
                            client_fds[i] = client_fd;
                            placed = 1;
                            break;
                        }
                    }
                    if (!placed) {
                        printf("Too many clients, closing %d\n", client_fd);
                        close(client_fd);
                    } else {
                        printf("New client fd=%d\n", client_fd);
                    }
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = client_fds[i];
            if (fd != -1 && FD_ISSET(fd, &read_fds)) {
                handle_client_request(fd, www_root);
                close(fd);
                client_fds[i] = -1;
            }
        }
    }

    close(listen_fd);
    return 0;
}
