#include <time.h>
#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <tasklib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errnoLib.h>
#include <timers.h>
#include <time.h>
#include <signal.h>
#include <syslib.h>

void terminate_a(void);
void terminate_b(void);
void ptimer_testA(void);
void ptimer_testB(void);

#define NSEC_PER_MICROSEC 1000
#define NSEC_PER_MSEC 1000000
#define NSEC_PER_SEC 1000000000
#define MICROSEC_PER_MSEC 1000

/* #define MY_TICK_NSECS 16666666 */
/* #define MY_TICK_NSECS 99999996 */
#define MY_TICK_NSECS 1000000

#define TICKS_PER_SEC (NSEC_PER_SEC/MY_TICK_NSECS)

SEM_ID synch_sem;                                                                   
int abort_test = FALSE;
int take_cnt = 0;
int give_cnt = 0;
int ticks_per_sec, tick_nsecs;

void ptimer_shutdown(void);
void ptimer_test(void);

static timer_t tt_timerA;
static timer_t tt_timerB;
/*static struct itimerspec last_itimeA;
static struct itimerspec last_itimeB;*/
static struct itimerspec itimeA = {{0,MY_TICK_NSECS}, {0,MY_TICK_NSECS}};
static struct itimerspec itimeB = {{0,MY_TICK_NSECS}, {0,MY_TICK_NSECS}};

static unsigned long tick_countA = 0;
static unsigned long tick_countB = 0;
/*static struct timespec rtclk_timeA = {0, 0};
static struct timespec rtclk_timeB = {0, 0};*/
static struct timespec rtclk_start_timeA = {0, 0};
static struct timespec rtclk_start_timeB = {0, 0};
/*static struct timespec rtclk_last_timeA = {0, 0};
static struct timespec rtclk_last_time = {0, 0};
*/
void task_a(void)
{
 

  while(!abort_test)
  {
      
      ptimer_testA();
 
  }
}

void task_b(void)
{
 
 
  while(!abort_test)
  {
      
      ptimer_testB();
    
  }
}


void monitor_interval_expiredA(int signo, siginfo_t *info, void *ignored)
{
    tick_countA++;
    printf("hi :)\n");
    if(tick_countA == 9)
    {
        terminate_a();
    }
}

void monitor_interval_expiredB(int signo, siginfo_t *info, void *ignored)
{
    tick_countB++;
    printf("bye :P\n");
    if(tick_countB == 9)
    {
        terminate_b();
    }
}


void ptimer_testB(void)
{
    int my_tid;
    struct sigevent ReleaseEventB;
    struct sigaction ReleaseActionB;
    
     ReleaseEventB.sigev_notify = SIGEV_SIGNAL;
     ReleaseEventB.sigev_signo = SIGRTMIN+2;
     ReleaseEventB.sigev_value.sival_int = my_tid;

     ReleaseActionB.sa_sigaction = monitor_interval_expiredB;
     ReleaseActionB.sa_flags = SA_SIGINFO;
     sigaction(SIGRTMIN+2, &ReleaseActionB, NULL);      
    
     itimeB.it_interval.tv_sec = 0;
       itimeB.it_interval.tv_nsec = MY_TICK_NSECS;
       itimeB.it_value.tv_sec = 0;
       itimeB.it_value.tv_nsec = MY_TICK_NSECS;
       
       timer_create(CLOCK_REALTIME, &ReleaseEventB, &tt_timerB);
       timer_settime(tt_timerB, 0, &itimeB, NULL);
       clock_gettime(CLOCK_REALTIME, &rtclk_start_timeB);
       
        
       while(1)
             {
              pause();
             }
      
}

void ptimer_testA(void)
{

  int my_tid;
 
  struct sigevent ReleaseEventA;
  struct sigaction ReleaseActionA;
 
  my_tid = taskIdSelf();

  ReleaseEventA.sigev_notify = SIGEV_SIGNAL;
  ReleaseEventA.sigev_signo = SIGRTMIN+1;
  ReleaseEventA.sigev_value.sival_int = my_tid;

  ReleaseActionA.sa_sigaction = monitor_interval_expiredA;
  ReleaseActionA.sa_flags = SA_SIGINFO;
  sigaction(SIGRTMIN+1, &ReleaseActionA, NULL);   
 
  itimeA.it_interval.tv_sec = 0;
  itimeA.it_interval.tv_nsec = MY_TICK_NSECS;
  itimeA.it_value.tv_sec = 0;
  itimeA.it_value.tv_nsec = MY_TICK_NSECS;
   
 
 
 
 
  /* initialize internal timer release wrapper */
  timer_create(CLOCK_REALTIME, &ReleaseEventA, &tt_timerA);     
   timer_settime(tt_timerA, 0, &itimeA, NULL);
  clock_gettime(CLOCK_REALTIME, &rtclk_start_timeA);
   /* keep tick-tock task resident */
      while(1)
      {
       pause();
      }

 
}

void terminate_a(void)
{
  int status;

  /* disable and delete timer */

  if((status = timer_cancel(tt_timerA)) == ERROR)
    perror("tt_timerA");

  if((status = timer_delete(tt_timerA)) == ERROR)
    perror("tt_timerA");
    fflush(stdout);
  exit(0);
}

void terminate_b(void)
{
  int status;

  /* disable and delete timer */

  if((status = timer_cancel(tt_timerB)) == ERROR)
    perror("tt_timerB");

  if((status = timer_delete(tt_timerB)) == ERROR)
    perror("tt_timerB");
    fflush(stdout);
                                   

  exit(0);
}

void release_the_hounds(void)
{
  int taskId;
 
  struct timespec rtclk_resolution;
 
  sysClkRateSet(100);



  if((taskId = taskNameToId("task_a")) != ERROR)
  {
    if(taskDelete(taskId) != ERROR)
        printf("Old task a deleted\n");
  }
  else
  {
    printf("Old task a not found\n");
  }
 

  if((taskId = taskNameToId("task_b")) != ERROR)
  {
    if(taskDelete(taskId) != ERROR)
        printf("Old task b deleted\n");
  }
  else
  {
      printf("Old task b not found\n");
  }
 
 
 

  if(taskSpawn("task_a", 90, 0, 4000, (FUNCPTR)task_a, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
  {
    printf("Task A task spawn failed\n");
  }
  else
    printf("Task A task spawned\n");

  if(taskSpawn("task_b", 100, 0, 4000, (FUNCPTR)task_b, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
  {
    printf("Task B task spawn failed\n");
  }
  else
    printf("Task B task spawned\n");

  if(clock_getres(CLOCK_REALTIME, &rtclk_resolution) == ERROR)
      perror("clock_getres");

    ticks_per_sec = sysClkRateGet();
    tick_nsecs = (NSEC_PER_SEC/ticks_per_sec);
    if(clock_getres(CLOCK_REALTIME, &rtclk_resolution) == ERROR)
        perror("clock_getres");

      ticks_per_sec = sysClkRateGet();
      tick_nsecs = (NSEC_PER_SEC/ticks_per_sec);
   
}
