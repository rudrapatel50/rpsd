#ifndef PLAYER_H
#define PLAYER_H

/**
 * @brief node structure for the linked list of active players
 * each player has a name, a file descriptor (fd),
 * an in-game status flag (in_game), and a pointer to the next player
 */
typedef struct PlayerNode {
    char name[200];
    int fd;
    int in_game;
    struct PlayerNode *next;
} PlayerNode;

//tracks all players currently connected to the server
extern PlayerNode *active_players;

PlayerNode *find_player_by_name(PlayerNode *head, const char *name);
PlayerNode *add_player(PlayerNode **head, const char *name, int fd);
void remove_player_by_name(PlayerNode **head, const char *name);
void send_logged_in_result(int fd);
void print_active_players(void);

#endif
