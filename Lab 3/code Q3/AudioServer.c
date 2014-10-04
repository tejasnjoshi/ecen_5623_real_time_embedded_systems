/* AudioServer.c - TCP server example */

#include "sys/types.h"
#include "stdio.h"
#include "signal.h"
#include "vxWorks.h" 
#include "taskLib.h"
#include "tcpExample.h"



/*VOID SendTask(int sFd);*/

/*STATUS tcpServer (int SERVER_PORT_NM)
{
  if (taskSpawn("tcpRecvTask", SERVER_WORK_PRIORITY, 0, SERVER_STACK_SIZE,(FUNCPTR) tcpRecvTask, 0, 0, 0,0,0, 0, 0, 0, 0, 0) == ERROR)
    {
      perror ("taskSpawn");
    }
}

STATUS tcpClient (int SERVER_PORT_NM)
{
  if (taskSpawn("tcpSendTask", CLIENT_WORK_PRIORITY, 0, SERVER_STACK_SIZE,(FUNCPTR) tcpSendTask, 0, 0, 0,0,0, 0, 0, 0, 0, 0) == ERROR)
    {
      perror ("taskSpawn");
    }
}*/



/*if (taskSpawn("SendTask", 4, 0, SERVER_STACK_SIZE,(FUNCPTR) SendTask, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0) == ERROR)
   {
     perror ("taskSpawn");
   }*/

