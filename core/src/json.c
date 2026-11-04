#include "aero.h"
#include <ctype.h>
_Alignas(16) char input[262144]={0}; _Alignas(16) Token tok[32768]={{0}};
static int pos,count;
static void ws(void){while(isspace((unsigned char)input[pos]))pos++;}
static int value(void){
 ws(); if(count>=32767)return -1; int n=++count; tok[n].start=pos;
 char c=input[pos++];
 if(c=='{'||c=='['){char end=c=='{'?'}':']'; ws(); while(input[pos]!=end){
   if(!input[pos])return -1; if(input[pos]==','||input[pos]==':'){pos++;continue;}
   if(value()<0)return -1; tok[n].size++; ws();
 }pos++;}
 else if(c=='"'){while(input[pos]&&input[pos]!='"'){if(input[pos]=='\\'&&input[pos+1])pos++;pos++;}if(!input[pos])return -1;pos++;}
 else {while(input[pos]&&!strchr(",]} \t\r\n",input[pos]))pos++;}
 tok[n].end=pos; return n;
}
int parse_json(void){pos=0;count=0;int n=value();ws();return input[pos]? -1:n;}
static int next(int n){int e=tok[n].end;n++;while(n<=count&&tok[n].start<e)n++;return n;}
static bool equal(int n,const char *s){return n>0&&input[tok[n].start]=='"'&&tok[n].end-tok[n].start==(int)strlen(s)+2&&!strncmp(input+tok[n].start+1,s,strlen(s));}
int field(int n,const char *s){if(n<=0)return 0;for(int k=n+1;k<=count&&tok[k].start<tok[n].end;){int v=k+1;if(equal(k,s))return v;k=next(v);}return 0;}
int item(int n,int i){if(n<=0||i<0)return 0;int k=n+1;while(i--&&k<=count)k=next(k);return k<=count&&tok[k].start<tok[n].end?k:0;}
int number(int n,int fallback){if(n<=0)return fallback;return (int)strtol(input+tok[n].start,NULL,10);}
int get(int n,const char *s,int f){return number(field(n,s),f);}
bool flag(int n,const char *s,bool f){int k=field(n,s);return k?input[tok[k].start]=='t':f;}
static unsigned hex4(int p){unsigned v=0;for(int j=0;j<4;j++){unsigned char c=(unsigned char)input[p+j];v=v*16+(c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:0);}return v;}
void string(int n,char *out,size_t size,const char *fallback){
 if(!size)return;
 if(n<=0||input[tok[n].start]!='"'){snprintf(out,size,"%s",fallback);return;}
 size_t used=0;int end=tok[n].end-1;
 for(int p=tok[n].start+1;p<end;){unsigned char c=(unsigned char)input[p++];char bytes[4];int count=1;bytes[0]=(char)c;
  if(c=='\\'&&p<end){c=(unsigned char)input[p++];bytes[0]=(char)c;
   if(c=='n')bytes[0]='\n';else if(c=='r')bytes[0]='\r';else if(c=='t')bytes[0]='\t';else if(c=='b')bytes[0]='\b';else if(c=='f')bytes[0]='\f';
   else if(c=='u'&&p+4<=end){unsigned code=hex4(p);p+=4;
    if(code>=0xd800&&code<=0xdbff&&p+6<=end&&input[p]=='\\'&&input[p+1]=='u'){unsigned low=hex4(p+2);if(low>=0xdc00&&low<=0xdfff){code=0x10000+((code-0xd800)<<10)+(low-0xdc00);p+=6;}}
    if(code<0x80){bytes[0]=(char)code;}else if(code<0x800){count=2;bytes[0]=(char)(0xc0|(code>>6));bytes[1]=(char)(0x80|(code&63));}else if(code<0x10000){count=3;bytes[0]=(char)(0xe0|(code>>12));bytes[1]=(char)(0x80|((code>>6)&63));bytes[2]=(char)(0x80|(code&63));}else{count=4;bytes[0]=(char)(0xf0|(code>>18));bytes[1]=(char)(0x80|((code>>12)&63));bytes[2]=(char)(0x80|((code>>6)&63));bytes[3]=(char)(0x80|(code&63));}
   }
  }
  if(used+(size_t)count>=size)break;memcpy(out+used,bytes,(size_t)count);used+=(size_t)count;
 }
 out[used]=0;
}
void quote(const char *s){putchar('"');for(;*s;s++){if(*s=='"'||*s=='\\')putchar('\\');if((unsigned char)*s<32)printf("\\u%04x",(unsigned char)*s);else putchar(*s);}putchar('"');}
void fail(const char *s){printf("{\"error\":");quote(s);puts("}");exit(2);}
