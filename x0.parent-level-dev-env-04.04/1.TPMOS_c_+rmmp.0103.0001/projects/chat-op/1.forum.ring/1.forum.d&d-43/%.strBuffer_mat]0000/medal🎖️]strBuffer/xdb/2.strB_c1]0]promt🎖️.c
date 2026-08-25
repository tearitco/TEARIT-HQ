


   ////////////

    char buffer[1024];   // Assume maximum line length is less than 1024 characters
    int rows = 0, cols = 0;

    while (fgets(buffer, sizeof(buffer), fp1) != NULL) {
        // Count number of columns and rows
        cols = 0;
        char *token = strtok(buffer, " \n");
        while (token != NULL) {
            if (*token == '\0') break;   // End of line
            cols++;
            token = strtok(NULL, " \n");
        }
        rows++;
    }

    rewind(fp1);

 /////////
printf("Matrix A size: %d x %d\n", rows, cols);

                        
   i want to modify this function to use strtok to count rows and column[rows are seperated by newline "\n", cols are seperated by blank space " "] in  char strBuffer; (assume it is already filled)
   instead of reading from fp1
   
       
                 
    ⚖️[readability rules: this will be in PURE GCC C (no structs , use arrays instead);must compile ; must run w/o segfaulting]⚖️                    
      
      
      
      
      
      
      
      
      
      
                        

   char* token = strtok(strBuffer, " ");
 
    while (token != NULL) {
 
        cols++;
        token = strtok(NULL, " ");
      
        
        while (token != NULL && *token != '\n') {
        
            token = strtok(NULL, " ");
        }
        if (*token == '\n') {
     
            rows++;
            token = strtok(NULL, " ");
        }
    
    }

this is just psudo code. write the real code for this. 

⚖️[readability rules: this will be in PURE GCC C (no structs , use arrays instead);must compile ; must run w/o segfaulting]⚖️















 
 here is my attempt but it doesn't work. can u try to make it work?

 char* token = strtok(strBuffer, " ");
 
    while (token != NULL) {
    // its reading 1 colum then 3 rows then other colum or some other funky thing , thats why u cant allocate like this  
        cols++;
        token = strtok(NULL, " ");
      
        
        while (token != NULL && *token != '\n') {
        
          ip_tokens[token_count] = token;
        token_count++;
        
            token = strtok(NULL, " ");
        }
        if (*token == '\n') {
     
            rows++;
            token = strtok(NULL, " ");
        }
    
    }















 char buffer[1024];   // Assume maximum line length is less than 1024 characters
    while (fgets(buffer, sizeof(buffer), fp0) != NULL) {
        // Count number of columns and rows
        cols = 0;
        char *token = strtok(buffer, " \n");
        while (token != NULL) {
            if (*token == '\0') break;   // End of line
            cols++;
            token = strtok(NULL, " \n");
        }
        rows++;
    }

lets modify this function to count rows and cols from strBuffer instead
here is strBuffer

 char strBuffer[] = "0 1 1 test0 0\n"
                        "0 0 10 test1 0\n"
                        "1 0 11 test2 0\n";
⚖️[readability rules: this will be in PURE GCC C (no structs , use arrays instead);must compile ; must run w/o segfaulting]⚖️
 
 








//🎖️
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 int rows = 0;
    int cols = 0;
   
int main() {

  char strBuffer[] = "0 1 100 test0\n"
    "0 2 100 test1\n"
    " 2 100 test2\n";
          

    return 0;
}

we need to add code to count rows (seperated by newline) ;  and cols (seperated by a space) 

use strtok



print the results for sanity check 

 ⚖️[readability rules: this will be in PURE GCC C (no structs , use arrays instead);must compile ; must run w/o segfaulting]⚖️
 
 
 
 we need to add code to count rows (seperated by newline) ;  and cols (seperated by a space) 

use strtok

 
 (dont assume all data will look like strBuffer; keep the code flexible
so it can be reused for similiar applications)
 
 
 char strBuffer[] = "0 1 100 test0\n0 2 100 test1\n"
                  
 
   char strBuffer[] = "0321 172.17.0.1 23 0\n"
                       "2 172.17.0.1 100 0\n"
                       "3 172.17.0.1 3121 0\n";
                       "3 172.17.0.1 0093 0\n";
                       "100 172.17.0.1 2 test.txt\n";
                       
                       
                        ⚖️[readability rules: this will be in PURE GCC C (no structs , use arrays instead);must compile ; must run w/o segfaulting]⚖️
 
 
