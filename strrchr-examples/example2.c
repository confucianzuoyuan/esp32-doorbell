#include <stdio.h>
#include <string.h>

int main() {
  const char str[] = "Hello, World!";
  const char ch = 'W';

  // 在子字符串"World!"中查询
  char* ptr = strrchr(str + 7, ch);

  if (ptr) {
    printf("'%c' found at position %ld\n", ch, ptr - str);
  } else {
    printf("'%c' not found in the substring\n", ch);
  }

  return 0;
}
