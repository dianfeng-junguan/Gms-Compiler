#ifndef _UTILS_H
#define _UTILS_H
#include <stddef.h>
typedef struct _filepos_t {
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
/**
   initialzie the list.
   this will ignore the previous data of the initialized list, so store it elsewhere or release it before
   calling this.
   do not use this after create_list. create_list and init_list do the same work.
 **/
void init_list(list_t* list, size_t capacity, size_t element_size);

void list_append(list_t *list, void *element);
void list_remove(list_t *list, size_t index);
void *list_get(list_t *list, size_t index);

/**
   copy a list into another list.
   this is a shallow copy. it copies the pointers but does not copy the data pointed to.
   this will directly overwrite the dest list, so store the value before doing this.
 **/
void list_copy(list_t* dest, list_t* src);

void free_list(list_t* list);

#define append(list, elementptr) (list_append(list, (void*)elementptr))

#define VERBOSE 1
#define REGULAR 3

#define NEED_LOG

#define LOG_LEVEL VERBOSE
#ifdef NEED_LOG
#define LOG(level, fmt, ...) if(level>=LOG_LEVEL)printf(fmt,##__VA_ARGS__);
#define LOGERR(level, sender, pos, fmt, ...) if(level>=LOG_LEVEL)cry_errorf(sender, pos, fmt,##__VA_ARGS__);
#else
#define LOG(level, fmt, ...)
#define LOGERR(level, sender, pos, fmt, ...)
#endif 
#endif
