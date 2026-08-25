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
volatile sig_atomic_t keep_running = 1; // Flag to control main loop

// Function prototypes
void reset_terminal_mode();
void set_conio_terminal_mode();
void initialize_market(Market *market);
bool file_exists(const char *filename);
void load_clock_state(GameTime *g_time);
char* get_setting(const char* setting_name);
int kbhit(void);
void signal_handler(int signo);
void buffer_print(abuf *ab, const char* format, ...);
void abAppend(abuf *ab, const char *s, int len);
void abFree(abuf *ab);
char* get_active_entity();

void reset_terminal_mode() {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    write(STDOUT_FILENO, "\x1b[?25h", 6); // Show cursor
    if (file_exists("data/wsr_clock.pid")) {
        system("kill $(cat data/wsr_clock.pid) 2>/dev/null");
        remove("data/wsr_clock.pid");
    }
}

void set_conio_terminal_mode() {
    struct termios new_termios;
    tcgetattr(STDIN_FILENO, &orig_termios);
    new_termios = orig_termios;
    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);
    atexit(reset_terminal_mode);
}

int kbhit(void) {
    struct timeval tv = { 0L, 0L };
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
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

void display_main_menu(Player *player, Market *market, char* choice_buffer) {
    abuf ab = ABUF_INIT;

    char balance_sheet_path[100];
    sprintf(balance_sheet_path, "players/%s/balance_sheet.txt", player->name);
    FILE *balance_sheet_file = fopen(balance_sheet_path, "r");
    if (balance_sheet_file) {
        char line[256];
        while (fgets(line, sizeof(line), balance_sheet_file)) {
            if (strstr(line, "Cash (CD's)..")) {
                sscanf(line, "Cash (CD's)..%f", &player->cash);
            }
        }
        fclose(balance_sheet_file);
    }

    abAppend(&ab, "\x1b[?25l", 6); // Hide cursor
    abAppend(&ab, "\x1b[2J", 4);   // Clear screen
    abAppend(&ab, "\x1b[H", 3);    // Move to home
    buffer_print(&ab, "_______________________________________________________\n");
    buffer_print(&ab, "0.^] 1.File] 2.Game Options] 3.Settings] 4.Help] +] -] \n");
    buffer_print(&ab, "Active Entity Selected: %s\n", get_active_entity());
    buffer_print(&ab, "5.Select Player] 6.Culture] 7.Entity Info] 8.Select Corp.] 9.History] 10.General]\n");
    buffer_print(&ab, "11.Tools] 12.Buy/Sell] 13.Financing] 14.Private] 15.Management] 16.Other Trans]\n");
    buffer_print(&ab, "Stock Symbol.... N/A Go\t\tStock Price.......Start Game!\n");
    buffer_print(&ab, "Controlled By.....Start Game!\t\tActing Now.......%s\n", player->name);

    load_clock_state(&g_time);
    char date_str[100];
    sprintf(date_str, "Date: %02d/%02d/%d %02d:%02d:%02d:%02d",
            g_time.month, g_time.day, g_time.year, g_time.hour, g_time.minute, g_time.second, g_time.centisecond);
    char* ticker_speed = get_setting("ticker_speed");
    buffer_print(&ab, "%s\t\tDay Passage Speed: %s\n", date_str, ticker_speed);

    buffer_print(&ab, "Player: %s\n", player->name);
    buffer_print(&ab, "17.End Turn] 18.Misc. Menu] 19.Chart] 20.Auto] 21.Watchlist]\n");
    buffer_print(&ab, "22.TICKER %s\n", player->ticker_on ? "[ON]" : "[OFF]");
    buffer_print(&ab, "----------------------------------\n");
    buffer_print(&ab, "My Balance Sheet:\n");
    buffer_print(&ab, "Cash (CD's)..\t\t%.2f\n", player->cash);
    buffer_print(&ab, "Other Assets....\t\t0.00\n");
    buffer_print(&ab, "Total Assets.....\t\t%.2f\n", player->cash);
    buffer_print(&ab, "Less Debt....\t\t-0.00\n");
    buffer_print(&ab, "Net Worth........\t\t%.2f\n", player->cash);
    buffer_print(&ab, "----------------------------------------------\n");
    
    // Read and display financial news from data/news.txt
    FILE *news_fp = fopen("data/news.txt", "r");
    if (news_fp != NULL) {
        char line[256];
        int line_count = 0;
        char news_items[10][256];  // Store up to 10 news items
        int news_count = 0;
        
        // Read the news file and extract news items (skip header lines)
        while (fgets(line, sizeof(line), news_fp) != NULL && news_count < 10 && line_count < 100) {
            // Skip header lines (first 4 lines) and separator lines
            if (line_count >= 4 && strlen(line) > 10 && strncmp(line, "----", 4) != 0) {
                // Remove newline character
                line[strcspn(line, "\n")] = 0;
                
                // Only add lines that contain actual news (with ticker symbols)
                if (strlen(line) > 10 && strchr(line, '(') != NULL && strchr(line, ')') != NULL) {
                    strncpy(news_items[news_count], line, sizeof(news_items[news_count]) - 1);
                    news_items[news_count][sizeof(news_items[news_count]) - 1] = '\0';
                    news_count++;
                }
            }
            line_count++;
        }
        fclose(news_fp);
        
        // Display a simple news headline with one item, but scroll slowly
        if (news_count > 0) {
            static int current_news = 0;
            static int scroll_counter = 0;  // Counter to slow down scrolling
            
            // Only update the news item every 30 refreshes (much slower scrolling)
            if (scroll_counter % 30 == 0) {
                current_news = (current_news + 1) % news_count;
            }
            scroll_counter++;
            
            buffer_print(&ab, "FINANCIAL NEWS HEADLINES [%s]\n", news_items[current_news]);
        } else {
            buffer_print(&ab, "FINANCIAL NEWS HEADLINES [No significant news]\n");
        }
    } else {
        buffer_print(&ab, "FINANCIAL NEWS HEADLINES [News unavailable]\n");
    }
    
    buffer_print(&ab, "\n");
    buffer_print(&ab, "-Quick Search Functions:-\n");
    buffer_print(&ab, "23.Research Report] 24.List Portfolio] 25.List Futures]\n");
    buffer_print(&ab, "26.Financial Profile] 27.List Options] 28.DB Search]\n");
    buffer_print(&ab, "29.Earnings Report] 30.Shareholder List] 31.My Corporations]\n");
    buffer_print(&ab, "----------------------------------\n");
    buffer_print(&ab, "Commodity Prices/Indexes/Indicators:\n");
    buffer_print(&ab, "Stock Index: Market closed\t\tPrime Rate: %.2f%%\n", market->interest_rate);
    buffer_print(&ab, "Long Bond: 7.25%%\t\tShort Bond: 6.50%%\n");
    buffer_print(&ab, "Spot Crude: $100.00\t\tSilver: $20.00\n");
    buffer_print(&ab, "Spot Gold: $1,200.00\t\tSpot Wheat: $5.00\n");
    buffer_print(&ab, "GDP Growth: 2.5%%\t\tSpot Corn: $5.00\n");
    buffer_print(&ab, "------------------------------------------\n");
    buffer_print(&ab, "Enter your choice (or 'n' to quit): %s", choice_buffer);

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
    char *newline = strchr(entity, '\n');
    if (newline) *newline = '\0';
    return entity;
}

void signal_handler(int signo) {
    fprintf(stderr, "SIGINT received in game.c (pid %d)\n", getpid());
    keep_running = 0; // Signal main loop to exit
    reset_terminal_mode();
}

int main(int argc, char *argv[]) {
    if (argc < 4) { 
        printf("Usage: ./game num_players game_length starting_cash [player_name player_type]...\n"); 
        return 1; 
    }

    // Ensure SIGINT is not blocked
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    srand(time(NULL));
    set_conio_terminal_mode();
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    Player players[5];
    int num_players = atoi(argv[1]);
    int game_length = atoi(argv[2]);
    float starting_cash = atof(argv[3]);
    Market market;

    int arg_index = 4;
    for (int i = 0; i < num_players; i++) {
        strcpy(players[i].name, argv[arg_index++]);
        players[i].type = (PlayerType)atoi(argv[arg_index++]);
        players[i].cash = starting_cash;
        players[i].ticker_on = false;
    }

    initialize_market(&market);

    int current_player_index = 0;
    int last_day = 0;
    char choice_buffer[10] = {0};
    int choice_len = 0;
    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);

    bool needs_redraw = true;
    while (keep_running) {
        Player *current_player = &players[current_player_index];

        if (kbhit()) {
            char c = getchar();
            fprintf(stderr, "Input received: %d (pid %d)\n", c, getpid());
            if (c == 'n' || c == 'N') {
                keep_running = 0;
                break;
            } else if (c >= '0' && c <= '9' && choice_len < 9) {
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
                        case 0:
                            keep_running = 0;
                            break;
                        case 1:
                            reset_terminal_mode();
                            fprintf(stderr, "Running file_submenu.+x (pid %d)\n", getpid());
                            system("./+x/file_submenu.+x");
                            set_conio_terminal_mode();
                            needs_redraw = true;
                            break;
                        case 3:
                            reset_terminal_mode();
                            system("./+x/settings_submenu.+x");
                            set_conio_terminal_mode();
                            needs_redraw = true;
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
                            needs_redraw = true;
                            break;
                        case 13: {
                            reset_terminal_mode();
                            char command[100];
                            sprintf(command, "./+x/financing.+x %s", current_player->name);
                            system(command);
                            set_conio_terminal_mode();
                            needs_redraw = true;
                            break;
                        }
                        case 15:
                            reset_terminal_mode();
                            system("./+x/management.+x");
                            set_conio_terminal_mode();
                            needs_redraw = true;
                            break;
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
                            needs_redraw = true;
                            break;
                        }
                        case 28:
                            reset_terminal_mode();
                            system("./+x/db_search.+x");
                            set_conio_terminal_mode();
                            needs_redraw = true;
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
