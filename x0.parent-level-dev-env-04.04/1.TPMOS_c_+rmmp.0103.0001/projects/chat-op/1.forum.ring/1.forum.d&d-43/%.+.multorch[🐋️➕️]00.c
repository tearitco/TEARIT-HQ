//☎️ just pass "my_ip" as "arg[1] & 2 or auto inc them in orc w/e //☎️ 


#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// this is "LOTS OF STUFF HAPPENING"
// try 2 walk thru it like the "MACRO_EPOCH" is supposed 2 
// and make sure things are going ok
/*
🤫️🤫️🤫️
🤫️😭️🤫️
🤫️🤫️🤫️

*/
// i cant wait to change "corpus" to vocab
// query to corpus
// and output to query 

#define EPOCHS 1
int main(int argc, char *argv[]) {
FILE *fp = fopen("f$+/answer_i.txt", "w"); // clears old answers
FILE *fp1 = fopen("f$+/answer_ii.txt", "w"); // clears old answers
///
FILE *fp2 = fopen("f$+/bpweights_0.txt", "w"); // clears old weiths
FILE *fp3 = fopen("f$+/bpbias_0.txt", "w"); // clears old bias
///

//🦘️this is weird. should we "ONLY USE VOCAB" as "CORPUS"
//🦘️ there telling u 2 make both , then pull probabilites from vocab...
//🦘️ but theres not embeddings for vocab... {gonna look @ code}

printf(":./+x/0.sol.count]2]xp]ON+.+x✔️\n");
system("./+x/0.sol.count]2]xp]ON+.+x -1 f$+/corpus.txt -2 f$+/index_0.txt"); //.txt only

//3️⃣️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️3️⃣️🦸‍♀️️
// i think index is supposed to be concated 2 corpus 
// u can even concat sentpieces 2 corpus as well for "super_corpus.txt" 🦸‍♀️️
// it gives corpus embeddings more power im sure
//3️⃣️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️🦙️3️⃣️🦸‍♀️️
//💪🏿️ big-corp on 

 
///////thats not it either? 
// this is an important step. 
// u should even train embeddings 
//printf(":./+x/1.cbow]1]NOW!🏹️.+x✔️\n");
//system("./+x/1.cbow]1]NOW!🏹️.+x -1 f$+/corpus.txt -2 f$+/query_embeds_0.txt");  //.txt only

printf(":./+x/1.6.query.embed]7.+x✔️\n");
system("./+x/1.6.query.embed]7.+x -1 f$+/corpus.txt -2 f$+/query_embeds_0.txt");  //.txt only

//💪🏿️ big-corp on 



// having 2 redo
//https://medium.com/@louiserigny/a-guide-to-understanding-positional-encoding-for-deep-learning-models-fdea4ee938f3
printf(":./+x/2.pe]FIX]ON+.+x✔️\n");  //.txt only
system("./+x/2.pe]FIX]ON+.+x -1 f$+/corpus.txt -2 f$+/positional_encoding_0.txt");  //.txt only
//💪🏿️ big-corp on 
  

///🥷️(sneaky cuz this one has "-tags"🔖️
printf(":3.concat]0]ON+.+x\n");  //.txt only
system("./+x/3.concat]0]ON+.+x -1 f$+/query_embeds_0.txt -2 f$+/positional_encoding_0.txt -3 f$+/resultant_0.txt");  //the other concat doesn't work , it never did!

//💪🏿️ big-corp on 

 //🇦🇷️arg swap still NECESSARY (4)
 printf(":./+x/4.encode_qkv]32]+.+x✔️\n");  //.txt only
  system("./+x/4.encode_qkv]32]+.+x -1 f$+/resultant_0.txt -2 f$+/q_0.txt -3 f$+/k_0.txt -4 f$+/v_0.txt");  //.txt only
//

//💪🏿️ big-corp on 


  // no mask
  //⬛️
   printf(":./+x/4.mMHA]VA5]1]0001.+x✔️\n");  //.txt only
  system("./+x/4.mMHA]VA5]1]0001.+x -1 f$+/q_0.txt -2 f$+/k_0.txt -3 f$+/v_0.txt -4 f$+/mha_0.txt -5 f$+/resultant_0.txt --");
  //⛳️
 //💪🏿️ big-corp on 
//  exit(111); //🔚️
 printf(":./+x/5.add_n_norm]+]0000.+x✔️\n");  //.txt only
  system("./+x/5.add_n_norm]+]0000.+x -1 f$+/mha_0.txt -2 f$+/resultant_0.txt -3 f$+/add_n_norm_0a.txt");
  
  //💪🏿️ big-corp on 

  //////
    printf("🚆️\n");
  printf(":./+x/6.feedforw]2]0004.+x✔️\n");
  system("./+x/6.feedforw]2]0004.+x -1 f$+/add_n_norm_0a.txt -2 f$+/ff_0.txt");
  //6.ff+bp]3]NU🦸🏻️]0000
  
 /*
  printf(":./+x/6.ff+bp]3]NU🦸🏻️]0001.+x✔️\n");
  system("./+x/6.ff+bp]3]NU🦸🏻️]0001.+x -1 f$+/add_n_norm_0a.txt -2 f$+/ff_0.txt -3 bpweights_0.txt -4 bpbias_0.txt --");
  */

 // printf("🚆️\n");
    //exit(110); //🔚️
   //💪🏿️ big-corp on 
 
 printf(":./+x/5.add_n_norm]+]0000.+x✔️\n");  //.txt only
  system("./+x/5.add_n_norm]+]0000.+x -1 f$+/ff_0.txt -2 f$+/add_n_norm_0a.txt -3 inference🤖️🧠️labs/f$+/add_n_norm_0b.txt");
  
  //🔰️
  //////
  ////
  
   //💪🏿️ big-corp on 
exit(111); //🔚️
  ////🚆️
   
 }
///🥷️🥷🏿️

  



