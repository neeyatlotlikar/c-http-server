# Minimal C HTTP Server in a Podman Container

This project is a from-scratch HTTP server written in C that serves static files using non-blocking I/O, packaged into a rootless Podman container.

## Features

- Minimal HTTP/1.1 server written in C
- Handles `GET` requests and serves static files from a `www/` directory
- Non-blocking sockets (`O_NONBLOCK`) with an event loop using `select()`
- Basic HTTP handling:
  - Path parsing from the request line
  - `200 OK` and `404 Not Found` responses
  - Simple `Content-Type` detection (e.g., `.html`, `.css`, `.js`)
- Structured project with a `Makefile` build
- Rootless Podman image using a multi-stage build

## Project structure

- `src/` – C source files (`server_select.c`)
- `www/` – Static files (e.g., `index.html`)
- `build/` – Compiled artifacts (e.g., `build/server`)
- `Containerfile` – Multi-stage container build definition
- `Makefile` – Build configuration

## Build and run (native)

### Build

`make`

### Run the server

`./build/server`

In another terminal, test:

`curl http://localhost:8080/`

Place your static files in `www/` (for example `www/index.html`). The server maps `/` to `/index.html` and serves other paths directly (e.g., `/style.css` → `www/style.css`).

## Build and run (Podman)

### Build container image

`podman build -t c-http-server:dev -f Containerfile .`

### Run the server in a rootless container

`podman run --rm -p 8080:8080 c-http-server:dev`

### Test from host

`curl http://localhost:8080/`

The container runs the same non-blocking C HTTP server binary and serves files from `/app/www` inside the image.

## Implementation notes

- Uses POSIX sockets: `socket`, `bind`, `listen`, `accept`, `read`, `write`, `close`
- Sets `O_NONBLOCK` on the listening socket and client sockets via `fcntl`
- Uses `select()` to implement a simple event loop:
  - Watches the listening socket for new connections
  - Watches client sockets for readable data
- Static file serving uses `stat()` for file size and `fopen`/`fread` to stream contents

## Skills demonstrated

- C systems programming (sockets, file I/O, non-blocking I/O)
- Understanding of HTTP request/response basics
- Event-driven server architecture using `select()` and file descriptors
- Linux tooling: `gcc`, `make`
- Containerization with rootless Podman and multi-stage builds

## Future work
- Graceful shutdown via signals (SIGTERM handling + select timeout).
- Basic routing, e.g. /api/hello returning JSON.
- Config via environment variables (port, www root).
- Slightly richer logging (timestamp + client fd).
- Simple metrics endpoint (e.g., /metrics with request counts).

