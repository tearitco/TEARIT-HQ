#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

// Structure to hold schedule entry
typedef struct {
    char interval[16];
    char command[256];
} ScheduleEntry;

// Function to parse timestamp from file
int parse_timestamp(const char *filename, Timestamp *ts) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error opening data/wsr_clock.txt");
        return 0;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
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

// Function to parse schedule from presets/schedule.txt
int parse_schedule(ScheduleEntry *schedule, int max_entries) {
    FILE *file = fopen("presets/schedule.txt", "r");
    if (!file) {
        perror("Error opening presets/schedule.txt");
        return 0;
    }

    char line[512];
    int count = 0;
    while (fgets(line, sizeof(line), file) && count < max_entries) {
        char *interval = strtok(line, " ");
        char *command = strtok(NULL, "\n");
        if (interval && command) {
            strncpy(schedule[count].interval, interval, sizeof(schedule[count].interval) - 1);
            schedule[count].interval[sizeof(schedule[count].interval) - 1] = '\0';
            strncpy(schedule[count].command, command, sizeof(schedule[count].command) - 1);
            schedule[count].command[sizeof(schedule[count].command) - 1] = '\0';
            count++;
        }
    }

    fclose(file);
    return count;
}

// Function to execute command based on interval
void execute_command(const char *interval, const ScheduleEntry *schedule, int schedule_count) {
    for (int i = 0; i < schedule_count; i++) {
        if (strcmp(schedule[i].interval, interval) == 0) {
            printf("Executing: %s\n", schedule[i].command);
            system(schedule[i].command);
        }
    }
}

int main() {
    Timestamp current = {0}, prev = {-1, -1, -1, -1, -1, -1, -1};
    ScheduleEntry schedule[10];
    int schedule_count = parse_schedule(schedule, 10);

    if (schedule_count == 0) {
        fprintf(stderr, "No valid schedule entries found\n");
        return 1;
    }

    // Main loop to check for epoch updates
    while (1) {
        if (!parse_timestamp("data/wsr_clock.txt", &current)) {
            break;
        }

        // Write to daily file and execute daily command if day has changed
        if (prev.day != current.day || prev.month != current.month || prev.year != current.year) {
            write_timestamp("data/clockwise_daily.txt", &current);
            execute_command("1_day", schedule, schedule_count);
        }

        // Write to monthly file if month has changed
        if (prev.month != current.month || prev.year != current.year) {
            write_timestamp("data/clockwise_monthly.txt", &current);
        }

        // Write to quarterly file and execute quarterly command if quarter has changed
        int prev_quarter = prev.month == -1 ? -1 : (prev.month - 1) / 3 + 1;
        int current_quarter = (current.month - 1) / 3 + 1;
        if (prev_quarter != current_quarter || prev.year != current.year) {
            write_timestamp("data/clockwise_quarterly.txt", &current);
            execute_command("3_months", schedule, schedule_count);
        }

        // Execute yearly command if year has changed
        if (prev.year != current.year) {
            execute_command("1_year", schedule, schedule_count);
        }

        // Update previous timestamp
        prev = current;

        // Sleep briefly to avoid excessive CPU usage
        usleep(100000); // 100ms
    }

    return 0;
}
