#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int centisecond;
} GameTime;

GameTime g_time;

void signal_handler(int signo);
void load_clock_state();
void save_clock_state();
void update_time(long elapsed_ms);

void signal_handler(int signo) {
    if (signo == SIGINT || signo == SIGTERM) {
        save_clock_state();
        exit(0);
    }
}

void load_clock_state() {
    FILE *fp = fopen("data/wsr_clock.txt", "r");
    if (fp == NULL) {
        g_time.year = 2025;
        g_time.month = 1;
        g_time.day = 1;
        g_time.hour = 0;
        g_time.minute = 0;
        g_time.second = 0;
        g_time.centisecond = 0;
        return;
    }
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *key = strtok(line, ":");
        char *value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "year") == 0) g_time.year = atoi(value);
            else if (strcmp(key, "month") == 0) g_time.month = atoi(value);
            else if (strcmp(key, "day") == 0) g_time.day = atoi(value);
            else if (strcmp(key, "hour") == 0) g_time.hour = atoi(value);
            else if (strcmp(key, "minute") == 0) g_time.minute = atoi(value);
            else if (strcmp(key, "second") == 0) g_time.second = atoi(value);
            else if (strcmp(key, "centisecond") == 0) g_time.centisecond = atoi(value);
        }
    }
    fclose(fp);
}

void save_clock_state() {
    FILE *fp = fopen("data/wsr_clock.txt", "w");
    if (fp == NULL) {
        fprintf(stderr, "Error: Could not open data/wsr_clock.txt for writing.\n");
        return;
    }
    fprintf(fp, "year:%d\nmonth:%d\nday:%d\nhour:%d\nminute:%d\nsecond:%d\ncentisecond:%d\n",
            g_time.year, g_time.month, g_time.day, g_time.hour, g_time.minute, g_time.second, g_time.centisecond);
    fclose(fp);
}

void update_time(long elapsed_ms) {
    FILE *fp = fopen("data/setting.txt", "r");
    char *speed_str = "N/A";
    static char value[100];
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            char *key = strtok(line, ":");
            char *val = strtok(NULL, "\n");
            if (key && val && strcmp(key, "ticker_speed") == 0) {
                if (val[0] == ' ') val++;
                strcpy(value, val);
                speed_str = value;
                break;
            }
        }
        fclose(fp);
    }

    double game_centiseconds_per_real_millisecond = 0.0;
    if (strcmp(speed_str, "cent") == 0) {
        game_centiseconds_per_real_millisecond = 36000.0;
    } else if (strcmp(speed_str, "sec") == 0) {
        game_centiseconds_per_real_millisecond = 360.0;
    } else if (strcmp(speed_str, "min") == 0) {
        game_centiseconds_per_real_millisecond = 6.0;
    } else if (strcmp(speed_str, "hour") == 0) {
        game_centiseconds_per_real_millisecond = 0.1;
    } else if (strcmp(speed_str, "day") == 0) {
        game_centiseconds_per_real_millisecond = 0.004166666666666667;
    }

    long long game_centiseconds_to_add = (long long)(elapsed_ms * game_centiseconds_per_real_millisecond);
    g_time.centisecond += game_centiseconds_to_add;

    while (g_time.centisecond >= 100) {
        g_time.centisecond -= 100;
        g_time.second++;
        if (g_time.second >= 60) {
            g_time.second = 0;
            g_time.minute++;
            if (g_time.minute >= 60) {
                g_time.minute = 0;
                g_time.hour++;
                if (g_time.hour >= 24) {
                    g_time.hour = 0;
                    g_time.day++;
                    if (g_time.day > 30) {
                        g_time.day = 1;
                        g_time.month++;
                        if (g_time.month > 12) {
                            g_time.month = 1;
                            g_time.year++;
                        }
                    }
                }
            }
        }
    }
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Save PID to file
    FILE *pid_fp = fopen("data/wsr_clock.pid", "w");
    if (pid_fp == NULL) {
        fprintf(stderr, "Error: Could not open data/wsr_clock.pid for writing.\n");
        exit(1);
    }
    fprintf(pid_fp, "%d\n", getpid());
    fclose(pid_fp);

    load_clock_state();

    struct timespec last_time, current_time;
    clock_gettime(CLOCK_MONOTONIC, &last_time);

    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &current_time);
        long elapsed_ms = (current_time.tv_sec - last_time.tv_sec) * 1000 +
                         (current_time.tv_nsec - last_time.tv_nsec) / 1000000;
        last_time = current_time;

        if (elapsed_ms > 0) {
            update_time(elapsed_ms);
            save_clock_state();
        }

        usleep(300000); // Sleep for 0.3 seconds (300,000 microseconds)
    }

    return 0;
}
