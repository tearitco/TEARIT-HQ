#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void save_game() {
    char save_name[100];
    char command[512];
    int valid_name = 0;

    while (!valid_name) {
        printf("Enter save name (alphanumeric, dash, underscore): ");
        scanf("%99s", save_name);

        if (strlen(save_name) == 0) {
            continue;
        }

        valid_name = 1;
        for (int i = 0; i < strlen(save_name); i++) {
            if (!isalnum(save_name[i]) && save_name[i] != '_' && save_name[i] != '-') {
                valid_name = 0;
                break;
            }
        }
        if (!valid_name) {
            printf("Invalid characters in save name.\n");
        }
    }

    // Create saves directory if it doesn't exist
    system("mkdir -p saves");

    // Create directory structure for the save
   
    //
    sprintf(command, "mkdir -p saves/%s/corporations", save_name);
    system(command);
    sprintf(command, "mkdir -p saves/%s/governments", save_name);
    system(command);
    
       sprintf(command, "mkdir -p saves/%s/players", save_name);
    system(command);
    
    //
      sprintf(command, "mkdir -p saves/%s/multiverse", save_name);
    system(command);
    
     //
     
      sprintf(command, "mkdir -p saves/%s/data", save_name);
    system(command);
    

    // Copy generated files
    sprintf(command, "cp -r corporations/generated saves/%s/corporations/", save_name);
    system(command);
    sprintf(command, "cp -r governments/generated saves/%s/governments/", save_name);
    system(command);
     // Copy players/ files
      sprintf(command, "cp -r players/ saves/%s/", save_name);
    system(command);
    //
       sprintf(command, "cp -r multiverse/ saves/%s/", save_name);
    system(command);
    
    
     sprintf(command, "cp -r data/ saves/%s/", save_name);
    system(command);


    // Zip the save directory
    sprintf(command, "cd saves && zip -r %s.zip %s && cd ..", save_name, save_name);
    system(command);

    // Delete the unzipped save directory
    sprintf(command, "rm -rf saves/%s", save_name);
    system(command);

    printf("Game saved as saves/%s.zip\n", save_name);
}

void load_game() {
    char save_files[100][256];
    int num_saves = 0;
    FILE *ls_pipe = popen("ls -1 saves/*.zip 2>/dev/null", "r");

    if (ls_pipe) {
        while (num_saves < 100 && fgets(save_files[num_saves], sizeof(save_files[0]), ls_pipe)) {
            save_files[num_saves][strcspn(save_files[num_saves], "\n")] = 0;
            num_saves++;
        }
        pclose(ls_pipe);
    }

    if (num_saves == 0) {
        printf("No save files found.\n");
        return;
    }

    printf("Available saves:\n");
    for (int i = 0; i < num_saves; i++) {
        char *display_name = strrchr(save_files[i], '/');
        display_name = display_name ? display_name + 1 : save_files[i];
        printf("%d. %s\n", i + 1, display_name);
    }

    int choice;
    printf("Enter save number to load (0 to cancel): ");
    scanf("%d", &choice);

    if (choice > 0 && choice <= num_saves) {
        char *full_path = save_files[choice - 1];
        char save_name[100];
        char command[512];

        char *start = strrchr(full_path, '/');
        if (start) {
            start++;
            char *end = strrchr(start, '.');
            if (end) {
                size_t length = end - start;
                strncpy(save_name, start, length);
                save_name[length] = '\0';

                printf("Loading %s...\n", save_name);

                // Unzip
                sprintf(command, "unzip -o saves/%s.zip -d saves/", save_name);
                system(command);
 system("rm -r corporations/generated/*");
                // Copy files
                sprintf(command, "cp -r saves/%s/corporations/generated/* corporations/generated/", save_name);
                system(command);
                
                 system("rm -r governments/generated/*");
                sprintf(command, "cp -r saves/%s/governments/generated/* governments/generated/", save_name);
                system(command);
                
                 system("rm -r players/*");
                
                   sprintf(command, "cp -r saves/%s/players/* players/", save_name);
                system(command);
                
                //
                 system("rm -r multiverse/*");
                
                   sprintf(command, "cp -r saves/%s/multiverse/* multiverse/", save_name);
                system(command);
                ///
                
                 system("rm -r data/*");
                
                   sprintf(command, "cp -r saves/%s/data/* data/", save_name);
                system(command);
                
                
            

                // Cleanup
                sprintf(command, "rm -rf saves/%s", save_name);
                system(command);

                // Create signal file for auto-restart
                FILE *fp = fopen(".load_successful", "w");
                if (fp) {
                    fclose(fp);
                }
            }
        }
    } else if (choice != 0) {
        printf("Invalid choice.\n");
    }
}

int main() {
  printf("\x1b[2J\x1b[H");
    int choice;
    do {
        printf("\n--- File Menu ---\n");
        printf("1. Save Game\n");
        printf("2. Load Game\n");
        printf("3. Back to Main Menu\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        switch (choice) {
            case 1:
                save_game();
                break;
            case 2:
                load_game();
                break;
            case 3:
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while (choice != 3);
    return 0;
}
