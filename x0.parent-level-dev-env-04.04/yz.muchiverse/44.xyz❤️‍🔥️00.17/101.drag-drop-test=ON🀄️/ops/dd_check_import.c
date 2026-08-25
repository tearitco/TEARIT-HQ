/* dd_check_import.c - Verify pet was imported to mutaclsym
 *
 * Usage: dd_check_import <exchange_dir> <pet_id>
 *
 * Checks if pet directory exists in exchange and contains state.txt.
 * Prints PASS/FAIL, exits 0/1.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <exchange_dir> <pet_id>\n", argv[0]);
        return 1;
    }

    const char *exchange_dir = argv[1];
    const char *pet_id = argv[2];

    char path[1024];

    /* Check pet directory exists */
    snprintf(path, sizeof(path), "%s/%s", exchange_dir, pet_id);
    if (!file_exists(path)) {
        printf("FAIL [import]: pet directory not found: %s\n", path);
        return 1;
    }

    /* Check state.txt exists */
    snprintf(path, sizeof(path), "%s/%s/state.txt", exchange_dir, pet_id);
    if (!file_exists(path)) {
        printf("FAIL [import]: state.txt not found in %s/%s\n", exchange_dir, pet_id);
        return 1;
    }

    /* Check piece.pdl exists */
    snprintf(path, sizeof(path), "%s/%s/piece.pdl", exchange_dir, pet_id);
    if (!file_exists(path)) {
        printf("FAIL [import]: piece.pdl not found in %s/%s\n", exchange_dir, pet_id);
        return 1;
    }

    printf("PASS [import]: pet %s successfully imported to %s\n", pet_id, exchange_dir);
    return 0;
}
