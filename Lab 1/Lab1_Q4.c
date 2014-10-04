#include <stdio.h>
#include <tasklib.h>
#include <stdio.h>
#include <stdlib.h>
#include <errnoLib.h>
#include <timers.h>
#include <time.h>
#include <signal.h>	
#include <syslib.h>
#include <siglib.h>

struct sigevent release_hound1;
static struct sigaction hound1_action;

struct sigevent release_hound2;
static struct sigaction hound2_action;

static struct itimerspec itimeA= {{0,0},{0,0}};
static struct itimerspec itimeB= {{0,0},{0,0}};

int hound1_tag, hound1_count;
int hound2_tag, hound2_count;
timer_t t_timer1, t_timer2;

int iterA=0,iterB=0;

void cease_houndA(void);
void cease_houndB(void);


#define T1 20
#define T2 50
#define C1 10
#define C2 20
#define ITERATION 5 


void fib(unsigned int n)
	{
	   unsigned int  first = 0, second = 1, next, c=0;
/*n= 4863800;*/
	   for(c=0; c<n;c++)
	   {
	     if ( c <= 1 )
	         next = c;
	      else
	      {
	         next = first + second;
	         first = second;
	         second = next;
	      }
	   }
	   
	}


void hound1_handler(int signo1, siginfo_t *info1, void *ignored1)
{
	int prio1 = taskIdSelf(); 
		taskPrioritySet(prio1, 1);
	if(hound1_count > C1)
	{
			if(iterA >ITERATION)
			{
			cease_houndA();
			}iterA++;
			
	}
	hound1_count++;
}

void hound2_handler(int signo2, siginfo_t *info2, void *ignored2)
{
	int prio2 = taskIdSelf(); 
	taskPrioritySet(prio2, 1);
	
	if(hound2_count > C2)
	{
				if(iterB >ITERATION)
							{
							cease_houndB();
							}iterB++;
				
	}
			hound2_count++;		
}


void hound_1(void)
{
	
	hound1_tag = taskIdSelf();
/*arm the timer on hound_1*/
	sigaction(SIGRTMIN+1, &hound1_action, NULL);	
	timer_create(CLOCK_REALTIME, &release_hound1, &t_timer1);     
	timer_settime(t_timer1, TIMER_ABSTIME, &itimeA, NULL);
	while(1)
	{
		pause();
/*Call fib here with count for 10ms*/
		fib(2431900);  
	}
	
}

void hound_2(void)
{
/*arm the timer on hound_2*/
	
	hound2_tag = taskIdSelf();
	sigaction(SIGRTMIN+2, &hound2_action, NULL);
	timer_create(CLOCK_REALTIME, &release_hound2, &t_timer2);     
	timer_settime(t_timer2, TIMER_ABSTIME, &itimeB, NULL);
			
		
		while(1)
		{
			pause();
	/*Call fib here*/
			fib(4863800);
		}
	
}

void release_the_hounds(void)
{
	int my_tid = taskIdSelf();
	sysClkRateSet(1000);
/*Spawn two hellhounds*/
	taskSpawn("hound_1",25,0,4000,(FUNCPTR)hound_1,0,0,0,0,0,0,0,0,0,0);
	taskSpawn("hound_2",50,0,4000,(FUNCPTR)hound_2,0,0,0,0,0,0,0,0,0,0);
/*setup the timer on hound_1*/	
	
/*event for hound1*/	
	release_hound1.sigev_notify = SIGEV_SIGNAL;
	release_hound1.sigev_signo = SIGRTMIN+1;
	release_hound1.sigev_value.sival_int = my_tid;

/*action for hound1*/	
	hound1_action.sa_sigaction = hound1_handler;
	hound1_action.sa_flags = SA_SIGINFO;
	   

/*Setting up interval for houndA*/	
	itimeA.it_interval.tv_sec = 0;
	itimeA.it_interval.tv_nsec = T1;
	itimeA.it_value.tv_sec = 0;
	itimeA.it_value.tv_nsec = T1;
	
/*setup the timer on hound_2*/
	
/*event for hound2*/	
	release_hound2.sigev_notify = SIGEV_SIGNAL;
	release_hound2.sigev_signo = SIGRTMIN+2;
	release_hound2.sigev_value.sival_int = my_tid;

/*action for hound2*/	
    hound2_action.sa_sigaction = hound2_handler;
    hound2_action.sa_flags = SA_SIGINFO;
	   
		
/*interval for hound2*/	
	itimeB.it_interval.tv_sec = 0;
	itimeB.it_interval.tv_nsec = T2;
	itimeB.it_value.tv_sec = 0;
	itimeB.it_value.tv_nsec = T2;
}


void cease_houndA(void)
{
	int status;
	if((status = timer_cancel(t_timer1)) == ERROR)
		perror("t_timer1");
	
	if((status = timer_delete(t_timer1)) == ERROR)
		perror("t_timer1");
	exit(0);
}

void cease_houndB(void)
{
	int status;
	if((status = timer_cancel(t_timer2)) == ERROR)
		perror("t_timer2");
	
	if((status = timer_delete(t_timer2)) == ERROR)
		perror("t_timer2");
	exit(0);
}
