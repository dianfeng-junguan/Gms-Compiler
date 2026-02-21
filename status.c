#include "status.h"
#include "utils.h"
#include "parser.h"
void init_compiler_global_data(compiler_global_data_t *globals){
  globals->symbol_tables=create_list(10, sizeof(symbol_table_t));
}
