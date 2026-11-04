#include "aero.h"
const char *states[]={"NEW","READY","RUNNING","WAITING","BLOCKED","COMPLETED","CANCELLED"};
int read_flights(int root,Flight *p){int a=field(root,"flights"),n=tok[a].size;if(n<1||n>MAX_P)fail("Provide 1 to 100 flights");
 for(int i=0;i<n;i++){int f=item(a,i);memset(p+i,0,sizeof(*p));char id[40];snprintf(id,sizeof(id),"FL%03d",i+1);string(field(f,"id"),p[i].id,sizeof(p[i].id),id);string(field(f,"airline"),p[i].airline,sizeof(p[i].airline),"AeroOS");string(field(f,"category"),p[i].category,sizeof(p[i].category),"medium");
 p[i].arrival=get(f,"arrival",0);p[i].departure=get(f,"departure",p[i].arrival+60);p[i].burst=get(f,"burst",5);p[i].remaining=p[i].burst;p[i].priority=get(f,"priority",3);p[i].effective=p[i].priority;p[i].emergency=flag(f,"emergency",false);p[i].memory=get(f,"memory",32);p[i].first=-1;p[i].completion=-1;
 if(p[i].arrival<0||p[i].arrival>1000||p[i].burst<1||p[i].burst>100||p[i].memory<1||p[i].memory>65536||p[i].priority<0||p[i].priority>99)fail("Flight values outside supported bounds");
 int req=field(f,"requirements");for(int r=0;r<6;r++){p[i].req[r]=number(item(req,r),r<2?1:0);if(p[i].req[r]<0||p[i].req[r]>100)fail("Invalid resource requirement");}
 for(int j=0;j<i;j++)if(!strcmp(p[j].id,p[i].id))fail("Flight IDs must be unique");
 }return n;
}
