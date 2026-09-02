/* game_init.c - Initialize game setup screen (FIXED VERSION)
 * Reads config.txt, displays setup menu, waits for user input
 * Output: writes to config.txt
 * Self-contained, no shared headers */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_PATH 256

int main(int argc, char **argv) {
    char config_path[MAX_PATH] = "data/config.txt";
    if (argc > 1) snprintf(config_path, sizeof(config_path), "%s", argv[1]);

    printf("\x1b[2J\x1b[H");  /* Clear screen */
    printf("=========================================\n");
    printf("  LEDGER-DRIVEN WORD GAME\n");
    printf("  Testing: Multiplayer + Ring + Ledger\n");
    printf("=========================================\n\n");

    printf("Welcome! Let's set up your game.\n\n");

    printf("How many players? (1-4): ");
    fflush(stdout);

    int num_players = 0;
    scanf("%d", &num_players);
    if (num_players < 1 || num_players > 4) num_players = 1;

    printf("\n");

    /* Collect player info */
    char player_types[5][20] = {"", "human", "human", "human", "human"};
    char player_names[5][50] = {"", "Player1", "Player2", "Player3", "Player4"};

    for (int i = 1; i <= num_players; i++) {
        printf("Player %d - Human (0) or Computer (1)? [0]: ", i);
        fflush(stdout);
        int player_type;
        if (scanf("%d", &player_type) != 1) player_type = 0;
        if (player_type != 0 && player_type != 1) player_type = 0;

        strcpy(player_types[i], player_type == 0 ? "human" : "computer");

        printf("Player %d - Enter name [Player%d]: ", i, i);
        fflush(stdout);
        char name[50] = "";
        getchar();  /* consume newline from scanf */
        if (fgets(name, sizeof(name), stdin)) {
            name[strcspn(name, "\n")] = '\0';
        }
        if (strlen(name) == 0) {
            snprintf(name, sizeof(name), "Player%d", i);
        }
        strncpy(player_names[i], name, sizeof(player_names[i]) - 1);

        printf("  -> %s (%s)\n", player_names[i], player_types[i]);
    }

    printf("\nHow many NPCs? (0-4) [0]: ");
    fflush(stdout);
    int num_npcs = 0;
    if (scanf("%d", &num_npcs) == 1) {
        if (num_npcs < 0) num_npcs = 0;
        if (num_npcs > 4) num_npcs = 4;
    }

    /* Collect NPC info */
    for (int i = 1; i <= num_npcs; i++) {
        int npc_id = num_players + i;
        printf("NPC %d - Enter name [NPC%d]: ", i, npc_id);
        fflush(stdout);
        char name[50] = "";
        getchar();  /* consume newline from scanf */
        if (fgets(name, sizeof(name), stdin)) {
            name[strcspn(name, "\n")] = '\0';
        }
        if (strlen(name) == 0) {
            snprintf(name, sizeof(name), "NPC%d", npc_id);
        }
        strncpy(player_names[npc_id], name, sizeof(player_names[npc_id]) - 1);
        strcpy(player_types[npc_id], "computer");

        printf("  -> %s (NPC/auto)\n", player_names[npc_id]);
    }

    printf("\nHow many epochs? (1-10) [5]: ");
    fflush(stdout);
    int epoch_length = 5;
    if (scanf("%d", &epoch_length) == 1) {
        if (epoch_length < 1) epoch_length = 1;
        if (epoch_length > 10) epoch_length = 10;
    }

    /* Write complete config file */
    FILE *fp = fopen(config_path, "w");
    if (!fp) {
        fprintf(stderr, "Error: cannot write config\n");
        return 1;
    }

    int total_actors = num_players + num_npcs;
    fprintf(fp, "num_players=%d\n", total_actors);  /* total actors (players + NPCs) */
    fprintf(fp, "num_human_players=%d\n", num_players);
    fprintf(fp, "num_npcs=%d\n", num_npcs);
    fprintf(fp, "epoch_length=%d\n", epoch_length);
    fprintf(fp, "current_epoch=1\n");
    fprintf(fp, "current_turn=0\n");
    fprintf(fp, "game_state=playing\n");
    fprintf(fp, "last_input=\n");

    for (int i = 1; i <= 8; i++) {
        fprintf(fp, "player_%d_type=%s\n", i, player_types[i]);
        fprintf(fp, "player_%d_name=%s\n", i, player_names[i]);
    }

    fprintf(fp, "show_ledger=0\n");
    fprintf(fp, "menu_active=0\n");

    fclose(fp);

    printf("\n=========================================\n");
    printf("Starting game:\n");
    printf("  %d human players\n", num_players);
    printf("  %d NPCs (auto-play)\n", num_npcs);
    printf("  %d epochs\n", epoch_length);
    printf("=========================================\n\n");

    return 0;
}
