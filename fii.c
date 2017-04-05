#include <c64.h>
#include <cbm.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main (void) {
  char* filename = "0:index.htm,s,r";
  char* file_data = (unsigned char*) malloc(2048);
  int size = -1;
  int res = cbm_open(6, 8, CBM_READ, filename);
  printf("res=%d\n", res);
  size = cbm_read(6, file_data, 2048);
  printf("size=%d\n", size);
  printf("data=");
  printf(file_data);
  printf("\n_oserror=%d\n", _oserror);  
  cbm_close(6);  
  free(file_data);
  return 0;
}
