#include "aero.h"
ResourcePool resource_pool;
bool resource_compatible(Flight *p,int r,int slot,int config){
 if(r>1)return true;int arr=field(config,r==0?"runway_classes":"gate_classes"),v=item(arr,slot);char c[24];string(v,c,sizeof(c),"all");return !strcmp(c,"all")||!strcmp(c,p->category);
}
bool resource_fits(Flight *p,int config,int memory,int used){
 if(used+p->memory>memory)return false;
 for(int r=0;r<6;r++){int free=0;for(int j=0;j<resource_pool.cap[r];j++)if(resource_pool.owner[r][j]<0&&resource_compatible(p,r,j,config))free++;if(free<p->req[r])return false;}return true;
}
void resource_acquire(Flight *p,int i,int config){for(int r=0;r<6;r++)if(p->allocated[r])return;if(!resource_fits(p,config,INT_MAX,0))return;for(int r=0;r<6;r++){int need=p->req[r];for(int j=0;j<resource_pool.cap[r]&&need;j++)if(resource_pool.owner[r][j]<0&&resource_compatible(p,r,j,config)){resource_pool.owner[r][j]=i;need--;p->allocated[r]++;}}}
void resource_release(Flight *p,int i){for(int r=0;r<6;r++){for(int j=0;j<resource_pool.base[r];j++)if(resource_pool.owner[r][j]==i)resource_pool.owner[r][j]=-1;p->allocated[r]=0;}}
