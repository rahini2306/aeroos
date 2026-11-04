#include "aero.h"
int main(int argc,char **argv){if(argc!=2){fprintf(stderr,"usage: aeroos schedule|airport|banker|deadlock|memory|sync|modelcheck < input.json\n");return 1;}
 size_t n=fread(input,1,sizeof(input)-1,stdin);input[n]=0;if(n==sizeof(input)-1)fail("Input too large");int root=parse_json();if(root<1||input[tok[root].start]!='{')fail("Invalid JSON object");
 if(!strcmp(argv[1],"schedule"))scheduler(root,false);else if(!strcmp(argv[1],"airport"))scheduler(root,true);else if(!strcmp(argv[1],"banker"))banker(root,false);else if(!strcmp(argv[1],"deadlock"))banker(root,true);else if(!strcmp(argv[1],"memory"))memory_lab(root);else if(!strcmp(argv[1],"sync"))sync_lab(root);else if(!strcmp(argv[1],"modelcheck"))model_check(root);else fail("Unknown command");return 0;}
