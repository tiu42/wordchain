#include <stdlib.h>
#include <stdio.h>

int main() {
  int ret = system("./seed");

  if (ret == 0) {
    printf("Seed data successful.\n");
  } else {
    printf("Error occurred while seeding data.\n");
  }

  return 0;
}
