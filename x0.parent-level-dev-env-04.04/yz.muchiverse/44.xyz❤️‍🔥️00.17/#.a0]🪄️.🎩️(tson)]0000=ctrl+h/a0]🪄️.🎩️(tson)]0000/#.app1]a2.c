#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    char input[256];
    
    while (1) {
    system("clear");
        printf("\n=== App1 ===\n");
        printf("Options:\n");
        printf("0. Say hello\n");
        printf("1. Launch App2\n");
        printf("2. Back to home\n");
        printf("Choice: ");
        
        fgets(input, sizeof(input), stdin);
        int choice = atoi(input);

        if (choice == 0) {
            printf("Hello from App1!\n");
        }
        else if (choice == 1) {
            system("./+x/app2");
        }
        else if (choice == 2) {
            printf("Returning to home screen...\n");
            return 0;
        }
        else {
            printf("Invalid option\n");
        }
        
         usleep(100000);
    }
    
    return 0;
}
