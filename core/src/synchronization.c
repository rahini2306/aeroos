#define _POSIX_C_SOURCE 200809L
#include "aero.h"
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdatomic.h>
typedef struct {int thread,value,active;char action[32];} Log;
static Log logs[20000];static int nl,counter,active,peak,limit,loops,threads,produced,consumed,queue[16],head,tail,readers,writes;
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER,log_mutex=PTHREAD_MUTEX_INITIALIZER,forks[16],bar_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t bar_cond=PTHREAD_COND_INITIALIZER;static int arrived,phase;
static pthread_rwlock_t rw=PTHREAD_RWLOCK_INITIALIZER;
static sem_t *slots,*items,*permit;static char mode[24];static atomic_int atomic_counter;
static void record(int id,const char *action,int value){pthread_mutex_lock(&log_mutex);if(nl<20000){logs[nl].thread=id;logs[nl].value=value;logs[nl].active=active;snprintf(logs[nl++].action,32,"%s",action);}pthread_mutex_unlock(&log_mutex);}
static void barrier(void){pthread_mutex_lock(&bar_mutex);int generation=phase;if(++arrived==threads){arrived=0;phase++;pthread_cond_broadcast(&bar_cond);}else while(generation==phase)pthread_cond_wait(&bar_cond,&bar_mutex);pthread_mutex_unlock(&bar_mutex);}
static sem_t *named_sem(int index,int initial){char name[80];snprintf(name,sizeof(name),"/aeroos-%ld-%d",(long)getpid(),index);sem_t *s=sem_open(name,O_CREAT|O_EXCL,0600,(unsigned)initial);if(s==SEM_FAILED)fail("Cannot create POSIX semaphore");sem_unlink(name);return s;}
static void *worker(void *arg){int id=*(int*)arg;
 for(int k=0;k<loops;k++){
  if(!strcmp(mode,"race")){int local=atomic_load(&atomic_counter);barrier();atomic_store(&atomic_counter,local+1);barrier();record(id,"atomic_lost_update",local+1);}
  else if(!strcmp(mode,"producer_consumer")){
   if(id%2==0){record(id,"waiting_slot",k);sem_wait(slots);pthread_mutex_lock(&mutex);int value=id*loops+k;queue[tail++%16]=value;produced++;record(id,"produced",value);pthread_mutex_unlock(&mutex);sem_post(items);}
   else{record(id,"waiting_item",k);sem_wait(items);pthread_mutex_lock(&mutex);int value=queue[head++%16];consumed++;record(id,"consumed",value);pthread_mutex_unlock(&mutex);sem_post(slots);}
  }
  else if(!strcmp(mode,"readers_writers")){
   if(id%3){pthread_rwlock_rdlock(&rw);pthread_mutex_lock(&log_mutex);readers++;pthread_mutex_unlock(&log_mutex);record(id,"read_enter",counter);record(id,"read_exit",counter);pthread_mutex_lock(&log_mutex);readers--;pthread_mutex_unlock(&log_mutex);pthread_rwlock_unlock(&rw);}
   else{pthread_rwlock_wrlock(&rw);counter++;writes++;record(id,"write_enter",counter);record(id,"write_exit",counter);pthread_rwlock_unlock(&rw);}
  }
  else if(!strcmp(mode,"dining")){int left=id,right=(id+1)%threads,first=left<right?left:right,second=left<right?right:left;record(id,"waiting_forks",first);pthread_mutex_lock(&forks[first]);pthread_mutex_lock(&forks[second]);pthread_mutex_lock(&mutex);counter++;record(id,"service_pair_acquired",counter);pthread_mutex_unlock(&mutex);pthread_mutex_unlock(&forks[second]);pthread_mutex_unlock(&forks[first]);record(id,"service_pair_released",k);}
  else{record(id,"waiting_lock",k);if(!strcmp(mode,"mutex"))pthread_mutex_lock(&mutex);else sem_wait(permit);pthread_mutex_lock(&log_mutex);active++;if(active>peak)peak=active;int observed=counter;pthread_mutex_unlock(&log_mutex);record(id,"critical_enter",observed);pthread_mutex_lock(&log_mutex);counter++;observed=counter;pthread_mutex_unlock(&log_mutex);record(id,"critical_exit",observed);pthread_mutex_lock(&log_mutex);active--;pthread_mutex_unlock(&log_mutex);if(!strcmp(mode,"mutex"))pthread_mutex_unlock(&mutex);else sem_post(permit);}
 }
 record(id,"terminated",loops);return NULL;
}
void sync_lab(int root){string(field(root,"mode"),mode,sizeof(mode),"mutex");const char *modes[]={"mutex","binary","counting","race","producer_consumer","readers_writers","dining"};bool valid=false;for(int i=0;i<7;i++)if(!strcmp(mode,modes[i]))valid=true;if(!valid)fail("Unknown synchronization mode");
 threads=get(root,"threads",4);loops=get(root,"iterations",20);limit=!strcmp(mode,"counting")?get(root,"capacity",2):1;if(threads<2||threads>16||loops<1||loops>100||limit<1||limit>threads)fail("Invalid thread configuration");if(!strcmp(mode,"producer_consumer")&&threads%2)fail("Producer-consumer requires an even thread count");
 permit=named_sem(0,limit);slots=named_sem(1,16);items=named_sem(2,0);pthread_t tid[16];int ids[16];for(int i=0;i<threads;i++)pthread_mutex_init(&forks[i],NULL);
 for(int i=0;i<threads;i++){ids[i]=i;if(pthread_create(tid+i,NULL,worker,ids+i))fail("pthread_create failed");}for(int i=0;i<threads;i++)pthread_join(tid[i],NULL);
 sem_close(permit);sem_close(slots);sem_close(items);for(int i=0;i<threads;i++)pthread_mutex_destroy(&forks[i]);if(!strcmp(mode,"race"))counter=atomic_load(&atomic_counter);
 printf("{\"mode\":");quote(mode);printf(",\"threads\":%d,\"joined\":%d,\"counter\":%d,\"expected_updates\":%d,\"peak_concurrent\":%d,\"capacity\":%d,\"produced\":%d,\"consumed\":%d,\"writes\":%d,\"events\":[",threads,threads,counter,threads*loops,peak,limit,produced,consumed,writes);
 for(int k=0;k<nl;k++){if(k)putchar(',');printf("{\"id\":%d,\"thread\":%d,\"action\":",k,logs[k].thread);quote(logs[k].action);printf(",\"value\":%d,\"active\":%d}",logs[k].value,logs[k].active);}puts("]}");
}
