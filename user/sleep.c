#include "kernel/types.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
  // 检查参数数量
  if (argc != 2) {
    write(2, "Usage: sleep <ticks>\n", 21);
    exit(1);
  }

  // 将字符串转换为整数
  int ticks = atoi(argv[1]);

  // 调用系统调用 sleep
  sleep(ticks);

  // 正常退出
  exit(0);
}
