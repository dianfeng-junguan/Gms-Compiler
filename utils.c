#include "utils.h"
#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
list_t create_list(size_t capacity, size_t element_size){
  if (capacity<=0) {
    capacity=1;
  }
  list_t list={
    .element_size=element_size,
    .capacity=capacity,
    .len=0,
    .array=(void*)malloc(capacity*element_size)
  };
  assert(list.array!=0);
  return list;
}

void list_append(list_t *list, void *element){
  // first check if it's full
  if(list->len==list->capacity){
    //realloc
    list->array=realloc(list->array, list->capacity*list->element_size*2);
    list->capacity*=2;
  }
  memcpy(list->array+list->len*list->element_size, element, list->element_size);
  list->len++;
}
void list_swap(list_t *list, size_t pos1, size_t pos2) {
  assert(pos1 < list->len);
  assert(pos2 < list->len);
  for (size_t i=0; i < list->element_size; ++i) {
    char *element1 = list->array + pos1 * list->element_size;
    char *element2 = list->array + pos2 * list->element_size;
    char tmp = element1[i];
    element1[i] = element2[i];
    element2[i]=tmp;    
  }
  
}
void list_insert(list_t *list, size_t position, void *element) {
  assert(list->len > position);
  // must panic when assert fails bc it means mistakes in code
  // check if it is capable of containing the grown len
  if (list->capacity<=list->len+1) {
    // grow the capacity
    list->array=realloc(list->array, list->capacity*list->element_size*2);
    list->capacity*=2;
  }
  memmove(list->array + list->element_size * (1 + position),
          list->array + list->element_size * position,
          list->element_size * (list->len - position));
  memcpy(list->array + list->element_size * position, element,
         list->element_size);
  list->len++;
}
void list_remove(list_t *list, size_t index){
  // check if the index overbounds
  assert(index<list->len);
  // compact the elements
  memmove(list->array + list->element_size * index,
         list->array + list->element_size * (index + 1),
         (list->len-index-1)*list->element_size);
  if(list->len<list->capacity/2){
    list->array=realloc(list->array, list->capacity*list->element_size/2);
    list->capacity/=2;
  }
}
void* list_get(list_t* list, size_t index){
  assert(index < list->len);
  return list->array+list->element_size*index;
} 
void free_list(list_t* list){
  free(list->array);
  list->array=0;
  list->len=list->capacity=0;
}
void free_list_dtor(list_t* list, void (*element_dtor)(void *element)){
  for (size_t i=0; i < list->len; ++i) {
    element_dtor(list->array+i*list->element_size);
  }
  free(list->array);
  list->array=0;
  list->len=list->capacity=0;
}

void list_copy(list_t* dest, list_t* src, copy_callback callback){
  dest->capacity=src->capacity;
  dest->len=src->len;
  dest->element_size=src->element_size;
  dest->array=realloc(dest->array, src->capacity*dest->element_size);
  memcpy(dest->array, src->array, src->capacity*src->element_size);
  for (size_t i = 0; i < src->len; ++i) {
    size_t offset=i*src->element_size;
    callback(src->array+offset,dest->array+offset);
  }
}
void init_list(list_t* list, size_t capacity, size_t element_size){
  *list=create_list(capacity, element_size);
}

list_t alloced_mems;
void init_my_allocator() { init_list(&alloced_mems, 100, sizeof(char *)); }

char* myalloc(size_t len){
  char* cloned=malloc(len+1);
  assert(cloned);
  memset(cloned,0,len+1);
  append(&alloced_mems, &cloned);
  return cloned;
}

char* clone_str(char* str){
  size_t len=strlen(str);
  char* cloned=myalloc(len+1);
  strcpy(cloned, str);
  return cloned;
}

void myfree(char* str){
  for (size_t i=0; i < alloced_mems.len; ++i) {
    if(list_get(&alloced_mems, i)==str){
      list_remove(&alloced_mems, i);
    }
  }
}
void free_rest(){
  free_list(&alloced_mems);
}
void list_concat(list_t* dest,list_t* src){
  for (size_t i=0; i<src->len; i++) {
    append(dest, list_get(src, i));
  }
}


cstring_t create_string(){
  cstring_t cstr = {0};
  cstr.len = 0;
  cstr.data = NULL;
  return cstr;
}

cstring_t string_from(const char *conststr){
  cstring_t cstr = create_string();
  cstr.len = strlen(conststr);
  cstr.data = malloc(cstr.len + 1);
  memset(cstr.data, 0, cstr.len + 1);  
  assert(cstr.data);
  strcpy(cstr.data, conststr);
  return cstr;
}
cstring_t string_clone(cstring_t *cstr) {
  cstring_t new_cstr = string_from(cstr->data);
  return new_cstr;
}

void string_push(cstring_t *cstr, char *to_push) {
  cstr->data = realloc(cstr->data, cstr->len + strlen(to_push) + 1);
  assert(cstr->data);
  strcpy(cstr->data+cstr->len, to_push);
  cstr->len += strlen(to_push);
}

cstring_t string_substr(cstring_t* str, size_t start, size_t end){
  assert(start <= end);
  assert(str->len > start);  
  assert(str->len > end);
  cstring_t cstr = create_string();
  cstr.data = malloc(end-start+1);
  cstr.len = end - start;
  memset(cstr.data, 0, end - start + 1);
  memcpy(cstr.data, str->data + start, end - start);
  return cstr;
}

void free_string(cstring_t* cstr){
  assert(cstr->data);
  free(cstr->data);
  cstr->data = NULL;
  cstr->len = 0;
}

char string_nth(cstring_t* cstr, size_t n){
  assert(cstr->len > n);
  return cstr->data[n];
}
static char string_fmtbuf[2048]={0};
void string_sprintf(cstring_t* cstr, char* fmt, ...){
  va_list args;
  va_start(args,fmt);
  vsnprintf(string_fmtbuf, sizeof(string_fmtbuf), fmt, args);
  size_t slen = strnlen(string_fmtbuf, sizeof(string_fmtbuf));
  if (cstr->len<slen) {
    cstr->data = cstr->data?realloc(cstr->data, slen + 1):malloc(slen+1);
    cstr->len = slen;
  }
  strcpy(cstr->data, string_fmtbuf);
  va_end(args);
}
static int log_level = VERBOSE;

static int log_parts_switch[] = {
    [INTERCODE_ALLOCSYM] = 1,
    [SEMATIC_CHECK] = 1,
    [PARSER_OUTPUT] = 0,
    [LEXER_OUTPUT] = 0,
    
};
void do_log(int level, int part, const char* fmt, ...){
  if (level>=log_level&&log_parts_switch[part]) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
}
