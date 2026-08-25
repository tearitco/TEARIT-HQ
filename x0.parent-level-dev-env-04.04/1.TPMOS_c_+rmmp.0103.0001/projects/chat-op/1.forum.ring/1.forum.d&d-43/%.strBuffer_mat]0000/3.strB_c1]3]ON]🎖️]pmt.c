#include <string.h>
#include <stdio.h>
#define MAX_TOKENS 1024
#define MAX_TOKEN_SIZE 64

char *ip_tokens[MAX_TOKENS];
int tokenCount ; 
void main() {
 char strBuffer[] = "0 1 1 test0 0 i 0\n"
                        "0 0 10 test1 0 am 1\n"
                        "1 0 11 test2 0 chillin in\n"
                         "0 1 1 test0 0 i 0\n"
                       
    char buffer[1024]; // Assume maximum line length is less than 1024 characters
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
        if (*token == '\0') break; // End of line
        cols++;
        token = strtok(NULL, " ");
    }

    printf("Matrix size: %d x %d\n", rows, cols);
   
  tokenCount =  rows * cols; 
  
 
    char strBuffer_copy[1024];
    strcpy(strBuffer_copy, strBuffer);
}

can we modify this code to store each token in strBuffer_copy into 
int ip_tokens[tokenCount] using
char *token = strtok(strBuffer_copy, " \n");
while (token != NULL) {
}
then we will print for sanity 





