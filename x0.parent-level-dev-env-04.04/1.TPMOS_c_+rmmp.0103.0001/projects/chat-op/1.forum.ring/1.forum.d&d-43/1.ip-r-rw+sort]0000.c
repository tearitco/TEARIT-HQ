
#include <ifaddrs.h> // for getting user ip.
// C program to demonstrate peer to peer chat using Socket Programming
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

//
#include <fcntl.h> 


FILE *fp0 = NULL;
char ***a_jagged = NULL ;
char ***b_jagged = NULL ;
char ***c_jagged = NULL ;
int rows = 0, cols = 0;
int my_pos_row = 0 ;
int port_inc = 0 ;
int my_port = 1; 
//

int counted_rows = 1; // default by the time u use it...(but u may add 2)
int common_cols = 4; 
//
#define MAX_TOKENS 1024
#define MAX_TOKEN_SIZE 64


//
int fd;
void debug_0(){
/*
fp0 = fopen("ip_list.txt", "w");
printf(fp0,"a_jagged 🐞️ = \n"); //NOPE]MOVE OUT ! YW 🛸️
*/

int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
//


int sz; 
 
 fd = open("foo.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644); 
if (fd < 0) 
{ 
    perror("r1"); 
    exit(1); 
} 
 
sz = write(fd, "hello geeks\n", strlen("hello geeks\n")); 
 
printf("called write(% d, \"hello geeks\\n\", %d)."
    " It returned %d\n", fd, strlen("hello geeks\n"), sz); 
 
close(fd); 
}

void print_a_jagged(){
/*
printf("a_jagged 🐞️ = \n");
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        printf("%s ", a_jagged[i][j]);

    }
    printf("\n");
}
*/

    fp0 = fopen("ip_list.txt", "w");
// printf("a_jagged = TIMESORTAFTER :\n");
     for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
                printf("%s ",  a_jagged[i][j]);
                fprintf(fp0,"%s ",  a_jagged[i][j]);

    }
   
      printf("\n");
       fprintf(fp0,"\n");
}
}


int fill_a_jagged(){
rows = 0, cols = 0;
char *ip_tokens[MAX_TOKENS];

 fp0 = fopen("ip_list.txt", "r+");
    if (fp0 == NULL) {
        fprintf(stderr, "Error opening file\n");
        return 1;
    }
    /////////
    
     ///////////
      printf("debug.\n");
    
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

    rewind(fp0);


    printf("Matrix A size: %d x %d\n", rows, cols);
    ////////////////////
    char buffer_0[MAX_TOKEN_SIZE];
    int token_count = 0;

    while (fscanf(fp0, "%63s", buffer_0) == 1) {
    buffer_0[strlen(buffer_0)] = NULL ; //'\0';
        ip_tokens[token_count] = strdup(buffer_0);
      // ip_tokens[sizeof(buffer_0)] = '\0';
        if (ip_tokens[token_count] == NULL) {
            printf("Out of memory\n");
            return 2;
        }
        token_count++;
    }

   fclose(fp0);

    // Add a null terminator to the array
    ip_tokens[token_count] = NULL;
    
    
    
  // rewind(fp0);
   ///////////
   // printf("%d tokens read.\n", token_count);
    for (int i = 0; ip_tokens[i]; i++) {
  //      printf("Token %d: %s\n", i, ip_tokens[i]);
     //   free(ip_tokens[i]);
    }
   ////////

//char ***c_jagged;

// *new code goes here
int k = 0 ;
a_jagged = (char ***)malloc(rows * sizeof(char **));
for (int i = 0; i < rows; i++) {
    a_jagged[i] = (char *)malloc(cols * sizeof(char *));
    for (int j = 0; j < cols; j++) {
        a_jagged[i][j] = strdup(ip_tokens[k]);
        // a_jagged[i][j][strlen(ip_tokens[k])] = NULL ;
        k++;
    }
}

 k = 0 ;
 
 /*
printf("a_jagged = \n");
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        printf("%s ", a_jagged[i][j]);
    //    printf("%s ",ip_tokens[k]);
        k++;
    }
    printf("\n");
}
*/
//free(ip_tokens);

}


/////
int fill_c_jagged(){
rows = 0, cols = 0;
char *ip_tokens[MAX_TOKENS];

 fp0 = fopen("ip_list.txt", "r+");
    if (fp0 == NULL) {
        fprintf(stderr, "Error opening file\n");
        return 1;
    }
    /////////
    
     ///////////
      printf("debug.\n");
    
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

    rewind(fp0);


    printf("Matrix C size: %d x %d\n", rows, cols);
    ////////////////////
    char buffer_0[MAX_TOKEN_SIZE];
    int token_count = 0;

    while (fscanf(fp0, "%63s", buffer_0) == 1) {
    buffer_0[strlen(buffer_0)] = NULL ; //'\0';
        ip_tokens[token_count] = strdup(buffer_0);
      // ip_tokens[sizeof(buffer_0)] = '\0';
        if (ip_tokens[token_count] == NULL) {
            printf("Out of memory\n");
            return 2;
        }
        token_count++;
    }

   fclose(fp0);

    // Add a null terminator to the array
    ip_tokens[token_count] = NULL;
    
    
    
  // rewind(fp0);
   ///////////
    printf("%d tokens read.\n", token_count);
    for (int i = 0; ip_tokens[i]; i++) {
        printf("Token %d: %s\n", i, ip_tokens[i]);
     //   free(ip_tokens[i]);
    }
   ////////
   
 //  

//char ***c_jagged;

// *new code goes here
int k = 0 ;
c_jagged = (char ***)malloc(rows * sizeof(char **));
for (int i = 0; i < rows; i++) {
    c_jagged[i] = (char *)malloc(cols * sizeof(char *));
    for (int j = 0; j < cols; j++) {
        c_jagged[i][j] = strdup(ip_tokens[k]);
        // a_jagged[i][j][strlen(ip_tokens[k])] = NULL ;
        k++;
    }
}



//exit(11);
 k = 0 ;
printf("c_jagged = \n");
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        printf("%s ", c_jagged[i][j]);
    //    printf("%s ",ip_tokens[k]);
        k++;
    }
    printf("\n");
}

//free(ip_tokens);

}
///////////////////////////

//🦙️need? not yet ,imo
////////////////🆕️🔚️

 int num_ips = 0; // Count the number of IPs found
    char **ip_addrs = NULL; // Array to store IP addresses

int get_ip(){

 struct ifaddrs *ifa;
    struct sockaddr_in *ipa_addr;

    // Get the list of network interfaces
    getifaddrs(&ifa);

   
    // Loop through each interface
    for (; ifa; ifa = ifa->ifa_next) {
        ipa_addr = (struct sockaddr_in *) ifa->ifa_addr;

        // Check if this is an IPv4 interface with a global IP address
        if (ipa_addr != NULL && ipa_addr->sin_family == AF_INET) {
            char buf[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ipa_addr->sin_addr, buf, sizeof(buf));
            /*
            printf("inet %s/%d brd %s scope global\n", buf,
                    ((struct sockaddr_in *) ifa->ifa_netmask)->sin_family == AF_INET ?
                    (int)((struct sockaddr_in *) ifa->ifa_netmask)->sin_port : 0, buf);
                    */

            // Allocate memory for the IP address array
            ip_addrs = realloc(ip_addrs, (num_ips + 1) * sizeof(char *));
            ip_addrs[num_ips++] = strdup(buf);
        }
    }

    freeifaddrs(ifa);

    // Print the entire array of IP addresses
    printf("Found %d IP addresses:\n", num_ips);
    for (int i = 0; i < num_ips; i++) {
        printf("%s\n", ip_addrs[i]);
    }

/*
    // Free the memory allocated for the IP address array
    for (int i = 0; i < num_ips; i++) {
        free(ip_addrs[i]);
    }
    free(ip_addrs);
*/
    return 0;
}
//🆕️

int time_sort(){
// right now i will find the ip that is 'me'
//(one should be now, were out of "add_me" (mark it in there if u need)

// either u can assume u know and move on , or u can try a "time sort"
/*
🧼️
int counted_rows = 1; // default by the time u use it...(but u may add 2)
int common_cols = 5; 
sort_b_jagged(b_jagged, counted_rows, common_cols);
🧼️

*/

  printf("b_jagged = TIMESORT :\n");
     for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
                printf("%s ",  a_jagged[i][j]);

    }
   
      printf("\n");
}


printf("test %s \n", a_jagged[0][0]);
//printf("test1 %d ", atoi(b_jagged[0][0]));
  char **temp = (char *)malloc(sizeof(char));
    // Bubble sort algorithm
    for (int i = 0; i < rows - 1; i++) {
        for (int j = 0; j < rows - i - 1; j++) {
      //  printf("%d < %d \n ", atoi(b_jagged[j][0]), atoi(b_jagged[j+1][0]));
        
            if (atoi(a_jagged[j][0]) < atoi(a_jagged[j+1][0])) {
              
                temp = a_jagged[j][0];
                a_jagged[j][0] = a_jagged[j + 1][0];
                a_jagged[j + 1][0] = temp;
                int k = 0; 
                // if it doesn't work w/e try again
                for(int k = 1; k < cols ; k++){
                temp = a_jagged[j][k];
                a_jagged[j][k] = a_jagged[j + 1][k];
                a_jagged[j + 1][k] = temp;
                }
                
                //i made this tbh . ez 🐛️🦋️🤯️🎖️
            }
            
        }
    }
    
    //
 /*   
     fp0 = fopen("ip_list.txt", "w");
 printf("a_jagged = TIMESORTAFTER :\n");
     for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
                printf("%s ",  a_jagged[i][j]);
                fprintf(fp0,"%s ",  a_jagged[i][j]);

    }
   
      printf("\n");
       fprintf(fp0,"\n");
}
*/
// we should be at the top, then can just go down the list. 
//print_a_jagged(); //🤫️
}
//🆕️
//char ***a_jagged = NULL ;
int add_me(){
 time_t now = time(NULL);
           char temp_0[32] ;
                 // Temporary string buffer
    sprintf(temp_0, "%d", now);
//check if my ip is on the list. 
// if not add it. + timestamp (all we need right now)
//🦙️ prompt can do this. 

int ip_on = 0 ;

  
    for (int i = 0; i < rows; i++) {
if(strcmp(ip_addrs[num_ips -1], a_jagged[i][1]) == 0 && my_port == atoi(a_jagged[i][2])){
 ip_on++; 
 my_pos_row = i ; // doesn't account for multiples/ ie port/...
 // maybe we get my_pos later, when we have port sorted ✳️ 
 
 //]🕰️ sept29:set time-stamp now. we'll(sort,then) use this for syn-ack

 a_jagged[i][0] = temp_0 ;
 counted_rows = rows ; 
}


}

printf("ip_on = %d\n",ip_on);

 //port_inc = ip_on + 1 ;

if(ip_on > 0){
printf("ip should already be good 2 go \n");// still need port ✳️
// just assume the user wants a "new port" each time. 
// problem solved for now ffs. chill. 
// fix that when u want it fixd its not a big deal
}

/*
if(ip_on == 0) then add another row to a_jagged
the put int timstamp in  a_jagged[i][0]
and add ip_addrs[num_ips -1] to a_jagged[i][1])

printf a_jagged for sanity check
 
 ⚖️[readability rules: this will be in PURE GCC C (no structs , use arrays instead);must compile ; must run w/o segfaulting]⚖️
*/
    int num_rows = rows + 1;
   if (ip_on == 0) {
        // Add another row to a_jagged
     //   int num_rows = rows + 1;
     
     
     
     b_jagged = (char **)malloc(num_rows * sizeof(char *));
    for (int i = 0; i < num_rows; i++) {
        b_jagged[i] = (char *)malloc(common_cols * sizeof(char));
        
    }
    
    //u still have 2 malloc each line. (do it like word probe)
 
            // Fill in the new row with timestamp and IP address
             // Convert integer to a string
//char *token = strtok(temp_0, " \n");
// b_jagged[rows][0] = "123";//strdup(temp_0);

b_jagged[rows][0] = (char *)malloc(strlen(temp_0) + 1);
     
       //  strcpy(b_jagged[rows][0], temp_0);
         b_jagged[rows][0] = temp_0 ;
         //////////
            b_jagged[rows][1] = ip_addrs[num_ips - 1];//
          char temp_1[16] ;  // Temporary string buffer
    sprintf(temp_1, "%d", my_port);  // Convert integer to a string
    

 b_jagged[rows][2] = strdup(temp_1);
 
 
      b_jagged[rows][3] = ip_addrs[num_ips - 1];
      
       char temp_2[16] ;  // Temporary string buffer
    sprintf(temp_2, "%d", my_port  + 1); 
 
      b_jagged[rows][4] = strdup(temp_2);
           /////
              // Print the updated jagged array for sanity check
   // printf("b_jagged:\n");
    /*
    for (int i = 0; i < num_rows; i++) {
      printf("%d %s/%d %s/%d \n", atoi(b_jagged[i][0]), b_jagged[i][1], atoi(b_jagged[i][2]) , b_jagged[i][3], atoi(b_jagged[i][4] ));
    }
    
    */
      //  rows = num_rows;
    }else{
    
    /*
       b_jagged = (char **)malloc(rows * sizeof(char *));
    for (int i = 0; i < rows; i++) {
        b_jagged[i] = (char *)malloc(common_cols * sizeof(char));
    }
    
    b_jagged = a_jagged ;
      printf("b_jagged = a :\n");
    for (int i = 0; i < rows; i++) {
     //   printf("%d %s/%d\n", atoi(b_jagged[i][0]), b_jagged[i][1], atoi(b_jagged[i][2]));
        
       
        // the other bjagged should use atoi also , not *(int *)
        
     
    }
     num_rows = rows; // is this still used?
     
     */
    }
    
    /*
    fp0 = fopen("ip_list.txt", "w");
     printf("b_jagged = FIN :\n");
     for (int i = 0; i < counted_rows; i++) {
    for (int j = 0; j < common_cols; j++) {
                printf("%s ", b_jagged[i][j]);
                  fprintf(fp0,"%s ", b_jagged[i][j]);

    }
   
      printf( "\n");
      fprintf(fp0, "\n");
      
}
*/
//print_a_jagged(); //🤫️
fclose(fp0);
}

int main(int argc, char const *argv[]){
 printf("sort start ❣️\n");
   if (argc > 1) {
    
 my_port = atoi(argv[1]);
 printf("myport sort = %d \n", my_port);
 }else{}
//📅️
get_ip(); 
fill_a_jagged();
add_me();
free(a_jagged);
//printf("new c_jagged = FIN :\n");
//fill_c_jagged();
fill_a_jagged();
time_sort();
}
