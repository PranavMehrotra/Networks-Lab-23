#include <stdio.h>
#include <time.h>

int main ()
{
  time_t rawtime;
  struct tm * timeinfo;

  time ( &rawtime );
  timeinfo = localtime ( &rawtime );
  printf("%d\n\n",rawtime);
  printf("%d %d %d %s %d",timeinfo->tm_sec,timeinfo->tm_year,timeinfo->tm_isdst,timeinfo->tm_zone,timeinfo->tm_mday);
  printf ( "\n\nCurrent local time and date: %s", asctime (timeinfo) );
  
  return 0;
}