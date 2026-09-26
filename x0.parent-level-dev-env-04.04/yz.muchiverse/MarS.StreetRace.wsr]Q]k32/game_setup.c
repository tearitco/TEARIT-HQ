#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

typedef enum {
    HUMAN,
    COMPUTER
} PlayerType;

typedef struct {
    char name[50];
    float cash;
    PlayerType type;
    bool ticker_on;
} Player;

bool file_exists(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp) {
        fclose(fp);
        return true;
    }
    return false;
}

void create_player_files(Player *player) {
    // Create player directory
    char player_dir[100];
    sprintf(player_dir, "players/%s", player->name);
    mkdir(player_dir, 0777);

    // Create player files
    char balance_sheet_path[100];
    sprintf(balance_sheet_path, "players/%s/balance_sheet.txt", player->name);
    FILE *balance_sheet_file = fopen(balance_sheet_path, "w");
    if (balance_sheet_file) {
        fprintf(balance_sheet_file, "My Balance Sheet:\n");
        fprintf(balance_sheet_file, "Cash (CD's)..\t\t%.2f\n", player->cash);
        fprintf(balance_sheet_file, "Other Assets....\t\t0.00\n");
        fprintf(balance_sheet_file, "Total Assets.....\t\t%.2f\n", player->cash);
        fprintf(balance_sheet_file, "Less Debt....\t\t-0.00\n");
        fprintf(balance_sheet_file, "Net Worth........\t\t%.2f\n", player->cash);
        fclose(balance_sheet_file);
    }

    char portfolio_path[100];
    sprintf(portfolio_path, "players/%s/portfolio.txt", player->name);
    FILE *portfolio_file = fopen(portfolio_path, "w");
    if (portfolio_file) {
        fprintf(portfolio_file, "STOCK & BOND PORTFOLIO LISTING FOR %s\n", player->name);
        fprintf(portfolio_file, "STOCK PORTFOLIO FOR %s (In U.S. Dollars)\n", player->name);
        fprintf(portfolio_file, "STOCK           SHARES      VALUE\n");
        fprintf(portfolio_file, "TOTAL STOCK PORTFOLIO VALUE: 0\n");
        fprintf(portfolio_file, "BOND PORTFOLIO FOR %s (In U.S. Dollars)\n", player->name);
        fprintf(portfolio_file, "BOND            VALUE\n");
        fprintf(portfolio_file, "%s OWNS NO BONDS ....", player->name);
        fclose(portfolio_file);
    }
}

void setup_game(Player players[], int *num_players, int *game_length, float *starting_cash) {
    printf("--- Game Setup ---\n");

    do {
        printf("Enter number of players (1-5): ");
        scanf("%d", num_players);
    } while (*num_players < 1 || *num_players > 5);

    printf("Enter game length (in years): ");
    scanf("%d", game_length);

    printf("Enter starting cash: ");
    scanf("%f", starting_cash);

    for (int i = 0; i < *num_players; i++) {
        printf("\n--- Player %d ---\n", i + 1);
        printf("Enter name: ");
        scanf("%s", players[i].name);

        int type;
        do {
            printf("Enter player type (0 for Human, 1 for Computer): ");
            scanf("%d", &type);
        } while (type < 0 || type > 1);
        players[i].type = (PlayerType)type;

        players[i].cash = *starting_cash;

        create_player_files(&players[i]);
    }
}

void save_starting_data(Player players[], int num_players, int game_length, float starting_cash) {
    FILE *fp = fopen("starting_data.txt", "w");
    if (fp == NULL) {
        printf("Error: Could not open starting_data.txt for writing.\n");
        return;
    }
    fprintf(fp, "num_players:%d\n", num_players);
    fprintf(fp, "game_length:%d\n", game_length);
    fprintf(fp, "starting_cash:%.2f\n", starting_cash);
    for (int i = 0; i < num_players; i++) {
        fprintf(fp, "player_%d_name:%s\n", i, players[i].name);
        fprintf(fp, "player_%d_type:%d\n", i, players[i].type);
    }
    fclose(fp);
}

int main() {
    Player players[5];
    int num_players;
    int game_length;
    float starting_cash;
    
    system("rm -rf players && mkdir players");
    
    system(">data/wsr_clock.txt");
    system(">data/active_entity.txt");
    
     system(">data/master_ledger.txt");
    system(">data/master_reader.txt");
    
       system("cp -r presets/data_data data/");
    
    
     system("./+x/setup_multiverse.+x");
    
    system("./+x/setup_governments.+x presets/starting_gov-list.txt");
    
    
      system("./+x/setup_corporations.+x");
      
      system("./+x/setup_corporations_stage2.+x");
      
     
  

    if (file_exists("starting_data.txt")) {
        printf("Found starting_data.txt, loading settings.\n");
        FILE *fp = fopen("starting_data.txt", "r");
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char *key = strtok(line, ":");
            char *value = strtok(NULL, "\n");
            if (key && value) {
                if (strcmp(key, "num_players") == 0) {
                    num_players = atoi(value);
                } else if (strcmp(key, "game_length") == 0) {
                    game_length = atoi(value);
                } else if (strcmp(key, "starting_cash") == 0) {
                    starting_cash = atof(value);
                } else if (strncmp(key, "player_", 7) == 0) {
                    int player_index;
                    char player_key[50];
                    sscanf(key, "player_%d_%s", &player_index, player_key);
                    if (strcmp(player_key, "name") == 0) {
                        strcpy(players[player_index].name, value);
                    } else if (strcmp(player_key, "type") == 0) {
                        players[player_index].type = (PlayerType)atoi(value);
                    }
                }
            }
        }
        fclose(fp);

        for (int i = 0; i < num_players; i++) {
            players[i].cash = starting_cash;
            create_player_files(&players[i]);
        }
    } else {
        printf("No starting_data.txt found, running interactive setup.\n");
        setup_game(players, &num_players, &game_length, &starting_cash);
        save_starting_data(players, num_players, game_length, starting_cash);
    }



    char command[1024];
    sprintf(command, "./+x/game.+x %d %d %.2f", num_players, game_length, starting_cash);
    for (int i = 0; i < num_players; i++) {
        char player_arg[100];
        sprintf(player_arg, " \"%s\" %d", players[i].name, players[i].type);
        strcat(command, player_arg);
    }

    printf("Executing: %s\n", command);
    system(command);

    return 0;
}
