#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[]) {
  // p1写，p0读
  int p1[2];  // ping管
  int p2[2];  // pong管
  pipe(p1);
  pipe(p2);
  char buffer[4];  // 字符栈，用于承载管道中读取的字符串
  // 子程序
  int pid = fork();  // 父子进程
  if (pid == 0) {
    close(p1[1]);            // 关闭ping管写口
    read(p1[0], buffer, 4);  // 读出字符
    close(p1[0]);
    printf("%d: received %s\n", getpid(), buffer);  // 按格式打印
    close(p2[0]);
    write(p2[1], "pong", 4);  // 写入pong
    close(p2[1]);
    exit(0);  // 退出
  }
  // 父程序
  else {
    close(p1[0]);
    write(p1[1], "ping", 10);  // 写入ping
    close(p1[1]);
    wait(0);  // 等待子进程
    close(p2[1]);
    read(p2[0], buffer, 10);
    close(p2[0]);
    printf("%d: received %s\n", getpid(), buffer);  // 按格式打印
    exit(0);                                        // 退出
  }
}