// C program to demonstrate peer to peer chat using Socket Programming
#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

char name[20];
int PORT; // yea , it doesn't need YOUR port lol . <3 

//📅️

char* my_ip[16]; 
int my_port ; 

char* destination_ip[16]; 
int destination_port ; 


//////////////////////////////


 char *file_0_txt = NULL ;  
 
 
 ////////✍🏻️
 
 void write_handshake(){
 //🤯️'oct12]
 //this has already sent (from argv, ur now just writing 1 2 it. 
  //🤯️✳️
  
  
  
 FILE *handshake_snd = fopen("handshake]send.txt", "w"); // read from the chat.txt file
 // were not sending anything , we gonna write & start monitoring handshake.txt till 3 then erase
// could it bee sending and recieving? for now im gonna do handshake]send.txt
 // & handshake]rcv.txt (keep it simple and observe 4 weird cases <3 )  
 // were not "reading" we assume its empty "w" will def empty it. 
 
 /*
 strcpy(my_ip, argv[1]);
    my_port = atoi(argv[2]);
///
     strcpy(destination_ip, argv[3]); //
 destination_port = atoi(argv[4]);
 */
 int handshake_step_number = 1;
 fprintf(handshake_snd, "%d %s %d\n ", handshake_step_number, my_ip, my_port);
 //🤠️ u need 2 send this , and read it in rcv when u read...parse it
 // thats the "complicated part here"]📅️oct13🤠️📌️
 
 //✳️-> now rcv should fprint ,2 (ONLY ) {4 now} and send back
 //oct12]doesn' thave 2 be "on same file tbh , confusion
 // we will hide "FAST FS" later🏎️ (we erase b4/after w/e is ok <3 ❤️‍🔥️
 //🍥️ (only ever 2 ips on here so not ip list)but may add ips l8r
 // may literally have both ips on here
 // read top as sender and 2n'd as reciver, check and send back
 fclose(handshake_snd);
 
 // 🚨️im TEMTED 2 move this into main , but wont 4 now cuz
  // 🚨️ i think rcv -syn-ack- has 2 happen in rcv...
  // 🚨️]just push it like this . 
  //💅🏻️ps, i have "both ip/ports" so no worries
 //✍🏻️
 }
 //♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️♣️
/*
check_handshake_addresses(){

 int handshake_st8 = 1000; 
 char *system_target_1[512]; // adjust the size according to your needs
 char *system_target_2[512];
 
  sprintf(system_target_1, "%s_%d.txt",my_ip, my_port);


 FILE *handshake_check = fopen(system_target_1, "r"); 
 
    if (handshake_check == NULL) {
        printf("No file found. Storing 0.\n");
        handshake_st8 = 1000 ; 
    } else {
  
      
       fscanf(handshake_check, "%d ", &handshake_st8);
    
    printf("SND]Sanity check: %d\n", handshake_st8);

      
       fclose(handshake_check);
      
    }
    /////////////////
    
      printf("Handshake value: %d\n", handshake_st8);
        FILE *file = fopen(system_target_1, "w"); // openthe file for writing
       if(handshake_st8 == 1000){
       
       
        if (file != NULL) {
            fprintf(file, "%d\n", 1000); // write 1, cuz sending syn-ack
            fclose(file);
        } else {
            printf("Error opening file: %s\n", 
system_target_1);
        }
       
       }
       
     
        //////////////rvd (syn-> sending syn-ack
         if(handshake_st8 == 1111){
         
           if (file != NULL) {
            fprintf(file, "%d\n", 2222); // write 1, cuz sending syn-ack
            fclose(file);
        } else {
            printf("Error opening file: %s\n", 
system_target_1);
        }
         
         
         }
        
        
     // the real question is "am i sender or rcvr? "
     // im gonna check current st8 of affairs... (by running prog)♣️
     
auto_send();
}
*/
 //♣️
 
 check_local_order(){
  int handshake_st8 = 9999;
   
  FILE *handshake_check = fopen("local_check.txt", "r"); 
  
     if (handshake_check == NULL) {
        printf("No file found. Storing 9999.\n");
        handshake_st8 = 9999 ; 
    } else {
  
      
    fscanf(handshake_check, "%d ", &handshake_st8);
    
    printf("0️⃣️SND]Sanity check: %d\n", handshake_st8);

      
       fclose(handshake_check);
      
    }
    
    /////////////
 FILE *file = fopen("local_check.txt", "w"); // openthe file for writing
       if(handshake_st8 == 9999){
       
              
        if (file != NULL) {
            fprintf(file, "%d\n", 8888); // write 1, cuz sending syn-ack
            fclose(file);
        } else {
            printf("Error opening file: \n");
        }
       
       
    //   return ;
       }
       
       // do we need 2 do this? is this ack send? 
          if(handshake_st8 == 8888){
         
           if (file != NULL) {
            fprintf(file, "%d\n", 7777); // write 1, cuz sending syn-ack
            fclose(file);
        } else {
            printf("Error opening file: \n");
        }
         
       //  return ; // is this right? can we delete file?
         }
         
         if(handshake_st8 == 7777){
         
        // return ; // right? delete now? ♣️
            if (file != NULL) {
            fprintf(file, "%d\n", 6666); // write 1, cuz sending syn-ack
            fclose(file);
        } else {
            printf("Error opening file: \n");
        }
         }
       
        if(handshake_st8 == 6666){
        printf("🔚️🔚️🔚️🔚️🔚️🔚️🔚️🔚️🔚️\n");
        return ;
        }
       
       auto_send();
 
 //❌️
 // as far as "rm local_check.txt" < we can wait till ack
 // (or peer timeout? )
 // or its gonna stop the other sends from sending
 // just stopping 1rst maybe good enuff 4 now...
//❌️   
 
 }
  //♣️
 
 void auto_send(){
 
 
  //write_handshake(); //oct12]🦃️nov29✖️

//♣️ (store NEW? in destination_ip / destination_port  ♣️ 
//🐡️y tho? didn't see send as arg???


    char buffer[2000] = {0};
    //Fetching port number
    int PORT_server;
    ////in ftp we hope4 📲️
     FILE *file;  //📲️


    //IN PEER WE TRUST

        int sock = 0, valread;
   // char dest_ip[16]; // assuming 15 characters for an IP address and 1 character for null termination

    // ...
  
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET; // For IPv4
    //inet_pton(AF_INET, "192.168.1.100", &serv_addr.sin_addr); // Example IP address

    inet_pton(AF_INET, destination_ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(destination_port);

    // ...
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, 
sizeof(serv_addr)) < 0) {
        perror("connect");
        printf("♣️\n");
        
        // if connect refused, was probably blah blah 
        // if local , is it first or second sender, 
        // we may keep ANOTHER]FINAL record 4 now.
        // 
        return;
    }
//}

 char hello[1024] = {0};
    /////
   

 

      file = fopen(file_0_txt, "r"); // read from the arg[] file

    if (file == NULL) {
        printf("Error opening file!\n");
        return;
    }

    char buffer_fp[2000] = {0};
    int n;

    while ((n = fread(buffer_fp, 1, sizeof(buffer_fp), file)) > 0) {
        send(sock, buffer_fp, n, 0);
    }
    
    printf("\nMessage sent {syn}]🥇️ \n");
    close(sock);
    
   

}

//////////////////////////////////////////////⏯️
void sending()
{

    char buffer[2000] = {0};
    //Fetching port number
    int PORT_server;
    ////in ftp we hope4 📲️
     FILE *file;  //📲️

  printf("Enter the destination IP address ex: 127.0.0.1 : "); //127.0.0.1
//10.0.0.16 ssh etc,

char dest_ip[16];
    scanf("%s", dest_ip);

    //IN PEER WE TRUST
    printf("Enter the port to send message:"); //Considering each peer will enter different port
    scanf("%d", &PORT_server);
    /////////
        int sock = 0, valread;
   // char dest_ip[16]; // assuming 15 characters for an IP address and 1 character for null termination

    // ...
  
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET; // For IPv4
    //inet_pton(AF_INET, "192.168.1.100", &serv_addr.sin_addr); // Example IP address

    inet_pton(AF_INET, dest_ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(PORT_server);

    // ...
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, 
sizeof(serv_addr)) < 0) {
        perror("connect");
        return;
    }
//}

 char hello[1024] = {0};
    /////
   
 

      file = fopen("chat.txt", "r"); // read from the chat.txt file

    if (file == NULL) {
        printf("Error opening file!\n");
        return;
    }

    char buffer_fp[2000] = {0};
    int n;

    while ((n = fread(buffer_fp, 1, sizeof(buffer_fp), file)) > 0) {
        send(sock, buffer_fp, n, 0);
    }
    
    printf("\nMessage sent\n");
    close(sock);
    

}



int main(int argc, char const *argv[])
{


    printf("peer.mod_1 in! \n");
   // scanf("%s", name);
   //////////////
   
   
     if (argc > 3) {
     
  ///
    strcpy(my_ip, argv[1]);
    my_port = atoi(argv[2]);
///
     strcpy(destination_ip, argv[3]); //
 destination_port = atoi(argv[4]);
file_0_txt = argv[5];
 ///llama write for argvs over 4 , store rest in array 
          printf("arg4!= %s \n",file_0_txt);
        // ...
    } else {
        printf("Error: no ip & port number specified\n");
        return 1;
    }
////////////
   
 //  sending();
 // should check handshake, one shouldnt' be sending...
// check_handshake_addresses();
check_local_order();
 //auto_send

}





