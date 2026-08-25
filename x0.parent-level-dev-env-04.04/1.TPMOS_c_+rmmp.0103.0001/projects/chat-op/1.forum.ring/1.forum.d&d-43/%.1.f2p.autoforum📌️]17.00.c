/*
📅️sept28]
🎖️stroking into matrix strings of diff size was causing BIG PROBLEM
but we fixed it by making our own strtok. will add 2morry 🎖️
📅️

ip/port / id? will enter as arg. 
but they will still be checkd against ip_list.txt

🍍️
make ip/port send thru "send()"

then we can start handshake🤝️]i think thats done.]YES🔍️


port/ ip = destination . soo...
we done🔍️ KNOW destination (its someone in our ip_list.txt, who acks')

> will set syn-ack 2 1 in rcv if we rcv ? 
(u literally want to read and update our ip_list,..
(yea but b4 that we just wanna KNOW if we get syn-acked...
(in code) 
👩🏻‍🚀️
(is it possible 2 send a "MESSAGE" AND a file "just 4 this or w/e"?
then i can send "hash/code w/e" and get it back)

*maybe send our.ip and get back ours and theres ? or w/e
(not the whole list just our 2 ips)

>u can put "connectee on another "colum of list.txt
and if its good, maybe put a hash on yet another colum w/e
u dont have 2 go outside of the list 2 do stuff or w/e

also then u can send blockchain or w/e u needed 2 send on 3.ack)

u could even send the name of the "file u guys are editing or w/e"
in the same matrix w/e 
(hes just thinkin ahead about "FA$T vs FS.txt vs w/e")
dont. just push thru with kiss 💋️ = 🚤️ = 🍀️



*will just make "rcv next thing on ip list 4 now 
*later u can make it w/e not on ur net or something w/e

🪜️
int common_cols = 3 ; >> set to 4; *5 actually 4 port 
give initial ip 
(if none were populated? (give 0.0.0.0 for that one
(it will get a better one if its list isnt' empty. 
otherwise it doesn't even matter...
(ill put it having a port 2 send 2 just 0.0.0.0 + my_port +1 <3 🚤️
(u need its port 2 
🪜️
👩🏻‍🚀️
👩🏻‍🚀️
📌️


*/

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
FILE *fp0 = NULL;
char ***a_jagged = NULL ;
char ***b_jagged = NULL ;
char ***c_jagged = NULL ;
int rows = 0, cols = 0;
int my_pos_row = 0 ;
int port_inc = 0 ;

//char* my_ip = NULL;
int counted_rows = 1; // default by the time u use it...(but u may add 2)
int common_cols = 5; 

int my_port = 1; 

  char *my_ip = NULL ; //a_jagged[0][1];
   char *send_ip = NULL; //a_jagged[1][1];
   int send_port ; //atoi(a_jagged[1][2]);
//
#define MAX_TOKENS 1024
#define MAX_TOKEN_SIZE 64
//
char name[20];
int PORT;

void sending();  // seems we could seperate this EZ?  // leave this, but call sys()
void receiving(int server_fd); // can we print this int / pass it as arg? 
void *receive_thread(void *server_fd);

int server_fd, new_socket, valread;
////////////////🆕️
//char *user_ip = NULL ; 
/* can store in matrix, or get oll 2 make this RIGHT*/
int exit_pro(){

printf( "❎️Exit protocol engaged\n");
return 0 ;
}

int fill_a_jagged(){
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
    printf("%d tokens read.\n", token_count);
    for (int i = 0; ip_tokens[i]; i++) {
        printf("Token %d: %s\n", i, ip_tokens[i]);
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
printf("a_jagged = \n");
for (int i = 0; i < rows; i++) {
    for (int j = 0; j < cols; j++) {
        printf("%s ", a_jagged[i][j]);
    //    printf("%s ",ip_tokens[k]);
        k++;
    }
    printf("\n");
}

//free(ip_tokens);

}

//ctime(time(NULL))


//send_ack();

void auto_run(){
//]📅️oct3
// should loop be running in here and checking for ack? w/e 

// but we def auto send right here so , i called it "auto_run" w/e 

//i just set "my_port" 2 be auto . so thats done. 
// u dont send YOUR PORT, u send  target ip & target port (and file )

//📅️oct11 (coming back ,kind of disoriented... ) 
// BUT WHO WILL IT SEND BACK 2 ? 
// SO YES SEND UR IP AND PORT ALSO . W/E EZ
// ❗️ actually "SEND()" doesn't need ur ip or port 
// cuz its onlying SEND()ING one file 
// anyways u need 2 sort this out. youll get it. 


// 👩🏻‍🏭️ u may send "handshake.txt"
// 👩🏻‍🏭️<send info? ] justpush thru >
// 👩🏻‍🏭️ <REMEMBER send / rcv wont do anything MAIN.c will do it all 
//🚑️also incredibly assault 2 day may rest. 
//----------------------------------
// syn > 
// syn-ack> 
//ack > 

      char *system_target_0[512]; // adjust the size according to your needs
     //   sprintf(system_target_0, "./+x/peer.mod_1.+x %d", PORT);
   //  sprintf(system_target_0, "./+x/2.f2p.mod_1]send]0000.+x %d", PORT);
   
   
     
     // sprintf(system_target_0, "./+x/2.f2p.mod_1]send]0002.+x %s %d %s %d %s",my_port, my_ip, send_ip , send_port , "ip_list.txt");
     
      sprintf(system_target_0, "./+x/2.f2p.mod_1]send]0002.+x %s %d %s %d %s",my_ip, my_port, send_ip , send_port , "ip_list.txt");
      
   //   sprintf(system_target_0, "/.test %d", 3);
      printf("DONE \n");
      //📅️oct3 next phase = ❣️
     //❣️ how does it know which port is mine? (ip-r-rw+sort ? 
     // did it already add me 2 list? we may need 2 go back and sort this out, its fine. btw   
     
     //u can do it however, u watn. thats the problem...❣️
      
   // u dont have to send ur port. reciever can parse it from the file its gonna send
   // but w/e u can do w/e u want...
   // this fork stuff coming up just over complicates things. 
   // u can actually send , my_port, rcvr_ip, rcvr_port , filetosend.txt
   
   printf("arg = %s \n",system_target_0);
        //    system(system_target_0);
          pid_t child_pid = fork();

    if (child_pid == 0) { // child process
        system(system_target_0); // execute the command and take over CLI IO
        //📅️]sept27]
        // will go 2 else when message is sent, its ok.
        //really want 2 figure out how 2 put this in a cli , loop thing
      //📅️]sept27]
        return 0;

    } else { // parent process
        wait(NULL); // wait for the child to finish
        // continue with your program here, CLI IO is available again
    printf("\n What is this? 📅️]sept27]\n");
    // need to show cli options and take input again
    }

  printf("unforked\n");
} 
///////////////////////////////////////////////////
//🆕️
int print_menu(){
printf("\n*****At any point in time press the following:*****\n1.Send message\n2.Quit\n3.Added FX()\n");
    /*
    03. get ip list ] will populate an ip list (ip/port)
    04.3 player chat-forum (auto update auto send
    05. send message 2 all ip/ports on list 
    */
    printf("\nEnter choice: \n");
}

int main(int argc, char const *argv[]){

//system("sh sh.unbind.sh");
//📅️
//get_ip(); 
//fill_a_jagged();
//add_me();
//time_sort();
//print_a_jagged();
//debug_0();

fill_a_jagged(); //my. ip should be at top. 2nd is sendee 

 my_ip = a_jagged[0][1];
   send_ip = a_jagged[1][1];
  send_port = atoi(a_jagged[1][2]);
printf("myip/ my_port & send_ip /send_port = %s/%d & %s/%d \n", my_ip, my_port, send_ip , send_port );
//////////////////////////////////////////////

 printf("Enter your port number:");  // can handle this as a passed arg later 📃️
  //  scanf("%d", &PORT);
  scanf("%d", &my_port);
  
  
  
  //"sample text my_port more text"
  
 // please give me the code to use sprintf() to make a string containing my_port , in pure gcc c 
  
  //sudo fuser -k %d/tcp
  
  char buffer_fuser_0[126]; // adjust size as needed

    sprintf(buffer_fuser_0, "sudo fuser -k %d/tcp", my_port);
system(buffer_fuser_0);
//free(buffer_fuser_0);



// basically gonna do a "MANUAL UNBIND" of our own values here , as well. 
 char buffer_fuser_1[126];
//sudo rm 172.17.0.1_1.txt
  sprintf(buffer_fuser_1, "sudo rm %s_%d.txt", my_ip, my_port);
system(buffer_fuser_1);
//free(buffer_fuser_1);

//🚸️(im happy 2 do it this way -> send full 2 .bifs l8r, (its also "RMMV LIKE";)🚸️🎩️
//🚸️ its linux (a sandbox) <- sec]PRIV!? ⁉️ ♟️🏁️👼️an angle of hte lord...📐️🎣️🛸️👨🏽‍🚀️
//🐍️eden io...🐉️ 🥜️🔩️ 💪️🦾️
  
   char system_target_0[256];
   
   
  
  
      sprintf(system_target_0, "./+x/1.ip-r-rw+sort]0000.+x %d", my_port);
      
  
system(system_target_0);
//////

auto_run(); // using different send than "menu" its fine, its great. 
// also these modules could use the "ip_list" instead of asking me 
// ill pass them as args, but it could get them themselves if  u wanted / w/e 
// we will iron out details like this 2 taste as we go w/e 

//printf("final ip = %s\n", ip_addrs[num_ips -1]);
// then check ip against matrix.
//check_against(); // lets flup our arch notes pls. 
//📅️
//    printf("Enter name:"); // dont need a name (use wallet address) 👜️
  //  scanf("%s", name);
//sprintf(name, "wallet_0000");
//
//my_port = atoi(a_jagged[0][2]);
//printf("my_port = %d \n", my_port);
  
   //port = 88; 
   // u cant just autoport, cuz if ur local u cant be on same port
   //or can u ? ill try A: no , 
   // ASK OLL 2 handle this , EZ PZ <3 📌️
   //🪄️(automate as much as possible, shoudl beable 2 launch PLAYER1 & PLAYER2 from orch soon🪄️

    
    struct sockaddr_in address;
    int k = 0;

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    // Forcefully attaching socket to the port
    
 
    ///////////////

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
   // address.sin_port = htons(PORT); 
   address.sin_port = htons(my_port); //🚔️auto porting complete ! //havent' tested it!
   //u can test it w/o doing syn-ack, therefor u should...

    //Printed the server socket addr and port
    printf("IP address is: %s\n", inet_ntoa(address.sin_addr));
//    printf("official port is: %d\n", (int)ntohs(address.sin_port));
//////////////
  
    
    
    ////////////////////////////


    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0)
    {
        perror("bind failed");
        // close(server_fd);
         ////////
         /*
         if (shutdown(server_fd, SHUT_RDWR) == -1) {
        perror("shutdown");
        exit(EXIT_FAILURE);
    }
    */
    //////////////
         
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    
    int ch;
    pthread_t tid;
    pthread_create(&tid, NULL, &receive_thread, &server_fd); //Creating thread to keep receiving message in real time
    print_menu();
    do
    {

        scanf("%d", &ch);
        switch (ch){
        case 1:
           // sending();
           
       //    char system_target_0 = "./+x/peer.mod_1.+x" ;
           char system_target_0[256]; // adjust the size according to your needs
     //   sprintf(system_target_0, "./+x/peer.mod_1.+x %d", PORT);
   //  sprintf(system_target_0, "./+x/2.f2p.mod_1]send]0000.+x %d", PORT);
     
      sprintf(system_target_0, "./+x/2.f2p.mod_1]send]0002.+x %d", my_port);
      // this is just a list of MY ips (loc & ipv 6 x2 ;){rite now}
  // were not sending MY IP tho . we send "DESTINATION IP + port"
  // we should grab a default from the page.txt 
  // dont worry about edgecases, just push this thru fast
     //////////
   
   printf("arg = %s \n",system_target_0);
        //    system(system_target_0);
          pid_t child_pid = fork();

    if (child_pid == 0) { // child process
        system(system_target_0); // execute the command and take over CLI IO
        //📅️]sept27]
        // will go 2 else when message is sent, its ok.
        //really want 2 figure out how 2 put this in a cli , loop thing
      //📅️]sept27]
        return 0;
    } else { // parent process
        wait(NULL); // wait for the child to finish
        // continue with your program here, CLI IO is available again
    printf("\n What is this? 📅️]sept27]\n");
    // need to show cli options and take input again
    }

  printf("unforked\n");

//💪🏿️ big-corp on 
            break;
        case 2:
            printf("\nLeaving\n");
            break;
              case 3:
            printf("\n new choice 📅️]sept27]\n");
            break;
        default:
            printf("\nWrong choice\n");
        }
      print_menu();

    } while (ch);

    close(server_fd);

    return 0;
    
     atexit(exit_pro());

}

//Sending messages to port
//modded_1

//Calling receiving every 2 seconds
void *receive_thread(void *server_fd)
{

int s_fd = *((int *)server_fd);
// concat this 

//printf("s_fd : %d \n",s_fd);
////////
           char system_target_1[256]; // adjust the size according to your needs

/*✳️
can make the type of recieve conditional module.
doesn't have 2 be the same old thing, wut condition?
can read jagged from file...[its complicated but can work...]
*/
    // sprintf(system_target_1, "./+x/peer.mod_2.+x %d", s_fd);
  //  sprintf(system_target_1, "./+x/2.f2p.mod_2]rcv]0001.+x %d %s %d", s_fd,my_ip ,my_port);
    
    
    //    sprintf(system_target_0, "./+x/2.f2p.mod_1]send]0002.+x %s %whid %s %d %s",my_ip, my_port, send_ip , send_port , "ip_list.txt");
    int nov25_counter_debug = 0; 
     sprintf(system_target_1, "./+x/2.f2p.mod_2]rcv]0002.+x %d %s %d %s %d %s, #%d \n",s_fd ,my_ip, my_port, send_ip , send_port , "ip_list.txt", nov25_counter_debug);
      printf("📆️nov25 %s \n",system_target_1);
        ////////
 // while (1){
      //  receiving(s_fd);
      nov25_counter_debug++ ; 
      system(system_target_1);
  //📆️NOV25] can print 2 check if "sprintf changes" when i change it
  // essentially its gonna get sent a new "ip_list.txt"
  //🤓️loop can be in system , no need 2 keep running the sys()
  // if its in a p_thread u can kill it or w/e 
  //* theres a few diff ways 2 do this, but u need 2 pick one...
  
  // it may be that the best way will be 2 check each time
  // cuz its theoretically possible it could keep changing. 
  // 👩🏻‍🚀️if its not resending new data each rcv; change (READ IN RCV packet?)
   // wanna check? throw an incrimenter in...
   //🤓️
     //   printf("s_fd : %d \n",s_fd);
   // }
}


/*
*/


