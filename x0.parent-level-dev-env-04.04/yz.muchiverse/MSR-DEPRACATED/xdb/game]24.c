#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/select.h>
#include <termios.h>
#include <signal.h>
#include <stdarg.h>
#include <ctype.h>

// Enum for player type
typedef enum {
    HUMAN,
    COMPUTER
} PlayerType;

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int centisecond;
} GameTime;

// Basic data structures
typedef struct {
    char name[50];
    float price;
    int shares;
} Company;

typedef struct {
    char name[50];
    float cash;
    PlayerType type;
    bool ticker_on;
} Player;

typedef struct {
    float interest_rate;
    char news[10][100];
} Market;

// Append buffer for screen output
typedef struct {
    char *b;
    int len;
} abuf;

#define ABUF_INIT {NULL, 0}

GameTime g_time;
struct termios orig_termios;

// Function prototypes
void reset_terminal_mode();
void set_conio_terminal_mode();
void initialize_market(Market *market);
void initialize_companies(Company companies[], int num_companies);

bool file_exists(const char *filename);
void load_clock_state(GameTime *g_time);
char* get_setting(const char* setting_name);
int kbhit(void);
void signal_handler(int signo);
void buffer_print(abuf *ab, const char* format, ...);
void abAppend(abuf *ab, const char *s, int len);
void abFree(abuf *ab);
char* get_active_entity();
void display_portfolio_holdings(Player *player);
const char* find_symbol_in_line(const char* line);

void reset_terminal_mode() {
    tcsetattr(0, TCSANOW, &orig_termios);
    write(STDOUT_FILENO, "\x1b[?25h", 6); // Show cursor
    if (file_exists("data/wsr_clock.pid")) {
        system("kill $(cat data/wsr_clock.pid) 2>/dev/null");
        remove("data/wsr_clock.pid");
    }
}

void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(0, &orig_termios);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(0, TCSANOW, &new_termios);
    atexit(reset_terminal_mode);
}

int kbhit(void) {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(0, &fds);
    return select(1, &fds, NULL, NULL, &tv);
}

void abAppend(abuf *ab, const char *s, int len) {
    char *new = realloc(ab->b, ab->len + len);
    if (new == NULL) return;
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void abFree(abuf *ab) {
    free(ab->b);
}

void buffer_print(abuf *ab, const char* format, ...) {
    char temp[1024];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);
    abAppend(ab, temp, len);
}

char* get_setting(const char* setting_name) {
    FILE *fp = fopen("data/setting.txt", "r");
    if (fp == NULL) return "N/A";
    static char value[100];
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, ":");
        char *val = strtok(NULL, "\n");
        if (key && val && strcmp(key, setting_name) == 0) {
            if (val[0] == ' ') val++;
            strcpy(value, val);
            fclose(fp);
            return value;
        }
    }
    fclose(fp);
    return "N/A";
}

bool file_exists(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp) { fclose(fp); return true; }
    return false;
}

void load_clock_state(GameTime *g_time) {
    FILE *fp = fopen("data/wsr_clock.txt", "r");
    if (fp == NULL) {
        g_time->year = 2025;
        g_time->month = 1;
        g_time->day = 1;
        g_time->hour = 0;
        g_time->minute = 0;
        g_time->second = 0;
        g_time->centisecond = 0;
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, ":");
        char *value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "year") == 0) g_time->year = atoi(value);
            else if (strcmp(key, "month") == 0) g_time->month = atoi(value);
            else if (strcmp(key, "day") == 0) g_time->day = atoi(value);
            else if (strcmp(key, "hour") == 0) g_time->hour = atoi(value);
            else if (strcmp(key, "minute") == 0) g_time->minute = atoi(value);
            else if (strcmp(key, "second") == 0) g_time->second = atoi(value);
            else if (strcmp(key, "centisecond") == 0) g_time->centisecond = atoi(value);
        }
    }
    fclose(fp);
}



void initialize_market(Market *market) {
    market->interest_rate = 2.5;
    strcpy(market->news[0], "Tech stocks are soaring!");
    strcpy(market->news[1], "Oil prices are dropping.");
}

void initialize_companies(Company companies[], int num_companies) {
    for (int i = 0; i < num_companies; i++) {
        sprintf(companies[i].name, "Company %d", i + 1);
        companies[i].price = (rand() % 1000) / 10.0;
        companies[i].shares = 1000;
    }
}

void display_main_menu(Player *player, Market *market, char* choice_buffer) {
    abuf ab = ABUF_INIT;

    char balance_sheet_path[100];
    sprintf(balance_sheet_path, "players/%s/balance_sheet.txt", player->name);
    FILE *balance_sheet_file = fopen(balance_sheet_path, "r");
    if (balance_sheet_file) {
        char line[256];
        while (fgets(line, sizeof(line), balance_sheet_file)) {
            if (strstr(line, "Cash (CD's)..")) {
                sscanf(line, "Cash (CD's)..%%f", &player->cash);
            }
        }
        fclose(balance_sheet_file);
    }

    abAppend(&ab, "\x1b[?25l", 6); // Hide cursor
    abAppend(&ab, "\x1b[2J", 4);   // Clear screen
    abAppend(&ab, "\x1b[H", 3);    // Move to home

    buffer_print(&ab, "1. File | 2. Game Options | 3. Settings | 4. Help\n");
    buffer_print(&ab, "Active Entity Selected: %s\n", get_active_entity());
    buffer_print(&ab, "5. Select Player 6. My News 7. Entity Info 8. Select Corp. 9. Select Last 10. General\n");
    buffer_print(&ab, "11. Buy Stock 12. Buy/Sell 13. Financing 14. Sell Stock 15. Management 16. Other Trans\n");
    buffer_print(&ab, "Stock Symbol.... N/A Go\t\tStock Price.......Start Game!\n");
    buffer_print(&ab, "Controlled By.....Start Game!\t\tActing Now.......%s\n", player->name);

    load_clock_state(&g_time);
    char date_str[100];
    sprintf(date_str, "Date: %%02d/%%02d/%%d %%02d:%%02d:%%02d:%%02d",
            g_time.month, g_time.day, g_time.year, g_time.hour, g_time.minute, g_time.second, g_time.centisecond);
    char* ticker_speed = get_setting("ticker_speed");
    buffer_print(&ab, "%s\t\tDay Passage Speed: %s\n", date_str, ticker_speed);

    buffer_print(&ab, "Player: %s\n", player->name);
    buffer_print(&ab, "17. End Turn 18. Misc. Menu 19. Chart 20. Cheat 21. Add/Del. Select Fill Clear\n");
    buffer_print(&ab, "22. TICKER %s\n", player->ticker_on ? "[ON]" : "[OFF]");
    buffer_print(&ab, "----------------------------------\n");
    buffer_print(&ab, "My Balance Sheet:\n");
    buffer_print(&ab, "Cash (CD's)..\t\t%%.2f\n", player->cash);
    buffer_print(&ab, "Other Assets....\t\t0.00\n");
    buffer_print(&ab, "Total Assets.....\t\t%%.2f\n", player->cash);
    buffer_print(&ab, "Less Debt....\t\t-0.00\n");
    buffer_print(&ab, "Net Worth........\t\t%%.2f\n", player->cash);
    buffer_print(&ab, "----------------------------------------------\n");
    buffer_print(&ab, "FINANCIAL NEWS HEADLINES\n");
    buffer_print(&ab, "\n");
    buffer_print(&ab, "-Quick Search Functions:-\n");
    buffer_print(&ab, "23. Research Report\t\t24. List Portfolio Holdings\n");
    buffer_print(&ab, "25. List Futures Contracts\t\t26. Financial Profile\n");
    buffer_print(&ab, "27. List Put and Call Options\t\t28. Recall DB Search List\n");
    buffer_print(&ab, "----------------------------------\n");
    buffer_print(&ab, "Commodity Prices/Indexes/Indicators:\n");
    buffer_print(&ab, "Stock Index: Market closed\t\tPrime Rate: %%%.2f%%\n", market->interest_rate);
    buffer_print(&ab, "Long Bond: 7.25%%\t\tShort Bond: 6.50%%\n");
    buffer_print(&ab, "Spot Crude: $100.00\t\tSilver: $20.00\n");
    buffer_print(&ab, "Spot Gold: $1,200.00\t\tSpot Wheat: $5.00\n");
    buffer_print(&ab, "GDP Growth: 2.5%%\t\tSpot Corn: $5.00\n");
    buffer_print(&ab, "------------------------------------------\n");
    buffer_print(&ab, "\nEnter your choice: %s", choice_buffer);

    abAppend(&ab, "\x1b[?25h", 6); // Show cursor

    write(STDOUT_FILENO, ab.b, ab.len);
    abFree(&ab);
}


char* get_active_entity() {
    FILE *fp = fopen("data/active_entity.txt", "r");
    if (fp == NULL) return "N/A";
    static char entity[10];
    fgets(entity, sizeof(entity), fp);
    fclose(fp);
    // Remove newline character if present
    char *newline = strchr(entity, '\n');
    if (newline) *newline = '\0';
    return entity;
}

void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        reset_terminal_mode();
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) { printf("Usage: ./game num_players game_length starting_cash [player_name player_type]...\n"); return 1; }

    srand(time(NULL));
    set_conio_terminal_mode();
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Player players[5];
    int num_players = atoi(argv[1]);
    int game_length = atoi(argv[2]);
    float starting_cash = atof(argv[3]);
    Market market;
    Company companies[5];

    int arg_index = 4;
    for (int i = 0; i < num_players; i++) {
        strcpy(players[i].name, argv[arg_index++]);
        players[i].type = (PlayerType)atoi(argv[arg_index++]);
        players[i].cash = starting_cash;
        players[i].ticker_on = false; // All players start with ticker off
    }

    initialize_market(&market);
    initialize_companies(companies, 5);

    int current_player_index = 0;
    int last_day = 0;
    char choice_buffer[10] = {0};
    int choice_len = 0;
    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);

    bool game_running = true;
    bool needs_redraw = true;
    while (game_running) {
        Player *current_player = &players[current_player_index];

        bool input_received = false;
        if (kbhit()) {
            char c = getchar();
            input_received = true;
            if (c >= '0' && c <= '9' && choice_len < 9) {
                choice_buffer[choice_len++] = c;
                choice_buffer[choice_len] = '\0';
            } else if (c == 8 || c == 127) {
                if (choice_len > 0) {
                    choice_len--;
                    choice_buffer[choice_len] = '\0';
                }
            } else if (c == 10) {
                if (choice_len > 0) {
                    int choice = atoi(choice_buffer);
                    switch (choice) {
                        case 0: game_running = false; break;
                        case 1:
                            reset_terminal_mode();
                            system("./+x/file_submenu.+x");
                            set_conio_terminal_mode();
                        
                            break;
                        case 3:
                            reset_terminal_mode();
                            system("./+x/settings_submenu.+x");
                            set_conio_terminal_mode();
                            break;
                        case 5: {
                            FILE *fp = fopen("data/active_entity.txt", "w");
                            if (fp != NULL) {
                                fprintf(fp, "%s", current_player->name);
                                fclose(fp);
                            }
                            break;
                        }
                        case 8:
                            reset_terminal_mode();
                            system("./+x/select_entity.+x");
                            set_conio_terminal_mode();
                            break;
                        case 13: {
                            reset_terminal_mode();
                            char command[100];
                            sprintf(command, "./+x/financing.+x %s", current_player->name);
                            system(command);
                            set_conio_terminal_mode();
                            break;
                        }
                        case 17:
                            current_player_index = (current_player_index + 1) % num_players;
                            if (players[current_player_index].ticker_on && !file_exists("data/wsr_clock.pid")) {
                                system("./+x/wsr_clock.+x &");
                            } else if (!players[current_player_index].ticker_on && file_exists("data/wsr_clock.pid")) {
                                system("kill $(cat data/wsr_clock.pid) 2>/dev/null");
                                remove("data/wsr_clock.pid");
                            }
                            break;
                        case 22:
                            current_player->ticker_on = !current_player->ticker_on;
                            if (current_player->ticker_on && !file_exists("data/wsr_clock.pid")) {
                                system("./+x/wsr_clock.+x &");
                            } else if (!current_player->ticker_on && file_exists("data/wsr_clock.pid")) {
                                system("kill $(cat data/wsr_clock.pid) 2>/dev/null");
                                remove("data/wsr_clock.pid");
                            }
                            break;
                        case 24: {
                            reset_terminal_mode();
                            char command[100];
                            sprintf(command, "./+x/display_portfolio.+x %s", current_player->name);
                            system(command);
                            set_conio_terminal_mode();
                            break;
                        }
                        case 28:
                            reset_terminal_mode();
                            system("./+x/db_search.+x");
                            set_conio_terminal_mode();
                            break;
                    }
                    choice_len = 0;
                    choice_buffer[0] = '\0';
                }
            }
            needs_redraw = true;
        }

        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long elapsed_ms = (current_time.tv_sec - last_time.tv_sec) * 1000 +
                         (current_time.tv_nsec - last_time.tv_nsec) / 1000000;
        last_time = current_time;

        if (current_player->ticker_on && elapsed_ms > 0) {
            load_clock_state(&g_time);
            needs_redraw = true;
        }

        load_clock_state(&g_time);
        if (g_time.day != last_day) {
            last_day = g_time.day;
            needs_redraw = true;
        }

        if (needs_redraw) {
            display_main_menu(current_player, &market, choice_buffer);
            needs_redraw = false;
        }

        usleep(20000);
    }

    reset_terminal_mode();
    return 0;
}
