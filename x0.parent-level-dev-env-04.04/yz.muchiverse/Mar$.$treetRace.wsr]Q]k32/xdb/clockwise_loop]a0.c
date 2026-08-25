#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 256

// Structure to hold timestamp
typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int centisecond;
} Timestamp;

// Function to parse timestamp from file
int parse_timestamp(const char *filename, Timestamp *ts) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening wsr_clock.txt");
        return 0;
    }

    char line[MAX_LINE];
    while (fgets(line, MAX_LINE, file)) {
        char *key = strtok(line, ":");
        char *value = strtok(NULL, "\n");
        if (key && value) {
            if (strcmp(key, "year") == 0) ts->year = atoi(value);
            else if (strcmp(key, "month") == 0) ts->month = atoi(value);
            else if (strcmp(key, "day") == 0) ts->day = atoi(value);
            else if (strcmp(key, "hour") == 0) ts->hour = atoi(value);
            else if (strcmp(key, "minute") == 0) ts->minute = atoi(value);
            else if (strcmp(key, "second") == 0) ts->second = atoi(value);
            else if (strcmp(key, "centisecond") == 0) ts->centisecond = atoi(value);
        }
    }

    fclose(file);
    return 1;
}

// Function to write timestamp to file
void write_timestamp(const char *filename, const Timestamp *ts) {
    FILE *file = fopen(filename, "a");
    if (!file) {
        perror("Error opening output file");
        return;
    }

    fprintf(file, "%04d-%02d-%02d %02d:%02d:%02d.%02d\n",
            ts->year, ts->month, ts->day,
            ts->hour, ts->minute, ts->second, ts->centisecond);
    fclose(file);
}

int main() {

system(">data/clockwise_daily.txt");
system(">data/clockwise_monthly.txt");
system(">data/clockwise_quarterly.txt");


    Timestamp current = {0}, prev = {-1, -1, -1, -1, -1, -1, -1};
    
    // Main loop to check for epoch updates
    while (1) {
        if (!parse_timestamp("data/wsr_clock.txt", &current)) {
            break;
        }

        // Write to daily file if day has changed
        if (prev.day != current.day || prev.month != current.month || prev.year != current.year) {
            write_timestamp("data/clockwise_daily.txt", &current);
        }

        // Write to monthly file if month has changed
        if (prev.month != current.month || prev.year != current.year) {
            write_timestamp("data/clockwise_monthly.txt", &current);
        }

        // Write to quarterly file if quarter has changed
        int prev_quarter = prev.month == -1 ? -1 : (prev.month - 1) / 3 + 1;
        int current_quarter = (current.month - 1) / 3 + 1;
        if (prev_quarter != current_quarter || prev.year != current.year) {
            write_timestamp("data/clockwise_quarterly.txt", &current);
        }

        // Update previous timestamp
        prev = current;

        // Sleep briefly to avoid excessive CPU usage (adjust as needed)
        usleep(100000); // 100ms
    }

    return 0;
}
