#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char *path) {
  static char buf[DIRSIZ + 1];  // dirsiz 是fs.h中定义的常量 14，表示文件名的最大字符
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;  // p指针落在最后一个/后第一个字母

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ) return p;
  memmove(buf, p, strlen(p));                         // 文件名赋值给buf
  memset(buf + strlen(p), '\0', DIRSIZ - strlen(p));  // 清空后面的值，将空改成\0以结束字符串
  return buf;                                         // 文件名指针
}

void find(char *path, char *aim) {
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch (st.type) {
    case T_FILE:
      if (!strcmp(fmtname(path), aim)) printf("%s\n", path);  // 相等
      break;

    case T_DIR:
      if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {  // 啥东西没看懂
        printf("find: path too long\n");
        break;
      }
      strcpy(buf, path);
      p = buf + strlen(buf);
      *p++ = '/';
      while (read(fd, &de, sizeof(de)) == sizeof(de)) {
        if (de.inum == 0 || !strcmp(de.name, ".") || !strcmp(de.name, "..")) continue;  // 防循环
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = '\0';
        find(buf, aim);
      }
      break;
  }
  close(fd);
}

int main(int argc, char *argv[]) {
  if (argc < 3) {  // 不符合find path name的格式
    exit(0);
  }
  find(argv[1], argv[2]);
  exit(0);
}
