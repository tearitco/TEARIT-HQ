#include <string.h>
#include <stdio.h>

#define MAX_TOKENS 1024
#define MAX_TOKEN_SIZE 64


int tokenCount;

void main() {
    char strBuffer[] = "test0 0 i 0 81 \ntest1 0 am 1 lo \n";
    /*
    char strBuffer[] = "0 1 1 test0 0 i 0\n"
                       "0 0 10 test1 0 am 1\n"
                       "1 0 11 test2 0 chillin in\n"
                       "0 1 1 test0 0 i 0\n";
                       */
    
char *strBuffer_copy[strlen(strBuffer)];
    strcpy(strBuffer_copy, strBuffer);

    char buffer[1024];  // Assume maximum line length is less than 1024 characters
    int rows = 0, cols = 0;

    char *token = strtok(strBuffer, "\n");
    rows = 0;
    while (token != NULL) {
        rows++;
        token = strtok(NULL, "\n");
    }

    cols = 0;
    token = strtok(strBuffer, " ");
    while (token != NULL && rows > 0) {
        if (*token == '\0') break;  // End of line
        cols++;
        token = strtok(NULL, " ");
    }

    printf("Matrix size: %d x %d\n", rows, cols);

    tokenCount = rows * cols;
    
    char *ip_tokens[tokenCount +1];

    
    int i = 0;
    token = strtok(strBuffer_copy, " \n");
    while (token != NULL) {
        ip_tokens[i] = strdup(token);
       
        i++;
        token = strtok(NULL, " \n");
    }
ip_tokens[tokenCount ] = '\0';

    // Print for sanity
    printf("Sanity check:\n");
    for (int j = 0; j < tokenCount; j++) {
        printf("%s\n", ip_tokens[j]);
    }
    
}

