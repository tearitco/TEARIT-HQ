#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
#include <time.h>
#include <stdarg.h>

#define MAX_PATH 4096
#define MAX_LINE 1024

// LSR Manager (v1.0)
// Responsibility: Mirror MSR game state to TPMOS Pieces and handle UI interaction.

char project_root[MAX_PATH] = ".";
char history_path[MAX_PATH] = "pieces/keyboard/history.txt";
char manager_state_path[MAX_PATH] = "projects/lsr/manager/state.txt";
char clock_path[MAX_PATH] = "data/wsr_clock.txt";
char pulse_path[MAX_PATH] = "pieces/display/frame_changed.txt";
char layout_path[MAX_PATH] = "pieces/display/current_layout.txt";

char choice_buffer[32] = "";
char active_entity[32] = "N/A";
char player_name[32] = "Player1";
char ticker_status[16] = "[OFF]";
char current_layout_name[64] = "lsr_main.chtpm";
char last_save_status[64] = "None";
float player_cash = 10000.0f;

void trigger_pulse() {
    FILE *f = fopen(pulse_path, "a");
    if (f) {
        fprintf(f, "P\n");
        fclose(f);
    }
}

void switch_layout(const char* name) {
    strncpy(current_layout_name, name, 63);
    FILE *f = fopen(layout_path, "w");
    if (f) {
        fprintf(f, "projects/lsr/layouts/%s\n", name);
        fclose(f);
    }
}

void update_state_file() {
    FILE *f = fopen(manager_state_path, "w");
    if (!f) return;

    char game_time[128] = "Date: 01/01/2026 00:00:00:00";
    FILE *cf = fopen(clock_path, "r");
    if (cf) {
        int y=2026, mo=1, d=1, h=0, mi=0, s=0, c=0;
        char line[256];
        while (fgets(line, sizeof(line), cf)) {
            if (strncmp(line, "year:", 5) == 0) y = atoi(line+5);
            else if (strncmp(line, "month:", 6) == 0) mo = atoi(line+6);
            else if (strncmp(line, "day:", 4) == 0) d = atoi(line+4);
            else if (strncmp(line, "hour:", 5) == 0) h = atoi(line+5);
            else if (strncmp(line, "minute:", 7) == 0) mi = atoi(line+7);
            else if (strncmp(line, "second:", 7) == 0) s = atoi(line+7);
            else if (strncmp(line, "centisecond:", 12) == 0) c = atoi(line+12);
        }
        fclose(cf);
        snprintf(game_time, 128, "Date: %02d/%02d/%d %02d:%02d:%02d:%02d", mo, d, y, h, mi, s, c);
    }

    fprintf(f, "active_entity=%s\n", active_entity);
    fprintf(f, "stock_symbol=N/A\n");
    fprintf(f, "stock_price=Start Game!\n");
    fprintf(f, "controlled_by=Start Game!\n");
    fprintf(f, "player_name=%s\n", player_name);
    fprintf(f, "game_time=%s\n", game_time);
    fprintf(f, "ticker_speed=Normal\n");
    fprintf(f, "ticker_status=%s\n", ticker_status);
    fprintf(f, "cash=%.2f\n", player_cash);
    fprintf(f, "other_assets=0.00\n");
    fprintf(f, "total_assets=%.2f\n", player_cash);
    fprintf(f, "debt=0.00\n");
    fprintf(f, "net_worth=%.2f\n", player_cash);
    fprintf(f, "news_headline=Welcome to LSR - Lunar Streetrace Raider!\n");
    fprintf(f, "last_save_status=%s\n", last_save_status);
    fprintf(f, "stock_index=Market closed\n");
    fprintf(f, "prime_rate=2.50%%\n");
    fprintf(f, "long_bond=7.25%%\n");
    fprintf(f, "short_bond=6.50%%\n");
    fprintf(f, "spot_crude=$100.00\n");
    fprintf(f, "spot_silver=$20.00\n");
    fprintf(f, "spot_gold=$1,200.00\n");
    fprintf(f, "spot_wheat=$5.00\n");
    fprintf(f, "gdp_growth=2.5%%\n");
    fprintf(f, "spot_corn=$5.00\n");
    fprintf(f, "choice_buffer=%s\n", choice_buffer);
    
    fclose(f);
}

void handle_input(int key) {
    if (key >= '0' && key <= '9') {
        size_t len = strlen(choice_buffer);
        if (len < 31) {
            choice_buffer[len] = (char)key;
            choice_buffer[len+1] = '\0';
        }
    } else if (key == 13 || key == 10) {
        int choice = atoi(choice_buffer);
        choice_buffer[0] = '\0';
        
        if (strcmp(current_layout_name, "lsr_main.chtpm") == 0) {
            if (choice == 1) switch_layout("lsr_file.chtpm");
            else if (choice == 0) exit(0);
        } else if (strcmp(current_layout_name, "lsr_file.chtpm") == 0) {
            if (choice == 3) switch_layout("lsr_main.chtpm");
            else if (choice == 1) strcpy(last_save_status, "Simulated Save Success");
        }
    } else if (key == 8 || key == 127) {
        size_t len = strlen(choice_buffer);
        if (len > 0) choice_buffer[len-1] = '\0';
    }
    update_state_file();
    trigger_pulse();
}

int main() {
    setbuf(stdout, NULL);
    update_state_file();
    trigger_pulse();

    long last_pos = 0;
    struct stat st;
    if (stat(history_path, &st) == 0) last_pos = st.st_size;

    while (1) {
        if (stat(history_path, &st) == 0) {
            if (st.st_size > last_pos) {
                FILE *hf = fopen(history_path, "r");
                if (hf) {
                    fseek(hf, last_pos, SEEK_SET);
                    char line[MAX_LINE];
                    while (fgets(line, sizeof(line), hf)) {
                        char *kpress = strstr(line, "KEY_PRESSED:");
                        if (kpress) {
                            int key = atoi(kpress + 12);
                            if (key > 0) handle_input(key);
                        }
                    }
                    last_pos = ftell(hf);
                    fclose(hf);
                }
            } else if (st.st_size < last_pos) {
                last_pos = 0;
            }
        }
        
        static int clock_check_counter = 0;
        if (clock_check_counter++ > 500) { // Pulse every 5 seconds instead of 0.5s
            update_state_file();
            trigger_pulse();
            clock_check_counter = 0;
        }
        usleep(10000);
    }
    return 0;
}
