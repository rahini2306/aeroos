#ifndef AERO_H
#define AERO_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#define MAX_P 100
#define MAX_R 6
#define MAX_T 10000
typedef struct { int start,end,size; } Token;
extern char input[262144];
extern Token tok[32768];
int parse_json(void);
int field(int object,const char *key);
int item(int array,int index);
int number(int token,int fallback);
int get(int object,const char *key,int fallback);
bool flag(int object,const char *key,bool fallback);
void string(int token,char *out,size_t size,const char *fallback);
void quote(const char *s);
void fail(const char *message);
typedef struct {
 char id[40],airline[64],category[24];
 int arrival,departure,burst,remaining,priority,effective,emergency,memory,req[6];
 int state,first,completion,waiting,bypassed,level,used,allocated[6];
} Flight;
typedef struct {int owner[6][100],cap[6],base[6];} ResourcePool;
extern ResourcePool resource_pool;
bool resource_compatible(Flight *p,int resource,int slot,int config);
bool resource_fits(Flight *p,int config,int memory,int used);
void resource_acquire(Flight *p,int process,int config);
void resource_release(Flight *p,int process);
enum { NEW, READY, RUNNING, WAITING, BLOCKED, COMPLETED, CANCELLED };
extern const char *states[];
int read_flights(int root,Flight *p);
void scheduler(int root,bool airport);
void banker(int root,bool detection);
void memory_lab(int root);
void sync_lab(int root);
void model_check(int root);
#endif
