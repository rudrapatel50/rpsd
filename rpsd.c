#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <signal.h>

#define BACKLOG 10
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    char name[200];
} player;

player players_queue[1024];
int count = 0;

void error_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int setup_server_socket(int port) {
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        error_exit("socket");

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        error_exit("bind");

    if (listen(sockfd, BACKLOG) == -1)
        error_exit("listen");

    return sockfd;
}

char* find_winner(char *move1, char *move2) {
    if (strcmp(move1, move2) == 0) {
        return "D";
    }

    if ((strcmp(move1, "ROCK") == 0 && strcmp(move2, "SCISSORS") == 0) || (strcmp(move1, "SCISSORS") == 0 && strcmp(move2, "PAPER") == 0) || (strcmp(move1, "PAPER") == 0 && strcmp(move2, "ROCK") == 0)) {
        return "W";
    } else {
        return "L";
    }
}

void parse_player(char *message, char *name) {
    char *start = strchr(message, '|');
    start += 1;
    char *end = strchr(start, '|');
    size_t len = end - start;
    strncpy(name, start, len);
    name[len] = '\0';
}

void send_message(int sock, char *type, char *arg1, char *arg2) {
    char message[1024];
    snprintf(message, sizeof(message), "%s|%s|%s||", type, arg1, arg2);
    send(sock, message, strlen(message), 0);
}

void handle_game(int fd1, char *name1, int fd2, char *name2) {
    
}

void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE];
    ssize_t bytes;

    const char *welcome = "W|1||";
    send(client_fd, welcome, strlen(welcome), 0);

    //REMOVE THIS PRINT
    printf("Handled client (fd: %d)\n", client_fd);
    bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    buffer[bytes] = '\0';
    
    if (buffer[0] != 'P') {
        close(client_fd);
        exit(1);
    }

    char pname[200];
    parse_player(buffer, pname);

    if (count >= 1024) {
        close(client_fd);
        exit(1);
    }

    players_queue[count].fd = client_fd;
    strncpy(players_queue[count].name, pname, 200);
    count += 1;

    if (count >= 2) {
        int fd1 = players_queue[0].fd;
        int fd2 = players_queue[1].fd;
        char name1[200], name2[200];
        strncpy(name1, players_queue[0].name, 200);
        strncpy(name2, players_queue[1].name, 200);

        for (int i = 2; i < count; i++) {
            players_queue[i-2] = players_queue[i];
        }
        count -= 2;

        pid_t pid = fork();
        if (pid == 0) {
            //handle_game()
        } 
        else if (pid > 0) {
            close(fd1);
            close(fd2);
            exit(0);
        }
        else {
            perror("fork");
            close(fd1);
            close(fd2);
            exit(1);
        }
    }

    //Read P|name|| from client and store it
    //then: Add this client to a shared queue for pairing
    //if another player is waiting: fork/exec game handler process
    //otherwise: keep client waiting (block until match available)

    close(client_fd); //THIS IS TEMPORARY
    exit(0);
}

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    int server_fd = setup_server_socket(port);
    printf("rpsd server listening on port %d...\n", port);

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) {
            perror("accept");
            continue;
        }

        printf("New client connected: %s:%d\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port));

        pid_t pid = fork();
        if (pid == 0) {
            close(server_fd);
            handle_client(client_fd);
        } else if (pid > 0) {
            close(client_fd);
        } else {
            perror("fork");
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
