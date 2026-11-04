#include "aero.h"
typedef struct {int start,size,owner;} Block;
void memory_lab(int root){char mode[24],alg[24];string(field(root,"mode"),mode,sizeof(mode),"paging");string(field(root,"algorithm"),alg,sizeof(alg),!strcmp(mode,"paging")?"lru":"first");
 if(!strcmp(mode,"paging")){
  if(strcmp(alg,"fifo")&&strcmp(alg,"lru")&&strcmp(alg,"optimal"))fail("Unknown replacement policy");
  int process_bytes=get(root,"process_bytes",0);if(process_bytes<0||process_bytes>100000000)fail("Invalid process size");
  int count=get(root,"frames",3),page_size=get(root,"page_size",256),refs=field(root,"references"),n=tok[refs].size;if(count<1||count>64||page_size<1||page_size>65536||n<1||n>1000)fail("Invalid paging configuration");
  int frame[64],last[64],loaded[64],faults=0,replacements=0;for(int i=0;i<count;i++){frame[i]=-1;last[i]=-1;loaded[i]=-1;}
  printf("{\"steps\":[");for(int k=0;k<n;k++){int page=number(item(refs,k),-1);if(page<0||page>1000000||(process_bytes&&page>=(process_bytes+page_size-1)/page_size))fail("Invalid page number");int slot=-1,evicted=-1;bool fault=true;for(int i=0;i<count;i++)if(frame[i]==page){slot=i;fault=false;break;}
   if(fault){faults++;for(int i=0;i<count;i++)if(frame[i]<0){slot=i;break;}if(slot<0){slot=0;if(!strcmp(alg,"optimal")){int far=-1;for(int i=0;i<count;i++){int next=n;for(int j=k+1;j<n;j++)if(number(item(refs,j),-1)==frame[i]){next=j;break;}if(next>far){far=next;slot=i;}}}else{for(int i=1;i<count;i++)if((!strcmp(alg,"lru")?last[i]:loaded[i])<(!strcmp(alg,"lru")?last[slot]:loaded[slot]))slot=i;}evicted=frame[slot];replacements++;}frame[slot]=page;loaded[slot]=k;}last[slot]=k;
   printf("%s{\"page\":%d,\"fault\":%s,\"slot\":%d,\"evicted\":%d,\"frames\":[",k?",":"",page,fault?"true":"false",slot,evicted);for(int i=0;i<count;i++)printf("%s%d",i?",":"",frame[i]);printf("]}");
  }
  int address=get(root,"logical_address",0);if(process_bytes&&address>=process_bytes)fail("Logical address outside process");if(address<0)fail("Negative address");int page=address/page_size,offset=address%page_size,physical=-1;for(int i=0;i<count;i++)if(frame[i]==page)physical=i*page_size+offset;
  printf("],\"page_faults\":%d,\"replacements\":%d,\"fault_rate\":%.5f,\"thrashing_indicator\":%s,\"translation\":{\"logical\":%d,\"page\":%d,\"offset\":%d,\"physical\":%d},\"internal_fragmentation\":%d,\"process_bytes\":%d,\"note\":\"Optimal is an offline benchmark. Thrashing indicator means a fault rate over 70 percent; it is a heuristic. Specify process_bytes to measure last-page internal fragmentation; default assumes fully used pages.\"}\n",faults,replacements,(double)faults/n,(double)faults/n>0.7?"true":"false",address,page,offset,physical,process_bytes?(page_size-process_bytes%page_size)%page_size:0,process_bytes);
 }else if(!strcmp(mode,"contiguous")){
  if(strcmp(alg,"first")&&strcmp(alg,"best")&&strcmp(alg,"worst")&&strcmp(alg,"next"))fail("Unknown allocation policy");
  int size=get(root,"size",1024),ops=field(root,"operations"),n=tok[ops].size;if(size<1||size>65536||n>500)fail("Invalid memory workload");Block b[1100]={{0,size,-1}};int nb=1,cursor=0,failed=0;printf("{\"steps\":[");
  for(int k=0;k<n;k++){int op=item(ops,k),pid=get(op,"process",k),bytes=get(op,"size",64);char action[16];string(field(op,"action"),action,sizeof(action),"allocate");bool success=false;
   if(!strcmp(action,"free")||!strcmp(action,"swap")){for(int j=0;j<nb;j++)if(b[j].owner==pid){b[j].owner=-1;success=true;}for(int j=0;j<nb-1;)if(b[j].owner<0&&b[j+1].owner<0){b[j].size+=b[j+1].size;memmove(b+j+1,b+j+2,(size_t)(nb-j-2)*sizeof(Block));nb--;}else j++;}
   else if(!strcmp(action,"compact")){Block fresh[1100];int end=0,c=0;for(int j=0;j<nb;j++)if(b[j].owner>=0){fresh[c++]=(Block){end,b[j].size,b[j].owner};end+=b[j].size;}if(end<size)fresh[c++]=(Block){end,size-end,-1};memcpy(b,fresh,(size_t)c*sizeof(Block));nb=c;cursor=end;success=true;}
   else if(!strcmp(action,"allocate")){if(bytes<1||bytes>size||pid<0)fail("Invalid allocation");int chosen=-1;bool duplicate=false;for(int j=0;j<nb;j++)if(b[j].owner==pid)duplicate=true;
    if(!duplicate)for(int j=0;j<nb;j++)if(b[j].owner<0&&b[j].size>=bytes){if(chosen<0)chosen=j;else if(!strcmp(alg,"best")&&b[j].size<b[chosen].size)chosen=j;else if(!strcmp(alg,"worst")&&b[j].size>b[chosen].size)chosen=j;else if(!strcmp(alg,"next")){int d=(b[j].start-cursor+size)%size,old=(b[chosen].start-cursor+size)%size;if(d<old)chosen=j;}}
    if(!duplicate&&!strcmp(alg,"next"))for(int j=0;j<nb;j++)if(b[j].owner<0&&cursor>=b[j].start&&cursor+bytes<=b[j].start+b[j].size){chosen=j;if(cursor>b[j].start){Block old=b[j];memmove(b+j+2,b+j+1,(size_t)(nb-j-1)*sizeof(Block));b[j]=(Block){old.start,cursor-old.start,-1};b[j+1]=(Block){cursor,old.size-(cursor-old.start),-1};nb++;chosen=j+1;}break;}
    if(chosen>=0){Block old=b[chosen];if(old.size>bytes){memmove(b+chosen+2,b+chosen+1,(size_t)(nb-chosen-1)*sizeof(Block));b[chosen+1]=(Block){old.start+bytes,old.size-bytes,-1};nb++;}b[chosen]=(Block){old.start,bytes,pid};cursor=(old.start+bytes)%size;success=true;}else failed++;
   }else fail("Unknown memory operation");
   printf("%s{\"action\":",k?",":"");quote(action);printf(",\"process\":%d,\"success\":%s,\"blocks\":[",pid,success?"true":"false");for(int j=0;j<nb;j++)printf("%s{\"start\":%d,\"size\":%d,\"owner\":%d}",j?",":"",b[j].start,b[j].size,b[j].owner);printf("]}");
  }
  int free=0,largest=0;for(int j=0;j<nb;j++)if(b[j].owner<0){free+=b[j].size;if(b[j].size>largest)largest=b[j].size;}printf("],\"free\":%d,\"external_fragmentation\":%d,\"internal_fragmentation\":0,\"failed_allocations\":%d,\"utilization\":%.5f}\n",free,free-largest,failed,(double)(size-free)/size);
 }else fail("Unknown memory mode");
}
