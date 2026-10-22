#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080

int main() {
    int client_fd = 0;
    struct sockaddr_in server_addr;
    char buffer[4096] = {0};

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd < 0) {
        perror("Socket creation error");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        return -1;
    }

    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection Failed");
        return -1;
    }

    pid_t child_pid = fork();

    if (child_pid == 0) {
        // listen to the server and print immediately
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int recv_size = read(client_fd, buffer, sizeof(buffer) - 1);
            
            if (recv_size <= 0) {
                printf("\nServer disconnected.\n");
                kill(getppid(), SIGKILL); // Tell parent to stop waiting for keyboard
                break;
            }
            write(STDOUT_FILENO, buffer, recv_size);
        }
        exit(0);
    } else {
        // PARENT : listen to the keyboard and send to server
        while (1) {
            memset(buffer, 0, sizeof(buffer));
            int send_size = read(STDIN_FILENO, buffer, sizeof(buffer));
            
            if (send_size > 0) {
                write(client_fd, buffer, send_size);
            } else {
                break;
            }
        }
        kill(child_pid, SIGKILL); // Cleaning up child if parent exits
    }

    close(client_fd);
    return 0;
}