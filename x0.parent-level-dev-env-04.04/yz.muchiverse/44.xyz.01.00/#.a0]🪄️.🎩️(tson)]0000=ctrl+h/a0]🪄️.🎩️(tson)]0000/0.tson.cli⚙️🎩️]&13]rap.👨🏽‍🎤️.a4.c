#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#define MAX_APPS 100
#define MAX_NAME 256
#define DIR_PATH "./+x/"
#define HASH_SIZE 32
#define SUBKEY_SIZE 8
#define PROC_HASH_SIZE 4 // 4 bytes = 8 hex digits

char* current_subkey = NULL;

void cleanup_children(int sig) {
    char subkey[SUBKEY_SIZE + 1];
    snprintf(subkey, sizeof(subkey), "guest");
    if (current_subkey) {
        strncpy(subkey, current_subkey, SUBKEY_SIZE + 1);
    }

    char proc_file[MAX_NAME];
    snprintf(proc_file, MAX_NAME, "user.%s/procman.%s.txt", subkey, subkey);

    FILE* fp = fopen(proc_file, "r");
    if (!fp) {
        exit(sig == SIGINT ? 1 : 0);
    }

    char line[MAX_NAME * 2];
    while (fgets(line, sizeof(line), fp) != NULL) {
        char proc_hash[9];
        pid_t pid;
        char command[MAX_NAME * 2];
        if (sscanf(line, "%8s %d %[^\n]", proc_hash, &pid, command) == 3) {
            if (kill(pid, SIGTERM) == 0) {
                printf("DEBUG: Killed PID %d\n", pid);
            } else if (errno != ESRCH) {
                fprintf(stderr, "Failed to kill PID %d: %s\n", pid, strerror(errno));
            }
        }
    }
    fclose(fp);

    usleep(100000);
    exit(sig == SIGINT ? 1 : 0);
}

void generate_random_hash(unsigned char* hash, int size) {
    if (RAND_bytes(hash, size) != 1) {
        fprintf(stderr, "Failed to generate random bytes: %s\n", strerror(errno));
        exit(1);
    }
}

int generate_key_pair(const char* subkey, unsigned char* hash) {
    char dir[MAX_NAME], pub_file[MAX_NAME], priv_file[MAX_NAME];
    snprintf(dir, MAX_NAME, "user.%s", subkey);
    snprintf(pub_file, MAX_NAME, "%s/pub.%s.txt", dir, subkey);
    snprintf(priv_file, MAX_NAME, "%s/priv.%s.txt", dir, subkey);

    printf("DEBUG: Creating directory %s\n", dir);
    if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
        fprintf(stderr, "Failed to create user directory: %s\n", strerror(errno));
        return 0;
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) {
        fprintf(stderr, "Failed to create EVP_PKEY_CTX: %s\n", ERR_error_string(ERR_get_error(), NULL));
        return 0;
    }

    EVP_PKEY* pkey = NULL;
    FILE* fp;

    printf("DEBUG: Initializing key generation\n");
    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        fprintf(stderr, "Failed to initialize keygen: %s\n", ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }

    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) {
        fprintf(stderr, "Failed to set keygen bits: %s\n", ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        fprintf(stderr, "Failed to generate key: %s\n", ERR_error_string(ERR_get_error(), NULL));
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }

    printf("DEBUG: Writing public key to %s\n", pub_file);
    fp = fopen(pub_file, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open public key file: %s\n", strerror(errno));
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }
    if (PEM_write_PUBKEY(fp, pkey) <= 0) {
        fprintf(stderr, "Failed to write public key: %s\n", ERR_error_string(ERR_get_error(), NULL));
        fclose(fp);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }
    fclose(fp);

    printf("DEBUG: Writing private key to %s\n", priv_file);
    fp = fopen(priv_file, "w");
    if (!fp) {
        fprintf(stderr, "Failed to open private key file: %s\n", strerror(errno));
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }
    if (PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL) <= 0) {
        fprintf(stderr, "Failed to write private key: %s\n", ERR_error_string(ERR_get_error(), NULL));
        fclose(fp);
        EVP_PKEY_free(pkey);
        EVP_PKEY_CTX_free(ctx);
        return 0;
    }
    fclose(fp);

    EVP_PKEY_free(pkey);
    EVP_PKEY_CTX_free(ctx);
    return 1;
}

int verify_key_pair(const char* subkey, const char* priv_key_path) {
    char pub_file[MAX_NAME];
    snprintf(pub_file, MAX_NAME, "user.%s/pub.%s.txt", subkey, subkey);

    printf("DEBUG: Opening public key %s\n", pub_file);
    FILE* pub_fp = fopen(pub_file, "r");
    if (!pub_fp) {
        fprintf(stderr, "Failed to open public key: %s\n", strerror(errno));
        return 0;
    }

    printf("DEBUG: Opening private key %s\n", priv_key_path);
    FILE* priv_fp = fopen(priv_key_path, "r");
    if (!priv_fp) {
        fprintf(stderr, "Failed to open private key: %s\n", strerror(errno));
        fclose(pub_fp);
        return 0;
    }

    EVP_PKEY* pub_key = PEM_read_PUBKEY(pub_fp, NULL, NULL, NULL);
    EVP_PKEY* priv_key = PEM_read_PrivateKey(priv_fp, NULL, NULL, NULL);

    fclose(pub_fp);
    fclose(priv_fp);

    if (!pub_key || !priv_key) {
        fprintf(stderr, "Failed to read keys: pub=%p priv=%p: %s\n", 
                (void*)pub_key, (void*)priv_key, ERR_error_string(ERR_get_error(), NULL));
        if (pub_key) EVP_PKEY_free(pub_key);
        if (priv_key) EVP_PKEY_free(priv_key);
        return 0;
    }

    int result = EVP_PKEY_cmp(pub_key, priv_key) == 1;
    EVP_PKEY_free(pub_key);
    EVP_PKEY_free(priv_key);
    return result;
}

int list_apps(char apps[MAX_APPS][MAX_NAME]) {
    DIR* dir = opendir(DIR_PATH);
    if (!dir) {
        fprintf(stderr, "Cannot open +x/ directory: %s\n", strerror(errno));
        return 0;
    }

    struct dirent* entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < MAX_APPS) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(apps[count], MAX_NAME, "%s", entry->d_name);
        count++;
    }
    closedir(dir);
    return count;
}

int main(int argc, char* argv[]) {
    char apps[MAX_APPS][MAX_NAME];
    char input[10];
    char subkey[SUBKEY_SIZE + 1] = "guest";
    unsigned char hash[HASH_SIZE];

    struct sigaction sa;
    sa.sa_handler = cleanup_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    OpenSSL_add_all_algorithms();

    printf("DEBUG: Starting authentication, argc=%d\n", argc);
    if (argc > 1) {
        char* filename = strrchr(argv[1], '/');
        if (!filename) filename = argv[1];
        else filename++;
        if (strncmp(filename, "priv.", 5) == 0 && strlen(filename) > 5) {
            size_t len = strlen(filename + 5);
            if (len > SUBKEY_SIZE) len = SUBKEY_SIZE;
            strncpy(subkey, filename + 5, len);
            subkey[len] = '\0';
            printf("DEBUG: Attempting to verify key pair for subkey %s\n", subkey);
            if (!verify_key_pair(subkey, argv[1])) {
                printf("DEBUG: Verification failed, reverting to guest\n");
                strncpy(subkey, "guest", SUBKEY_SIZE + 1);
            }
        }
    } else {
        printf("DEBUG: Generating new key pair\n");
        generate_random_hash(hash, 16);
        snprintf(subkey, SUBKEY_SIZE + 1, "%02x%02x%02x%02x", 
                 hash[0], hash[1], hash[2], hash[3]);
        if (!generate_key_pair(subkey, hash)) {
            printf("DEBUG: Key generation failed, using guest\n");
            strncpy(subkey, "guest", SUBKEY_SIZE + 1);
        }
    }

    current_subkey = subkey;
    printf("Logged in as %s\n", subkey);

    while (1) {
        printf("\nHome Screen\n");
        int app_count = list_apps(apps);
        if (app_count == 0) {
            printf("No apps found in +x/\n");
        } else {
            for (int i = 0; i < app_count; i++) {
                printf("%d. %s\n", i + 1, apps[i]);
            }
        }
        printf("Enter app number, 'm' for thread manager, or 'q' to quit: ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "q") == 0) {
            break;
        }

        if (strcmp(input, "m") == 0) {
            char manager_call[MAX_NAME];
            snprintf(manager_call, sizeof(manager_call), "./+x/thread_manager.+x %s", subkey);
            if (system(manager_call) == -1) {
                fprintf(stderr, "Failed to execute thread_manager.+x: %s\n", strerror(errno));
            }
            continue;
        }

        int choice = atoi(input);
        if (choice < 1 || choice > app_count) {
            printf("Invalid choice\n");
            continue;
        }

        char command[MAX_NAME];
        char proc_hash[9];
        snprintf(command, MAX_NAME, "%s%s", DIR_PATH, apps[choice - 1]);
        
        unsigned char proc_hash_raw[PROC_HASH_SIZE];
        generate_random_hash(proc_hash_raw, PROC_HASH_SIZE);
        sprintf(proc_hash, "%02x%02x%02x%02x", proc_hash_raw[0], proc_hash_raw[1], proc_hash_raw[2], proc_hash_raw[3]);
        proc_hash[8] = '\0';

        char system_call[MAX_NAME * 3];
        snprintf(system_call, sizeof(system_call), "./+x/run_app_manager.+x %s %s %s", command, proc_hash, subkey);
        if (system(system_call) == -1) {
            fprintf(stderr, "Failed to execute run_app_manager.+x: %s\n", strerror(errno));
        } else {
            printf("Launched %s with hash %s\n", apps[choice - 1], proc_hash);
        }
    }

    cleanup_children(0);
    printf("Exiting CLI platform\n");
    return 0;
}
