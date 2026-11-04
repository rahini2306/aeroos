#include "aero.h"
#include <assert.h>
int main(void){
 snprintf(input,sizeof(input),"{\"resources\":{\"runway_classes\":[\"heavy\",\"medium\"]},\"flights\":[{\"id\":\"A\",\"category\":\"heavy\",\"requirements\":[1,1,0,0,0,0]},{\"id\":\"B\",\"category\":\"medium\",\"requirements\":[1,1,0,0,0,0]}]}");
 int root=parse_json(),config=field(root,"resources");Flight p[100];assert(read_flights(root,p)==2);
 for(int r=0;r<6;r++){resource_pool.cap[r]=resource_pool.base[r]=2;for(int j=0;j<100;j++)resource_pool.owner[r][j]=-1;}
 assert(resource_fits(p,config,1024,0));assert(!resource_compatible(p,0,1,config));resource_acquire(p,0,config);
 assert(resource_pool.owner[0][0]==0);resource_release(p+1,1);assert(resource_pool.owner[0][0]==0);
 resource_acquire(p+1,1,config);assert(resource_pool.owner[0][1]==1);resource_release(p,0);assert(resource_pool.owner[0][0]==-1);assert(resource_pool.owner[0][1]==1);
 assert(!resource_fits(p,config,1,0));resource_release(p+1,1);assert(resource_pool.owner[0][1]==-1);
 puts("C resource ownership, capacity, memory, and compatibility assertions passed");return 0;
}
