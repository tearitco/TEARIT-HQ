// C program to demonstrate peer to peer chat using Socket Programming
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

int server_fd;
//Receiving messages on our port


///////////////////////////////////////////////🤝️
char* my_ip[16]; 
int my_port ; 
// we didn't pass these as args yet, ez 2 tho . lesgo .
char* destination_ip[16]; 
int destination_port ; 

 //☎️🤝️
FILE *fp0 = NULL;
//  char *my_ip = NULL ; //a_jagged[0][1];
 // int my_port ; 
   char *send_ip = NULL; //a_jagged[1][1]; // we already have 1(dest). pick tbh
   int send_port ; //atoi(a_jagged[1][2]);
   
   int shake_count = 0 ;
   //📅️oct15]
//🧵️



int rows = 0, cols = 0;

char ***a_jagged = NULL ;

void print_a_jagged() {
 printf("a jagged sanity : \n");
    int i, j;
    for (i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            printf("%s ", a_jagged[i][j]);
        }
        printf("\n");
    }
}

#define MAX_TOKENS 1024
#define MAX_TOKEN_SIZE 64
char *ip_tokens[MAX_TOKENS];
int token_count = 0;

//
char buffer[MAX_TOKENS] = {0};
char* strBuffer = NULL ; 
char* strBuffer_copy = NULL ; 
char* strBuffer_copy0 = NULL ; 
char* strBuffer_copy1 = NULL ; 
//i want to modify this code to read from "char strBuffer" instead of fp1

//-------------------------------
void  count_rows_cols_2(){
printf("TEST _2 \n");
int tokenCount;


strBuffer_copy0 = malloc(strlen(buffer)+1);
strBuffer_copy0 = strdup(buffer);
 strBuffer_copy1 = malloc(strlen(buffer)+1);
strBuffer_copy1 = strdup(buffer);
 


 char buffer[1024];  // Assume maximum line length is less than 1024 characters
  

    char *token = strtok(strBuffer_copy0, "\n");
    rows = 0;
    while (token != NULL) {
        rows++;
        token = strtok(NULL, "\n");
    }

    cols = 0;
    token = strtok(strBuffer_copy0, " ");
    while (token != NULL && rows > 0) {
        if (*token == '\0') break;  // End of line
        cols++;
        token = strtok(NULL, " ");
    }

    printf("Matrix size: %d x %d\n", rows, cols);
printf("TEST _2 \n");
    tokenCount = rows * cols;
    
    
   
   
    char *ip_tokens[tokenCount ];

    
    int i = 0;
   char *token1  = strtok(strBuffer_copy1, " \n");
    while (token1 != NULL) {
        ip_tokens[i] = strdup(token1);
         printf("%s\n", ip_tokens[i]);
        i++;
        token1 = strtok(NULL, " \n");
    }
    
//ip_tokens[tokenCount +1 ] = '\0';



    // Print for sanity
    printf("Sanity check:\n");
    
    for (int j = 0; j < tokenCount; j++) {
        printf("%s\n", ip_tokens[j]);
    }
    
    // ♣️ rest is EZ? 
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

}
//-------------------------------
////////
void  count_rows_cols(){

strBuffer_copy0 = malloc(strlen(buffer)+1);
strBuffer_copy0 = strdup(buffer);

// printf("strBuffer_copy = \n%s \n", strBuffer_copy0);
 
 //////////////////////📅️oct16]📅️
   char *token1;
 token1 = strtok(strBuffer_copy0, " \n");
    while (token1 != NULL) {
        ip_tokens[token_count] = token1;
        token_count++;
        token1 = strtok(NULL, " \n");
    }
    

//////////////////
      printf("%d tokens read.\n", token_count);
    for (int i = 0; i < token_count; i++) {
        printf("Token %d: %s\n", i, ip_tokens[i]);
  
    }
    
  //  free(strBuffer_copy0); // doing this MESSES UP STUFF " BAD" 
    
    strBuffer_copy1 = malloc(strlen(buffer)+1);
strBuffer_copy1 = strdup(buffer);
 
 // i think strBuffer has to be rewound or something...

    char* token = strtok(strBuffer_copy1, " ");
 
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
    
    printf("🗓️nov24 check [the list is bs right? u need a real list...] \n ");
    
    
    /*🗓️nov24
      printf("arg4!= %s \n",file_0_txt);
    2.f2p.mod_1]send]0002.c <- file we sent...(wut is it tho? how pulled?)
    <- was part of arg from ./ %.1.f2p.autoforum📌️]0017.c
    -> the confusion is we dont know wut fx() ;) 🤯️😷️=doxujin
    
    
    sprintf(system_target_0, "./+x/2.f2p.mod_1]send]0002.+x %s %d %s %d %s",my_ip, my_port, send_ip , send_port , "ip_list.txt");
    //😱️
    // were sending "IP" list (but we didn't write anything yet... or combine. <-FEAR!(losing og data?)
    // ignore FEAR. right now! just read it {PARSE IT} and return 2 sender 
    
    // we dont know wut order it was (SORTED IN <<or if we need 2 resort. 
    // u probably wanna write it first, u havn'et yet 'temp.ip.synack.txt'⬇️HERE!⬇️
    
    //🐡️problem? we have lots of safety measures <3
   //"handshake]send.txt"
   ->handshake]rcv.txt
    //😱️
     
     
     */
     
     
     
     
    printf("Rows: %d, Columns: %d\n", rows, cols);


int k = 0 ;
 a_jagged = (char ***)malloc(rows * sizeof(char *));
    for (int i = 0; i < rows; i++) {
        a_jagged[i] = (char *)malloc(cols * sizeof(char *));
        for (int j = 0; j < cols; j++) {
            a_jagged[i][j] = strdup(ip_tokens[k]);
            k++;
        }
    }

FILE *handshake_rcv = fopen("handshake]rcv.txt", "w"); 

 printf("a_jagged =\n");
    // print the jagged array
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            printf("%s ", a_jagged[i][j]);
            
             fprintf(handshake_rcv, "%s ", a_jagged[i][j]);
        }
        printf("\n");
         fprintf(handshake_rcv, "\n");
         
         //🗓️nov24] do u even want this data? w/e / were buildling infra
    }

 fclose(handshake_rcv);

}



   
      // apprantly this is way different than reading from file. 
   // 🦙️just gonna olamma prompt 
   
   void syn_or_ack(){
   //☎️🤝️🦘️
   // if 1 row , its syn else ack 
   // it can be in here or there, just push . 🏎️
   
    printf("\n 😻️🖨️🤝️ \n");
//fill_a_jagged_from_strBuffer();
//count_rows_cols() ; // faulty 
count_rows_cols_2();
//fill_a_jagged();
//fill_a_jagged_from_str();
//print_a_jagged();
   }



void read_handshake(){
// were gonna count, and when we get 2 #3 in handshake.txt  (minus header) 🤓️
// then we can send "blockchain or w/e (i will include filenames in header
//☎️🤝️ syn >  (how do u know were on syn? { in header. | by count of [ROWS]})🦘️
//☎️🤝️  syn-ack > 
//☎️🤝️  ack > 

//📍️📮️ we dont need "nuanced reader" its a very simple read...(just handshake )
// even if we do use "ip-list.txt, i doubt we will have as many rows as now
//🏓️📌️ need 2 read syn we just recieved from file first. 
//🏓️📌️ u have 2 token parse it cuz u want IP/PORT

//🤯️
// we already recieved it (its chat.txt 4 now , u need 2 change it...
//🧕🏻️this is important, i want u 2 know u dont have to read "HEADER"
// u can skip header and read it differently if u want.
// however many lines u want. (esp "file name 2 read / write 2  < 4 sockets @least
//❌️just do it, u can w8 but it makes things easier...❌️
//👩🏻‍🚀️u should still w8 till everything else is working tbh <3 ]EZ❣️
//💃🏾️save the fancy moves 4 l8r : assume filename.txt 4 now <3]EZ❣️💃🏾️👩🏻‍🚀️
//🧕🏻️
//🤯️
}
void send_syn_ack(){
//🆕️ everythings the same, but u should be using 
 // also we are in "SYN" ; u need 2 grab "SENDER IP (top?) from a_jagged[0][1] / a_jagged[0][2]
//♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️v-old code..-v
//🏓️ send_ip < isn't real yet. u need 2 read it off rcv.txt first.
//🏓️ this is where we are. 
// u need 2 send back. u SHOULD do this using system("./send.+x")
// its robust enough ;just like .h if not , fix it. 
//☎️🤝️ syn > 
//☎️🤝️  syn-ack > 
 printf("\n♣️malformed?\n");
// exit(911);
// print_a_jagged();
//☎️🤝️  ack > (do we finally take senders ip? add news?= w/e]L8r)🐢️
printf("\n♣️malformed= %s %d\n", a_jagged[0][1] , atoi(a_jagged[0][2]));

//printf("\n♣️malformed= %s \n", a_jagged[0][1] );
//i dont think a_jagged is full yet. ♣️


      char *system_target_0[512]; // adjust the size according to your needs
      
sprintf(system_target_0, "./+x/2.f2p.mod_1]send]0002.+x %s %d %s %d %s",my_ip, my_port, a_jagged[0][1] , atoi(a_jagged[0][2]) , "ip_list.txt");

 printf("arg = %s \n",system_target_0);
 
 system(system_target_0);
 
 // ♣️ sending "syn-ack" 🆕️🐢️(will merg ip list later = versioning/BTC)
// (1 is sending (2 needs 2 change their flag (2 2 ? )
// (or they should have set it 2 "2" on 'SYN-SEND'
// meaning u should do a flag check/set in _SEND
//just do it, it cant hurt, if u dont need, just turn it off...

// also why doesn't '1🥇️' auto send? ?????????
// these are just things we aren't "SOLID" on yet...
//+ stalkers trying 2 make it hard 2 think [ez-tho]EGO=BED=RIP🪦️
//🧕️honestly we can use same "FX" 2 🔍️check/write handshake]MOD🧕️🐢️l8r. 💋️✂️
//📬️

// just send . shrug. this IS the safetfy...
// ♣️(timeout and deprioritize if no response)<- each time ⏱️
}
/////////////////////////////////////////////////////


//🦃️🤝️🦃️
int handshake_st8 = 0; 
check_handshake_addresses(){

//🦃️
// things are basically the same, but will read/write 2 'port/#.txt'
//instead of 'handshake]rcv.txt'
//🦃️

 char *system_target_1[512]; // adjust the size according to your needs
 char *system_target_2[512];
//sprintf(system_target_1, "./+x/2.f2p.mod_1]send]0002.+x %s %d %s %d %s",my_ip, my_port, destination_ip , destination_port , "handshake]rcv.txt");
  sprintf(system_target_1, "%s_%d.txt",my_ip, my_port);

printf("\n🦃️🦃️🦃️🦃️🦃️🦃️🦃️🦃️🦃️\n%s\n🦃️🦃️🦃️🦃️🦃️🦃️🦃️🦃️🦃️🦃️🦃️🦃️🦃️\n",system_target_1);

int handshake_st8 = 1111; 
 FILE *handshake_check = fopen(system_target_1, "r"); 
 
  //🦙️ read contents of this file (should be one int) to handshake_st8 and print result. if there is no file store 0 print result for sanity.
 
 //🦙️let ollama do this
 


    if (handshake_check == NULL) {
        printf("No file found. Storing 1111.\n");
        handshake_st8 = 1111 ; 
    } else {
  
      
       fscanf(handshake_check, "%d ", &handshake_st8);
    
    printf("Sanity check: %d\n", handshake_st8);

      
       fclose(handshake_check);
      
    }
     
  printf("Handshake value: %d\n", handshake_st8);
  
     if(handshake_st8 == 1111){
 printf("\n♣️3rd sanity-send-syn-ack");
 // made it, didn't get here before 🧲️🛸️
 
 send_syn_ack(); 
 
 }
  
  if(handshake_st8 == 1000){
  
  /*
  sprintf(system_target_1, "%s/%d.txt",my_ip, my_port);
 i want to open system_target_1 as a file and write 
int handshake_st8 to it. proceed
*/
    FILE *file = fopen(system_target_1, "w"); // openthe file for writing
        if (file != NULL) {
            fprintf(file, "%d\n", 1111); // write 1, cuz sending syn-ack
            fclose(file);
        } else {
            printf("Error opening file: %s\n", 
system_target_1);
        }
         printf("Sanity check: %d\n", 1111);
        
        //♣️
        // also we are in "SYN" ; u need 2 grab "SENDER IP (top?) from a_jagged[0][1] / a_jagged[0][2]
        
        // then send syn-ack // and u didn't need 2 write "0"?<-can @ end...
        //after gettng "ack" (or some other st8 w/e)
        // now u should probably set it 2 "1"
///2222 ❗️nov30 *dont do this YET, send is doing "AUTOSEND()" , which causes segfault!❗️👬️<pairbonding;BRB👬️
  }
   if(handshake_st8 == 2222){
   printf("🇫🇮️🇫🇮️🇫🇮️🇫🇮️🇫🇮️RCV 2222 - send ack+blockchain : %d\n", handshake_st8);
 }

 /*
 later we may delete "system_target_1" on program kill or w/e
*/
}

void rcv_handshake(){

// recieved first or last handshake. which? first writes, third clears
// read handshake file. its just ints 4 now? u may want ips, id keep it jagged...

 // were not rcving anything , we gonna write & start monitoring handshake.txt till 3 then erase
 
 // could it bee sending and recieving? for now im gonna do handshake]send.txt
 // & handshake]rcv.txt (keep it simple and observe 4 weird cases <3 ) 
 printf("\nHandshake protocol]RCV 🤝️ 🥈️{send-synack NOW!}<-get sendr ip & ur ip from list ⬇️ \n");
 // now just fire up send (w8 , wut does our handshake.txt say? 
 // maybe we have no business sending. we can do that here
 // and modularize it later. 
// write_handshake();
// read_handshake();

 
 //syn_or_ack();
  printf("\n 😻️🖨️🤝️ \n");
 count_rows_cols_2();
 check_handshake_addresses();

}

// im rewriting(editing) "receiving(int server_fd)" 
// commented it , want to change write 2 name. w/e ]ez.but safety1rst

void receiving(int server_fd){
    struct sockaddr_in address;
    int valread;
    
    int addrlen = sizeof(address);
    fd_set current_sockets, ready_sockets;

    //Initialize my current set
    FD_ZERO(&current_sockets);
    FD_SET(server_fd, &current_sockets);
    int k = 0;
    //💿️loops happen bro. its not magic
    while (1)  
    {
        k++;
        ready_sockets = current_sockets;

        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
        {
            perror("Error");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (FD_ISSET(i, &ready_sockets))
            {

                if (i == server_fd)
                {
                    int client_socket;

                    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0){
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                    FD_SET(client_socket, &current_sockets);
                }else{
                    valread = recv(i, buffer, sizeof(buffer), 0);
                    
                    ////🎅️
                    struct sockaddr_in sender_addr;

            socklen_t sender_len = sizeof(sender_addr);
                   getpeername(i, (struct sockaddr *)&sender_addr, &sender_len);
///////////

            printf("\nReceived packet from address %s:%d with size %d bytes\n",
//🦃️doesn't mean we "have a relationship wiht them. do normal🦃️
                   inet_ntoa(sender_addr.sin_addr),
                   ntohs(sender_addr.sin_port),
                   valread);
               //📬️seems like its mapping 2 'virtual port #'
               // w/e as long as i can use "real #'s and it knows w/e"
               // we shall find out
                  //📬️   
                    printf("\n%s\n", buffer);//🎅️
                    
                    
   
      //////////
                    FD_CLR(i, &current_sockets);
                    ///////]ON!📲️
                    

                     FILE *file;
                    file = fopen("handshake.txt", "w"); // read the chat.txt file
                    
                     if (file == NULL) {
                        printf("Error opening file!\n");
                        return;
                    }


////////////📅️oct13📅️🦙️🦙️


/*
 strBuffer = malloc(valread + 1); // allocate memory for the string
for (int i = 0; i < valread; ++i) {
    strBuffer[i] = buffer[i];
}
strBuffer[valread] = '\0'; // add a null terminator

// Print the contents of the string to the console:
printf("%s\n", strBuffer);
*/


rcv_handshake(); // oct12 📅️
// Don't forget to free the allocated memory when you're done with it
//free(strBuffer);
///////////////////////////////////////////////////





/*
int n = fwrite(buffer, 1, valread, file);
if (n < 0) {
    perror("Write failed\n");
    fclose(file);
    return;
}
*/
/*
                    int n;
                    
                    while ((n = fwrite(buffer, 1, valread, file)) < 0) {
                        perror("Write failed\n");
                        fclose(file);
                        return;

                    }
                    */
                    
                    /////////////////📅️oct13📅️🦙️

                    fclose(file);


                    //////📲️
                }
            }
        }

        if (k == (FD_SETSIZE * 2))
            break;
    }
    
    ////////
    

    
    //////////
}



//Receiving messages on our port
/*
void receiving(int server_fd){
    struct sockaddr_in address;
    int valread;
    char buffer[2000] = {0};
    int addrlen = sizeof(address);
    fd_set current_sockets, ready_sockets;

    //Initialize my current set
    FD_ZERO(&current_sockets);
    FD_SET(server_fd, &current_sockets);
    int k = 0;
    //💿️loops happen bro. its not magic
    while (1)  
    {
        k++;
        ready_sockets = current_sockets;

        if (select(FD_SETSIZE, &ready_sockets, NULL, NULL, NULL) < 0)
        {
            perror("Error");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < FD_SETSIZE; i++)
        {
            if (FD_ISSET(i, &ready_sockets))
            {

                if (i == server_fd)
                {
                    int client_socket;

                    if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0){
                        perror("accept");
                        exit(EXIT_FAILURE);
                    }
                    FD_SET(client_socket, &current_sockets);
                }else{
                    valread = recv(i, buffer, sizeof(buffer), 0);
                    
                    ////🎅️
                    struct sockaddr_in sender_addr;

            socklen_t sender_len = sizeof(sender_addr);
                   getpeername(i, (struct sockaddr *)&sender_addr, &sender_len);
///////////
rcv_handshake(); // oct12 📅️
            printf("\nReceived packet from address %s:%d with size %d bytes\n",
                   inet_ntoa(sender_addr.sin_addr),
                   ntohs(sender_addr.sin_port),
                   valread);
               //📬️seems like its mapping 2 'virtual port #'
               // w/e as long as i can use "real #'s and it knows w/e"
               // we shall find out
                  //📬️   
                    printf("\n%s\n", buffer);//🎅️
                    
                    
   
      //////////
                    FD_CLR(i, &current_sockets);
                    ///////]ON!📲️
                    
                     FILE *file;
                    file = fopen("chat_0.txt", "w"); // read the chat.txt file
                    
                     if (file == NULL) {
                        printf("Error opening file!\n");
                        return;
                    }

                    int n;

                    while ((n = fwrite(buffer, 1, valread, file)) < 0) {
                        perror("Write failed\n");
                        fclose(file);
                        return;
                    }

                    fclose(file);


                    //////📲️
                }
            }
        }

        if (k == (FD_SETSIZE * 2))
            break;
    }
    
    ////////
    
    
    //////////
}


*/


/////////////
char* arg_0[64]; 
int main(int argc, char const *argv[])
{

  while (1){
        sleep(2);
    printf("peer.mod_2 in! {📆️nov25-2do:pls dump ALL6 ip/s...} \n");
    
   
    
   // scanf("%s", name);
   //////////////
     if (argc > 5) {
         server_fd = atoi(argv[1]);
       
        
        /*
        char* my_ip[16]; 
int my_port ; 
// we didn't pass these as args yet, ez 2 tho . lesgo .
char* destination_ip[16]; 
int destination_port ; 
*/


          ///
    strcpy(my_ip, argv[2]);
    my_port = atoi(argv[3]);
    
      strcpy(destination_ip, argv[4]);
    destination_port = atoi(argv[5]);
    
    //📆️25]❓️is it sending "MY IP & DEST IP (as a rcv? 
    // yea because it knows already based on blah blah 
    // however if "rcvr" changes/leaves we will do that LATER <3❣️
  
///
 printf("rcv is on ;)🐞️\n myip/myport!= %s/%d \n send-ip/send-port!= %s/%d \n",my_ip,my_port,destination_ip,destination_port);
        // ...
    } else {
        printf("Error: no port number specified|not enuff args\n");
      //  return 1;
    }
////////////
   
 //  sending();
 receiving(server_fd);
 }
}





