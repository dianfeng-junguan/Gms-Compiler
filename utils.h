#ifndef _UTILS_H
#define _UTILS_H
#include <stddef.h>
typedef struct {
  size_t line;
  size_t column;
}filepos_t;
/*
  the list is an array of pointers to the real data.
  when created it has a capacity. if it's full. it reallocates
  a bigger mem block.

 */
typedef struct 
{
  size_t len;
  size_t capacity;
  size_t element_size;
  void** array;
}list_t;

list_t create_list(size_t capacity, size_t element_size);

void list_append(list_t *list, void *element);
void list_remove(list_t *list, size_t index);
void* list_get(list_t* list, size_t index);

void free_list(list_t* list);

#define append(list, elementptr) (list_append(list, (void*)elementptr))


#endif
