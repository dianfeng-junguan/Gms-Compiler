#ifndef _UTILS_H
#define _UTILS_H
#include <stddef.h>
typedef unsigned long long u64;
typedef long long i64;
typedef int symbol_type_index_t;
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
  char* array;
  
}list_t;

// this is mainly used to make string operations more convenient.
// moreover, this can be used to deal with wide char.
typedef struct{
  char *data;
  size_t len;
} CString;

// create an empty CString
CString create_string();
// create a CString from a raw char*. it does not consume the char*
CString string_from(const char *conststr);
// create a CString by cloning a existing one.
CString string_clone(CString* cstr);
/// push a raw string into CString.
void string_push(CString *cstr, char *to_push);
// copy a slice of CString.
CString string_substr(CString *str, size_t start, size_t end);
// take the nth char of the cstr.
char string_nth(CString* cstr, size_t n); 
// free the CString
void free_string(CString* cstr);

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
void list_remove_shallow(list_t *list, size_t index);
void *list_get(list_t *list, size_t index);

typedef void (*copy_callback)(void* old, void* newt);
/**
   copy a list into another list.
   use the callback to do specific deep copying
   this will directly overwrite the dest list, so store the value before doing this.
 **/
void list_copy(list_t* dest, list_t* src, copy_callback callback);
void list_concat(list_t* dest,list_t* src);
void free_list(list_t* list);
void free_list_shallow(list_t *list);
typedef void (*list_ele_dtor)(void *element);
/**
 * @brief      free the list and the elements with destructor
 *
 * @details    call dtor on every element
 *
 * @param      list the list
 * @param      element_dtor the destructor
 *
 * @return     void
 */
void free_list_dtor(list_t *list, list_ele_dtor element_dtor);
#define FREE_LIST_DTOR(list, dtor) free_list_dtor((list), (list_ele_dtor)(dtor))

#define FREEIF(cont)                                                           \
  if (cont) {                                                                  \
    free(cont);                                                                \
  }
#define FREEIFD(cont, dtor)			\
  if(cont)dtor((char*)cont);

// ==================================
// heap alloc funcs
void init_my_allocator();
char* clone_str(char* str);
char *myalloc(size_t len);
void myfree(char *str);
void free_rest();
// ==================================


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


