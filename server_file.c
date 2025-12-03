#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>

#define SERVER_PORT 8080
#define BACKLOG 10
#define RECV_BUF_SIZE 2048
#define MAX_PATH 256

// Simple content type mapping (expand as needed)
const char *get_content_type(const char *path) {
    if (strstr(path, ".html")) return "text/html";
    if (strstr(path, ".css")) return "text/css";
    if (strstr(path, ".js")) return "text/javascript";
    return "application/octet-stream";
}

int main(void) {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    char recv_buf[RECV_BUF_SIZE];
    char www_root[] = "www/";
    
    // Create socket (IPv4, TCP)
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

    // Bind to 0.0.0.0:8080
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("File-serving server listening on port %d (www/directory)...\n", SERVER_PORT);

    while(1) {
        // Accept a single connection (blocking)
        client_fd = accept(server_fd, (struct sockaddr *)&addr, &addrlen);
        if (client_fd < 0) { perror("accept"); continue; }

        printf("Client connected.\n");

        // Read request (blocking)
        ssize_t n = read(client_fd, recv_buf, sizeof(recv_buf) - 1);
        if (n < 0) { perror("read"); close(client_fd); continue; }
        recv_buf[n] = '\0';
        printf("Received request:\n%s\n", recv_buf);
        
	// Parse GET path (simple: find space after "GET ", before "HTTP")
	char *start = strstr(recv_buf, "GET ") + 4;
	char *end = strstr(start, " HTTP");
	if (!start || !end) { close(client_fd); continue; }
        size_t path_len = end - start;
	if (path_len >= MAX_PATH - 5) { close(client_fd); continue; } // Safety 
        
	char path[MAX_PATH];
	strncpy(path, start, path_len);
        path[path_len] = '\0';

	printf("Requested path: %s\n", path);
	
	// Default to index.html if "/"
	if (strcmp(path, "/") == 0) {
            strcpy(path, "/index.html");
	}
        
	// Build full file path
	char filepath[MAX_PATH];
	snprintf(filepath, sizeof(filepath), "%s%s", www_root, path + 1); // Skip leading "/"
	
	// Check if file exists and get size
        struct stat st;
        if (stat(filepath, &st) != 0 || S_ISDIR(st.st_mode)) {
            // 404
            const char *response404 =
                "HTTP/1.1 404 Not Found\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 13\r\n"
                "\r\n"
                "File not found";
            ssize_t result = write(client_fd, response404, strlen(response404));
            if (result < 0) { perror("response404"); } 
	    printf("Sent 404 for %s\n", filepath);
        } else {
            // Serve file
            FILE *file = fopen(filepath, "rb");
            if (!file) { close(client_fd); continue; }

            const char *content_type = get_content_type(filepath);
            char header[512];
            snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %ld\r\n"
                "\r\n", content_type, (long)st.st_size);

            // Send header
            ssize_t result = write(client_fd, header, strlen(header));
            if (result < 0) { perror("send header"); }
	    
            // Send file content
            char buf[4096];
            size_t bytes_read;
            while ((bytes_read = fread(buf, 1, sizeof(buf), file)) > 0) {
                ssize_t result = write(client_fd, buf, bytes_read);
                if (result < 0) { perror("serve file"); break; }
	    }
            fclose(file);
            printf("Served file: %s (%ld bytes)\n", filepath, (long)st.st_size);
        }

        // Clean up
        close(client_fd);
    }
    close(server_fd);

    return 0;
}

