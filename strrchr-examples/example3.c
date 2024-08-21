#include <stdio.h>
#include <string.h>

int main() {
  const char str[] = "Tutorialspoint";
  const char ch = 'n';

  char* ptr = strrchr(str, ch);

  if (ptr) {
    printf("Last occurrence of '%c' in \"%s\" is at index %ld\n", ch, str, ptr - str);
  } else {
    printf("'%c' is not present in \"%s\"\n", ch, str);
  }

  return 0;
}
