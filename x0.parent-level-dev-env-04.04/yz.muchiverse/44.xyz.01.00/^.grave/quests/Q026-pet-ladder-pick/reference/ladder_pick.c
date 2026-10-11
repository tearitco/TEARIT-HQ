#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void trim(char *s){char*p=s;while(*p==' '||*p=='\t')p++;memmove(s,p,strlen(p)+1);size_t n=strlen(s);while(n&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n'))s[--n]=0;}
static int split(char*l,char**f,int max){int n=0;char*p=l;for(;n<max;){f[n++]=p;char*b=strchr(p,'|');if(!b)break;*b=0;p=b+1;}for(int i=0;i<n;i++)trim(f[i]);return n;}
int main(int c,char**v){ if(c!=4)return 2; char*e;long now=strtol(v[3],&e,10); if(*e||!v[3][0])return 2; long day=now/86400;
 FILE*f=fopen(v[1],"r"); if(!f)return 2; char l[1024]; char nm[64][64]; long ord[64],bud[64],used[64],cool[64]; int n=0;
 while(fgets(l,sizeof l,f)){trim(l);if(!l[0]||l[0]=='#')continue;char*fl[8];int k=split(l,fl,8);if(k>=4&&!strcmp(fl[0],"PROVIDER")&&n<64){snprintf(nm[n],64,"%s",fl[1]);ord[n]=atol(fl[2]);bud[n]=atol(fl[3]);used[n]=0;cool[n]=0;n++;}} fclose(f);
 f=fopen(v[2],"r"); if(f){while(fgets(l,sizeof l,f)){trim(l);if(!l[0]||l[0]=='#')continue;char*fl[8];int k=split(l,fl,8);for(int i=0;i<n;i++)if(k>=4&&!strcmp(fl[0],"USED")&&!strcmp(fl[1],nm[i])&&atol(fl[2])==day)used[i]+=atol(fl[3]);else if(k>=3&&!strcmp(fl[0],"COOLDOWN")&&!strcmp(fl[1],nm[i])&&atol(fl[2])>cool[i])cool[i]=atol(fl[2]);}fclose(f);}
 int best=-1;for(int i=0;i<n;i++){if(bud[i]<=0||used[i]>=bud[i]||now<cool[i])continue;if(best<0||ord[i]<ord[best])best=i;}
 if(best<0){puts("NONE|exhausted");return 1;} printf("PICK|%s\n",nm[best]);return 0;}
