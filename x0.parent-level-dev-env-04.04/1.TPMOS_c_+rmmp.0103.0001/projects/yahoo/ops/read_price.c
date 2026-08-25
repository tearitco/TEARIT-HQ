// File: read_price.c
// Compile: gcc -o /+x/read_price.+x read_price.c
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: read_price.+x symbol\n");
        return 1;
    }
    char *symbol = argv[1];
    char filename[256];
    snprintf(filename, sizeof(filename), "%s.txt", symbol);
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "Failed to open file %s\n", filename);
        return 1;
    }
    char buffer[32769];
    size_t bytes = fread(buffer, 1, sizeof(buffer) - 1, fp);
    buffer[bytes] = 0;
    fclose(fp);
    char *price_str = strstr(buffer, "\"regularMarketPrice\":");
    if (!price_str) {
        fprintf(stderr, "Price not found in %s\n", filename);
        return 1;
    }
    price_str += strlen("\"regularMarketPrice\":");
    char price[32];
    int i = 0;
    while (price_str[i] && price_str[i] != ',' && i < 31) {
        price[i] = price_str[i];
        i++;
    }
    price[i] = 0;
    printf("%s Current Price: %s\n", symbol, price);
    return 0;
}
