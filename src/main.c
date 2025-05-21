#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "../include/commands.h"
#include "../include/utilitaires.h"
#include "../include/sha1.h"

int main(int argc, char *argv[])
{

  if (argc < 2)
  {
    print_usage();
    return 1;
  }

  commands(argc, argv);

  return 0;
}