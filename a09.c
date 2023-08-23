
/* GPL3+ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "a09.h"

int main(int argc,char *argv[])
{
  struct a09           a09;
  struct opcode const *op;
  
  (void)argc;
  (void)argv;
  
  a09.in        = stdin;
  a09.out       = fopen("a09.out","wb");
  a09.list      = NULL;
  a09.symtab    = NULL;
  a09.namespace = strdup("");
  
  op = op_find("NOP");
  (*op->func)(op,&a09);
  
  free(a09.namespace);
  fclose(a09.out);
  return 0;
}
