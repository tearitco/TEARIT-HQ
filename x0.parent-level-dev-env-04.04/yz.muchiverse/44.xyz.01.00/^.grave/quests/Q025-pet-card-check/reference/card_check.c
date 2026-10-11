#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
static void trim(char *s){char*p=s;while(*p==' '||*p=='\t')p++;memmove(s,p,strlen(p)+1);size_t n=strlen(s);while(n&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n'))s[--n]=0;}
int main(int c,char**v){ if(c!=2)return 2; FILE*f=fopen(v[1],"r"); if(!f)return 2; char l[4096]; int ln=0,ok=0,bad=0; char ids[512][32]; int ni=0;
 while(fgets(l,sizeof l,f)){ln++; trim(l); if(!l[0]||l[0]=='#')continue; const char*why=NULL; char*fl[8];int nf=0; char*p=l; if(strncmp(l,"CARD",4)){why="notcard";goto out;}
  for(;nf<8;){fl[nf++]=p;char*b=strchr(p,'|');if(!b)break;*b=0;p=b+1;} for(int i=0;i<nf;i++)trim(fl[i]);
  if(nf!=6||strcmp(fl[0],"CARD")){why="fields";goto out;}
  {size_t n=strlen(fl[1]);int g=n>=3&&n<=24;for(size_t i=0;g&&i<n;i++)if(!(islower((unsigned char)fl[1][i])||isdigit((unsigned char)fl[1][i])||fl[1][i]=='_'))g=0;if(!g){why="id";goto out;}}
  if(strcmp(fl[2],"word")&&strcmp(fl[2],"fact")&&strcmp(fl[2],"howto")){why="kind";goto out;}
  {size_t n=strlen(fl[3]);if(n<3||n>60){why="prompt";goto out;}} {size_t n=strlen(fl[4]);if(n<1||n>80){why="answer";goto out;}}
  if(strcmp(fl[5],"gemma")&&strcmp(fl[5],"groq")&&strcmp(fl[5],"poolside")&&strcmp(fl[5],"openrouter")&&strcmp(fl[5],"human")){why="source";goto out;}
  for(int k=3;k<=4;k++){for(char*q=fl[k];*q;q++)if((unsigned char)*q<32||(unsigned char)*q>126){why="charset";goto out;} char lo[200];size_t i=0;for(;fl[k][i]&&i<199;i++)lo[i]=(char)tolower((unsigned char)fl[k][i]);lo[i]=0;if(strstr(lo,"http")){why="url";goto out;}}
  for(int i=0;i<ni;i++)if(!strcmp(ids[i],fl[1])){why="dup";goto out;} if(ni<512)snprintf(ids[ni++],32,"%s",fl[1]);
  out: if(why){printf("BAD|%d|%s\n",ln,why);bad++;}else{printf("OK|%d\n",ln);ok++;} }
 printf("SUMMARY|ok=%d|bad=%d\n",ok,bad); return bad?1:0; }
