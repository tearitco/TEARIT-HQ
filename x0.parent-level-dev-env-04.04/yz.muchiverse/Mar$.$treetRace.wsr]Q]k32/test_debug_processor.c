#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

// Simple test of debug command functionality
int main() {
    printf("Debug command system test running...\n");
    printf("Reading debug_command_processed.txt: ");
    FILE *fp = fopen("debug_command_processed.txt", "r");
    if (fp) {
        int count;
        fscanf(fp, "%d", &count);
        printf("%d\n", count);
        fclose(fp);
    } else {
        printf("File not found, creating with 0\n");
        fp = fopen("debug_command_processed.txt", "w");
        fprintf(fp, "0");
        fclose(fp);
    }
    
    printf("Current commands:\n");
    fp = fopen("debug_commands.txt", "r");
    if (fp) {
        char line[256];
        int line_num = 1;
        while (fgets(line, sizeof(line), fp)) {
            printf("%2d: %s", line_num++, line);
        }
        fclose(fp);
    }
    
    printf("\nTest completed.\n");
    return 0;
}
