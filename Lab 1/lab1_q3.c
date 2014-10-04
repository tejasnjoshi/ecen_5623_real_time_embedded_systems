/*Now, determine how many terms in the sequence corresponds to 10 milliseconds of computation on a lab target
PC (note that the answer may vary depending upon the specific target used). 
The easiset way to do this is touse System Viewer to measure the CPU time taken by a task calling your 
function with a large value - see how long that takes and then scale up or down the number as needed to
achiever 10 milliseconds of computation. Repeat this to determine how many terms are required for 20
milliseconds of computation using System Viewer. 
[This has to be done on a lab machine, simulator will not be acceptable]*/ 

#include<stdio.h>
#include<tasklib.h>

void fib(void)
{
   unsigned int n, first = 0, second = 1, next, c=0;
   n= 4863800;
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

void fib_test(void)
{
		taskSpawn("fib",25,0,300,(FUNCPTR)fib,0,0,0,0,0,0,0,0,0,0);
}
