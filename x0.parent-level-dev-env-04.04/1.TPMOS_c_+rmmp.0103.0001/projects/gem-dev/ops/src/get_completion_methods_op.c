#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char* argv[]) {
    if (argc < 2) return 0;
    
    char *matches = argv[1];
    char *copy = strdup(matches);
    char *token = strtok(copy, "  ");
    int count = 0;
    
    printf("<button label=\"[Suggestions]\" onClick=\"ACTIVATE\" id=\"comp_root\"><br/>");
    printf("<button label=\"[Back]\" onClick=\"BACK\" id=\"comp_back\" /><br/>");
    
    while (token && count < 5) {
        printf("<button label=\"%s\" onClick=\"KEY:%d\" id=\"comp_%d\" /><br/>", token, count + 2, count);
        token = strtok(NULL, "  ");
        count++;
    }
    
    printf("</button>");
    
    free(copy);
    return 0;
}
