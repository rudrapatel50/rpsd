#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "player.h"

PlayerNode *active_players = NULL;

/**
 * @brief find a player in the linked list by their name
 * @param head pointer to the head of the linked list
 * @param name the name of the player to search for
 * @return pointer to the found PlayerNode, or NULL if not found
 */
PlayerNode *find_player_by_name(PlayerNode *head, const char *name) {
    while (head) {
        if (strcmp(head->name, name) == 0) {
            return head;
        }
        head = head->next;
    }
    return NULL;
}

/**
 * @brief add a new player to the linked list of active players
 * @param head double pointer to the head of the linked list
 * @param name the name of the new playe
 * @param fd the file descriptor associated with the player
 * @return pointer to the newly created PlayerNode, or NULL if memory allocation fails
 */
PlayerNode *add_player(PlayerNode **head, const char *name, int fd) {
    PlayerNode *new_node = malloc(sizeof(PlayerNode));
    if (!new_node) { 
        return NULL;
    }

    strncpy(new_node->name, name, sizeof(new_node->name) - 1);
    new_node->name[sizeof(new_node->name) - 1] = '\0';
    new_node->fd = fd;
    new_node->in_game = 0;
    new_node->next = *head;
    *head = new_node;
    return new_node;
}

/**
 * @brief remove a player from the linked list by their name
 * @param head double pointer to the head of the linked list
 * @param name the name of the player to remove
 */
void remove_player_by_name(PlayerNode **head, const char *name) {
    PlayerNode *curr = *head;
    PlayerNode *prev = NULL;

    while (curr != NULL) {
        if (strcmp(curr->name, name) == 0) {
            if (prev != NULL) {
                prev->next = curr->next;
            } else {
                *head = curr->next;
            }
            free(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

/**
 * @brief send a result message to a client indicating they are already logged in
 * sends a result message with "Logged in" and closes the client's socket
 * @param fd the file descriptor of the client
 */
void send_logged_in_result(int fd) {
    const char *msg = "R|rock|Logged in||\n";
    write(fd, msg, strlen(msg));
    close(fd);
}

/**
 * @brief print the current list of active players to standard output
 * used mainly for debugging purposes
 */
void print_active_players(void) {
    printf("[DEBUG] Active players list:\n");

    PlayerNode *curr = active_players;
    if (curr == NULL) {
        printf("(empty)\n");
    }

    while (curr != NULL) {
        printf("- %s (fd=%d, in_game=%d)\n", curr->name, curr->fd, curr->in_game);
        curr = curr->next;
    }
}