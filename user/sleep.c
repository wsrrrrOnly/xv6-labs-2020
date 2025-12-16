
#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  int i;
  if(argc < 2){
    printf("usage: sleep [number of ticks]\n");
    exit(1);
  }
  i = atoi(argv[1]);
  sleep(i);
  exit(0);
}