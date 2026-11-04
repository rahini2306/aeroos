#include "aero.h"
typedef struct {int p,start,end,lane;} Slice;
static Slice *slices;
static int blocked_wait[MAX_P],queue_stamp[MAX_P];
#define owner resource_pool.owner
#define cap resource_pool.cap
#define base resource_pool.base
static Flight *process_base;
static const char *names[]={"runways","gates","baggage","maintenance","fuel","crew"};
static int rank_of(Flight *p,const char *alg,int t,bool aging,int age){
 (void)t;
 int effective=p->priority-(aging?(p->waiting-blocked_wait[p-process_base])/age:0);if(effective<0)effective=0;p->effective=effective;
 if(!strcmp(alg,"priority")||!strcmp(alg,"priority_preemptive"))return effective;
 if(!strcmp(alg,"sjf"))return p->burst;
 if(!strcmp(alg,"srtf"))return p->remaining;
 if(!strcmp(alg,"mlq"))return p->priority/3;
 if(!strcmp(alg,"mlfq"))return p->level;
 return 0;
}
static bool before(Flight *a,Flight *b,const char *alg,int t,bool aging,int age){
 if(a->emergency!=b->emergency)return a->emergency>b->emergency;
 if(!strcmp(alg,"hrrn")){double ar=1.0+(double)a->waiting/a->burst,br=1.0+(double)b->waiting/b->burst;if(ar!=br)return ar>br;}
 int ar=rank_of(a,alg,t,aging,age),br=rank_of(b,alg,t,aging,age);if(ar!=br)return ar<br;
 if(!strcmp(alg,"mlfq")&&queue_stamp[a-process_base]!=queue_stamp[b-process_base])return queue_stamp[a-process_base]<queue_stamp[b-process_base];
 if(a->arrival!=b->arrival)return a->arrival<b->arrival;return strcmp(a->id,b->id)<0;
}
void scheduler(int root,bool airport){
 slices=calloc(100000,sizeof(Slice));if(!slices)fail("Cannot allocate timeline");
 Flight p[MAX_P];process_base=p;int n=read_flights(root,p);for(int i=0;i<n;i++)queue_stamp[i]=p[i].arrival*1000;char alg[32];string(field(root,"algorithm"),alg,sizeof(alg),"fcfs");const char *valid[]={"fcfs","sjf","srtf","priority","priority_preemptive","rr","hrrn","mlq","mlfq"};bool ok=false;for(int a=0;a<9;a++)if(!strcmp(alg,valid[a]))ok=true;if(!ok)fail("Unknown scheduling algorithm");
 if(airport&&(!strcmp(alg,"srtf")||!strcmp(alg,"priority_preemptive")||!strcmp(alg,"rr")||!strcmp(alg,"mlfq")))fail("Preemptive policies are restricted to abstract tasks");
 for(int i=0;i<n;i++)if(airport&&p[i].req[0]!=1)fail("Airport operations must require exactly one runway");
 int config=field(root,"resources"),defaults[]={3,6,4,3,2,5};for(int r=0;r<6;r++){cap[r]=base[r]=get(config,names[r],defaults[r]);if(cap[r]<0||cap[r]>100)fail("Resource capacity must be 0..100");for(int j=0;j<100;j++)owner[r][j]=-1;}
 if(get(config,"max_flights",100)<1||get(config,"max_flights",100)>100||n>get(config,"max_flights",100))fail("Configured flight limit exceeded");
 int memory=get(config,"memory",1024),q=get(root,"quantum",2),age=get(root,"aging_interval",5),threshold=get(root,"starvation_threshold",15);if(memory<1||memory>65536||q<1||q>100||age<1||threshold<1)fail("Invalid simulation configuration");
 bool aging=flag(root,"aging",false);int total=0;for(int i=0;i<n;i++)total+=p[i].burst;if(total>3000)fail("Total execution demand exceeds 3000 ticks");
 int event_id=0;int used=0,done=0,cancelled=0,ns=0,previous=-1,switches=0,t=0;long queue_area=0;int peak_queue=0,failed_requests=0;
 long busy[6]={0},contention[6]={0};bool resource_waiters[6][MAX_P]={{false}};int maxqueue[6]={0};bool timedout=false;int faults=field(root,"faults");
 for(int k=0;k<tok[faults].size;k++){int f=item(faults,k);if(get(f,"resource",0)<0||get(f,"resource",0)>=6||get(f,"time",0)<0||get(f,"time",0)>1000||get(f,"duration",10)<1||get(f,"duration",10)>1000||get(f,"reduction",1)<0||get(f,"reduction",1)>100)fail("Invalid fault configuration");}
 printf("{\"context\":\"%s\",\"algorithm\":",airport?"airport":"tasks");quote(alg);printf(",\"snapshots\":[");
 for(t=0;t<=MAX_T;t++){
  if(t)putchar(',');
  for(int r=0;r<6;r++)cap[r]=base[r];
  for(int k=0;k<tok[faults].size;k++){int f=item(faults,k),time=get(f,"time",0),dur=get(f,"duration",10),r=get(f,"resource",0),reduction=get(f,"reduction",1);if(r>=0&&r<6&&t>=time&&t<time+dur){cap[r]-=reduction;if(cap[r]<0)cap[r]=0;}}
  int events[MAX_P*3],old[MAX_P*3],target[MAX_P*3],ne=0;
  for(int i=0;i<n;i++){
   if(p[i].state==RUNNING&&p[i].remaining==0){old[ne]=RUNNING;target[ne]=COMPLETED;events[ne++]=i;p[i].state=COMPLETED;p[i].completion=t;done++;used-=p[i].memory;if(airport)resource_release(p+i,i);}
   if(p[i].state==NEW&&p[i].arrival<=t){old[ne]=NEW;target[ne]=READY;events[ne++]=i;p[i].state=READY;}
   int cancel_at=get(item(field(root,"flights"),i),"cancel_at",-1);if(cancel_at>=0&&t>=cancel_at&&p[i].state!=COMPLETED&&p[i].state!=CANCELLED){old[ne]=p[i].state;target[ne]=CANCELLED;events[ne++]=i;if(p[i].state==RUNNING){used-=p[i].memory;if(airport)resource_release(p+i,i);}p[i].state=CANCELLED;cancelled++;}
   rank_of(p+i,alg,t,aging,age);
  }
  // A preemption releases the abstract CPU only; airport dispatch never interrupts a runway.
  if(!airport){for(int i=0;i<n;i++)if(p[i].state==RUNNING){bool pre=false;
   if(!strcmp(alg,"rr")&&p[i].used>=q)pre=true;
   if(!strcmp(alg,"mlfq")&&p[i].used>=q*(1<<p[i].level)){pre=true;if(p[i].level<2)p[i].level++;p[i].used=0;}
   if(!strcmp(alg,"srtf")||!strcmp(alg,"priority_preemptive")||!strcmp(alg,"mlq")||!strcmp(alg,"mlfq"))for(int j=0;j<n;j++)if(p[j].state==READY&&before(p+j,p+i,alg,t,aging,age))pre=true;
   if(pre){old[ne]=RUNNING;target[ne]=READY;events[ne++]=i;p[i].state=READY;if(strcmp(alg,"mlfq"))p[i].used=0;queue_stamp[i]=t*1000+100+i;used-=p[i].memory;}
  }}
  int selected[MAX_P],selected_from[MAX_P],alternatives[MAX_P][MAX_P],alt_count[MAX_P]={0},deferred[MAX_P][MAX_P],defer_count[MAX_P]={0},avail_before[MAX_P][6],mem_before[MAX_P],sel=0;bool considered[MAX_P]={false};
  for(int pass=0;pass<n;pass++){
   if(!airport){bool running=false;for(int i=0;i<n;i++)if(p[i].state==RUNNING)running=true;if(running)break;}
   int best=-1;
   for(int i=0;i<n;i++)if((p[i].state==READY||p[i].state==BLOCKED)&&!considered[i]){
    if(!airport&&!strcmp(alg,"rr")){
     // Circular ready-queue order is maintained by enqueue stamps below.
     if(best<0||p[i].emergency>p[best].emergency||(p[i].emergency==p[best].emergency&&queue_stamp[i]<queue_stamp[best]))best=i;
    }else if(best<0||before(p+i,p+best,alg,t,aging,age))best=i;
   }
   if(best<0)break;considered[best]=true;
   bool available=airport?resource_fits(p+best,config,memory,used):true;
   if(!available){failed_requests++;if(p[best].state!=BLOCKED){old[ne]=p[best].state;target[ne]=BLOCKED;events[ne++]=best;}p[best].state=BLOCKED;continue;}
   selected_from[sel]=p[best].state;mem_before[sel]=memory-used;for(int r=0;r<6;r++){avail_before[sel][r]=0;for(int j=0;j<cap[r];j++)if(owner[r][j]<0)avail_before[sel][r]++;}for(int j=0;j<n;j++)if(j!=best&&(p[j].state==READY||p[j].state==BLOCKED)){if(!airport||resource_fits(p+j,config,memory,used))alternatives[sel][alt_count[sel]++]=j;else deferred[sel][defer_count[sel]++]=j;}
   p[best].state=RUNNING;if(p[best].first<0)p[best].first=t;if(strcmp(alg,"mlfq"))p[best].used=0;used+=p[best].memory;if(airport)resource_acquire(p+best,best,config);
   selected[sel++]=best;
   for(int i=0;i<n;i++)if(i!=best&&(p[i].state==READY||p[i].state==BLOCKED))p[i].bypassed++;
  }
  int queued[6]={0};for(int i=0;i<n;i++)if(p[i].state==BLOCKED)for(int r=0;r<6;r++)if(p[i].req[r]){int free=0;for(int j=0;j<cap[r];j++)if(owner[r][j]<0&&resource_compatible(p+i,r,j,config))free++;if(free<p[i].req[r]){queued[r]++;resource_waiters[r][i]=true;}}
  printf("{\"time\":%d,\"memory_used\":%d,\"processes\":[",t,used);
  for(int i=0;i<n;i++){if(i)putchar(',');printf("[%d,%d,%d,%d,%d,%d,%d,%d",p[i].state,p[i].remaining,p[i].waiting,p[i].first,p[i].completion,p[i].effective,p[i].bypassed,blocked_wait[i]);for(int r=0;r<6;r++)printf(",%d",p[i].allocated[r]);putchar(']');}printf("],\"owners\":[");
  for(int r=0;r<6;r++){if(r)putchar(',');putchar('[');for(int j=0;j<base[r];j++){if(j)putchar(',');printf("%d",owner[r][j]);}putchar(']');}printf("],\"capacity\":[");for(int r=0;r<6;r++)printf("%s%d",r?",":"",cap[r]);printf("],\"queues\":[");for(int r=0;r<6;r++){if(queued[r]>maxqueue[r])maxqueue[r]=queued[r];printf("%s%d",r?",":"",queued[r]);}printf("],\"events\":[");
  bool comma=false;for(int e=0;e<ne;e++){if(comma)putchar(',');comma=true;printf("{\"id\":%d,\"timestamp\":%d,\"type\":\"state_transition\",\"process\":%d,\"from\":",event_id++,t,events[e]);quote(states[old[e]]);printf(",\"to\":");quote(states[target[e]]);printf(",\"reason\":\"%s\"}",old[e]==NEW?"Arrival admitted to ready queue":target[e]==COMPLETED?"Service complete; owned resources released":target[e]==CANCELLED?"Cancellation applied; owned resources released":target[e]==BLOCKED?"Deferred: compatible resource slots or memory are unavailable":"Abstract CPU preempted; process re-enqueued");}
  for(int e=0;e<sel;e++){int i=selected[e];if(comma)putchar(',');comma=true;printf("{\"id\":%d,\"timestamp\":%d,\"type\":\"dispatch\",\"process\":%d,\"from\":",event_id++,t,i);quote(states[selected_from[e]]);printf(",\"to\":\"RUNNING\",\"algorithm\":");quote(alg);printf(",\"reason\":");char reason[220];snprintf(reason,sizeof(reason),"%s selected by %s%s; effective priority %d, remaining %d; %s",p[i].id,alg,p[i].emergency?" emergency precedence":"",p[i].effective,p[i].remaining,airport?"compatible resources and memory available":"abstract CPU available");quote(reason);printf(",\"alternatives\":[");for(int j=0;j<alt_count[e];j++)printf("%s%d",j?",":"",alternatives[e][j]);printf("],\"deferred\":[");for(int j=0;j<defer_count[e];j++)printf("%s%d",j?",":"",deferred[e][j]);printf("],\"resource_availability\":[");for(int r=0;r<6;r++)printf("%s%d",r?",":"",avail_before[e][r]);printf("],\"memory_available\":%d}",mem_before[e]);}
  for(int k=0;k<tok[faults].size;k++){int f=item(faults,k),time=get(f,"time",0),duration=get(f,"duration",10),r=get(f,"resource",0);if(t==time||t==time+duration){if(comma)putchar(',');comma=true;printf("{\"id\":%d,\"timestamp\":%d,\"type\":\"resource_change\",\"process\":-1,\"to\":\"RESOURCE_CHANGE\",\"resource\":%d,\"reason\":",event_id++,t,r);char note[180];snprintf(note,sizeof(note),"%s capacity %s: %d slots of admission capacity; active owners retain their allocations",names[r],t==time?"reduced":"restored",cap[r]);quote(note);putchar('}');}}
  printf("]}");
  int observed_queue=0;for(int i=0;i<n;i++)if(p[i].state==READY||p[i].state==BLOCKED)observed_queue++;if(observed_queue>peak_queue)peak_queue=observed_queue;
  if(done+cancelled==n)break;
  if(t==MAX_T){timedout=true;break;}
  bool can_run=false,future=false;for(int i=0;i<n;i++){if(p[i].state==RUNNING)can_run=true;if(p[i].state==NEW)future=true;}for(int k=0;k<tok[faults].size;k++){int f=item(faults,k);if(get(f,"time",0)+get(f,"duration",10)>t)future=true;}if(!can_run&&!future){timedout=true;break;}
  int queued_total=0;for(int i=0;i<n;i++)if(p[i].state==READY||p[i].state==BLOCKED)queued_total++;queue_area+=queued_total;if(queued_total>peak_queue)peak_queue=queued_total;
  int run=-1;
  for(int i=0;i<n;i++){
   if(p[i].state==RUNNING){run=i;int lane=0;if(airport)for(int j=0;j<base[0];j++)if(owner[0][j]==i){lane=j;break;}
    int found=-1;for(int s=ns-1;s>=0&&s>=ns-n;s--)if(slices[s].p==i&&slices[s].end==t&&slices[s].lane==lane){found=s;break;}
    if(found>=0)slices[found].end++;else if(ns<100000)slices[ns++]=(Slice){i,t,t+1,lane};p[i].remaining--;p[i].used++;
   }else if(p[i].state==READY||p[i].state==BLOCKED){p[i].waiting++;if(p[i].state==BLOCKED)blocked_wait[i]++;}
  }
  if(!airport){if(run>=0&&previous>=0&&run!=previous)switches++;if(run>=0)previous=run;
   // FIFO stamps: new arrivals enter before a task whose quantum just expired.
   
  }
  for(int r=0;r<6;r++){contention[r]+=queued[r];for(int j=0;j<base[r];j++)if(owner[r][j]>=0)busy[r]++;}

 }
 printf("],\"gantt\":[");for(int s=0;s<ns;s++){if(s)putchar(',');printf("{\"process\":%d,\"start\":%d,\"end\":%d,\"lane\":%d}",slices[s].p,slices[s].start,slices[s].end,slices[s].lane);}printf("],\"flights\":[");
 double wait=0,tat=0,response=0,fair_sum=0,fair_sq=0,emergency_response=0;int emergency_count=0;int maxwait=0,starved=0,responded=0,executed=0;for(int i=0;i<n;i++)executed+=p[i].burst-p[i].remaining;for(int i=0;i<n;i++){if(i)putchar(',');printf("{\"id\":");quote(p[i].id);printf(",\"airline\":");quote(p[i].airline);printf(",\"state\":");quote(states[p[i].state]);printf(",\"arrival\":%d,\"burst\":%d,\"priority\":%d,\"waiting\":%d,\"resource_waiting\":%d,\"response\":%d,\"turnaround\":%d,\"completion\":%d,\"bypassed\":%d}",p[i].arrival,p[i].burst,p[i].priority,p[i].waiting,blocked_wait[i],p[i].first<0?-1:p[i].first-p[i].arrival,p[i].completion<0?-1:p[i].completion-p[i].arrival,p[i].completion,p[i].bypassed);
 wait+=p[i].waiting;if(p[i].completion>=0){tat+=p[i].completion-p[i].arrival;double fraction=(double)p[i].burst/(p[i].completion-p[i].arrival);fair_sum+=fraction;fair_sq+=fraction*fraction;}if(p[i].first>=0){response+=p[i].first-p[i].arrival;responded++;if(p[i].emergency){emergency_response+=p[i].first-p[i].arrival;emergency_count++;}}if(p[i].waiting>maxwait)maxwait=p[i].waiting;if(p[i].waiting-blocked_wait[i]>=threshold)starved++;
 }
 printf("],\"metrics\":{\"average_waiting\":%.5f,\"average_turnaround\":%.5f,\"average_response\":%.5f,\"max_waiting\":%d,\"throughput\":%.5f,\"context_switches\":%d,\"service_utilization\":%.5f,\"starvation_warnings\":%d,\"completed\":%d,\"cancelled\":%d,\"average_queue_length\":%.5f,\"peak_queue\":%d,\"failed_resource_requests\":%d,\"duration\":%d,\"fairness_index\":%.5f,\"emergency_average_response\":%.5f,\"incomplete\":%s},\"resource_metrics\":[",wait/n,done?tat/done:0,responded?response/responded:0,maxwait,t?(double)done/t:0,switches,t?(double)executed/(t*(airport?(base[0]>0?base[0]:1):1)):0,starved,done,cancelled,t?(double)queue_area/t:0,peak_queue,failed_requests,t,done&&fair_sq?fair_sum*fair_sum/(done*fair_sq):0,emergency_count?emergency_response/emergency_count:0,timedout?"true":"false");
 for(int r=0;r<6;r++){int affected=0;for(int i=0;i<n;i++)if(resource_waiters[r][i])affected++;if(r)putchar(',');printf("{\"name\":");quote(names[r]);printf(",\"busy_slot_ticks\":%ld,\"utilization\":%.5f,\"idle_slot_ticks\":%ld,\"contention_ticks\":%ld,\"max_queue\":%d,\"average_resource_wait\":%.5f}",busy[r],t&&base[r]?(double)busy[r]/(t*base[r]):0,(long)t*base[r]-busy[r],contention[r],maxqueue[r],affected?(double)contention[r]/affected:0);}puts("]}");free(slices);
}
