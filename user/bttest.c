#include "kernel/types.h"
#include "user/user.h"

void
foo(int n)
{
  if (n > 0)
    foo(n - 1);
  else
    backtrace();
}

int
main(int argc, char *argv[])
{
  foo(3);
  exit(0);
}