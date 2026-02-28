#ifndef _STATUS_H
#define _STATUS_H
#include "utils.h"
/* used to store some global data */
typedef struct {
  list_t symbol_tables;
  
}compiler_global_data_t;
void init_compiler_global_data(compiler_global_data_t *globals);
#endif
