
# Stage 1: build the server
FROM docker.io/library/debian:stable-slim AS build

RUN apt-get update && apt-get install -y gcc make && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy source and Makefile
COPY src/ src/
COPY www/ www/
COPY Makefile .

# Build the server binary
RUN make all

# Stage 2: runtime image
FROM docker.io/library/debian:stable-slim

WORKDIR /app

# Copy binary and www/ from build stage
COPY --from=build /app/build/server /app/server
COPY --from=build /app/www /app/www

EXPOSE 8080

CMD ["/app/server"]
