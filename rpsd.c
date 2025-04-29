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
#include "player.h"

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
    if (sockfd == -1) {
        error_exit("socket");
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        error_exit("bind");
    }

    if (listen(sockfd, BACKLOG) == -1) {
        error_exit("listen");
    }

    return sockfd;
}

char find_winner(char *move1, char *move2) {
    if (strcmp(move1, move2) == 0) {
        return 'D';
    }
    if ((strcmp(move1, "ROCK") == 0 && strcmp(move2, "SCISSORS") == 0) ||
        (strcmp(move1, "SCISSORS") == 0 && strcmp(move2, "PAPER") == 0) ||
        (strcmp(move1, "PAPER") == 0 && strcmp(move2, "ROCK") == 0)) {
        return 'W';
    } else {
        return 'L';
    }
}

void parse_move(char *message, char *move) {
    char *start = strchr(message, '|');
    if (start == NULL) {
        return;
    }

    start++;
    char *end = strchr(start, '|');

    if (end == NULL) {
        return;
    }

    size_t len = end - start;
    strncpy(move, start, len);
    move[len] = '\0';
}

void handle_game(int fd1, char *name1, int fd2, char *name2) {
    ssize_t bytes1, bytes2;
    char buffer1[BUFFER_SIZE], buffer2[BUFFER_SIZE];

    printf("[DEBUG] Game started: %s vs %s \n", name1, name2);
    fflush(stdout);

    while (1) {
        char begin1[300], begin2[300];
        snprintf(begin1, sizeof(begin1), "B|%s||", name2);
        snprintf(begin2, sizeof(begin2), "B|%s||", name1);
        send(fd1, begin1, strlen(begin1), 0);
        send(fd2, begin2, strlen(begin2), 0);

        bytes1 = recv(fd1, buffer1, sizeof(buffer1) - 1, 0);
        if (bytes1 <= 0) {
            printf("[DEBUG] %s disconnected (forfeit)\n", name1);
            send(fd2, "R|F|||", strlen("R|F|||"), 0);
            break;
        }
        buffer1[bytes1] = '\0';
        
        bytes2 = recv(fd2, buffer2, sizeof(buffer2) - 1, 0);
        if (bytes2 <= 0) {
            printf("[DEBUG] %s disconnected (forfeit)\n", name2);
            send(fd1, "R|F|||", strlen("R|F|||"), 0);
            break;
        }
        buffer2[bytes2] = '\0';

        char move1[50], move2[50];
        parse_move(buffer1, move1);
        parse_move(buffer2, move2);

        printf("[DEBUG] %s (%s) vs %s (%s)\n", name1, move1, name2, move2);
        fflush(stdout);

        char result = find_winner(move1, move2);
        char result1, result2;
        if (result == 'W') {
            result1 = 'W';
            result2 = 'L';
        } else if (result == 'L') {
            result1 = 'L';
            result2 = 'W';
        } else {
            result1 = result2 = 'D';
        }

        char msg1[300], msg2[300];
        snprintf(msg1, sizeof(msg1), "R|%c|%s||", result1, move2);
        snprintf(msg2, sizeof(msg2), "R|%c|%s||", result2, move1);
        send(fd1, msg1, strlen(msg1), 0);
        send(fd2, msg2, strlen(msg2), 0);

        bytes1 = recv(fd1, buffer1, sizeof(buffer1) - 1, 0);
        bytes2 = recv(fd2, buffer2, sizeof(buffer2) - 1, 0);

        if (bytes1 > 0) {
            buffer1[bytes1] = '\0';
        } else {
            buffer1[0] = '\0';
        }

        if (bytes2 > 0) {
            buffer2[bytes2] = '\0';
        } else {
            buffer2[0] = '\0';
        }

        if ((bytes1 > 0 && buffer1[0] == 'Q') || (bytes2 > 0 && buffer2[0] == 'Q')) {
            break;
        }

        if (buffer1[0] == 'C' && buffer2[0] == 'C') {
            continue;
        }
        else {
            break;
        }
    }
    remove_player_by_name(&active_players, name1);
    remove_player_by_name(&active_players, name2);

    close(fd1);
    close(fd2);
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

        char buffer[BUFFER_SIZE];
        ssize_t bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            close(client_fd);
            continue;
        }
        buffer[bytes] = '\0';

        if (buffer[0] != 'P') {
            close(client_fd);
            continue;
        }

        const char *welcome = "W|1||";
        send(client_fd, welcome, strlen(welcome), 0);

        char pname[200];
        parse_move(buffer, pname);

        if (find_player_by_name(active_players, pname) != NULL) {
            send_logged_in_result(client_fd);
            continue;
        }

        add_player(&active_players, pname, client_fd);

        players_queue[count].fd = client_fd;
        strncpy(players_queue[count].name, pname, 200);
        count++;

        if (count >= 2) {
            int fd1 = players_queue[0].fd;
            int fd2 = players_queue[1].fd;
            char name1[200], name2[200];
            strncpy(name1, players_queue[0].name, 200);
            strncpy(name2, players_queue[1].name, 200);

            for (int i = 2; i < count; i++) {
                players_queue[i - 2] = players_queue[i];
            }
            count -= 2;

            pid_t pid = fork();
            if (pid == 0) {
                printf("[DEBUG] Match: %s vs %s\n", name1, name2);
                fflush(stdout);
                handle_game(fd1, name1, fd2, name2);
                exit(0);
            } else if (pid > 0) {
                close(fd1);
                close(fd2);
        
                remove_player_by_name(&active_players, name1);
                remove_player_by_name(&active_players, name2);
            } else {
                perror("fork");
                close(fd1);
                close(fd2);
            }
        }
    }

    close(server_fd);
    return 0;
}