#include "utils.h"
#include <assert.h>
#include <stddef.h>
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
    .array=(void*)malloc(capacity*sizeof(void*))
  };
  assert(list.array!=0);
  return list;
}

void list_append(list_t *list, void *element){
  // first check if it's full
  if(list->len==list->capacity){
    //realloc
    list->array=realloc(list->array, list->capacity*sizeof(void*)*2);
    list->capacity*=2;
  }
  list->array[list->len++]=element;
}

void list_remove_shallow(list_t *list, size_t index){
  // check if the index overbounds
  assert(index<list->len);
  if(list->array[index]){
    list->array[index]=0;  
  }
  // and we compact the elements
  for (size_t i=index; i<list->len-1; i++) {
    list->array[i]=list->array[i+1];
  }
  if(list->len>0){
    list->len--;
  }
  if(list->len<list->capacity/2){
    list->array=realloc(list->array, list->capacity*sizeof(void*)/2);
    list->capacity/=2;
  }
}
void list_remove(list_t *list, size_t index){
  // check if the index overbounds
  assert(index<list->len);
  if(list->array[index]){
    free(list->array[index]);
    list->array[index]=0;  
  }
  // and we compact the elements
  for (size_t i=index; i<list->len-1; i++) {
    list->array[i]=list->array[i+1];
  }
  if(list->len>0){
    list->len--;
  }
  if(list->len<list->capacity/2){
    list->array=realloc(list->array, list->capacity*sizeof(void*)/2);
    list->capacity/=2;
  }
}
void* list_get(list_t* list, size_t index){
  assert(index<list->len);
  assert(list->array[index]!=0);
  return list->array[index];
} 
void free_list_shallow(list_t* list){  
  
  free(list->array);
  list->array=0;
  list->len=list->capacity=0;
}
void free_list(list_t* list){
  for (size_t i=0; i < list->len; ++i) {
    if (list->array[i]!=0) { 
      free(list->array[i]);
    }
  }
  free(list->array);
  list->array=0;
  list->len=list->capacity=0;
}
void free_list_dtor(list_t* list, void (*element_dtor)(void *element)){
  for (size_t i=0; i < list->len; ++i) {
    if (list->array[i]!=0) {
      element_dtor(list->array[i]);
      free(list->array[i]);
    }
  }
  free(list->array);
  list->array=0;
  list->len=list->capacity=0;
}

void list_copy(list_t* dest, list_t* src, copy_callback callback){
  dest->array=realloc(dest->array, src->capacity*sizeof(void*));
  dest->capacity=src->capacity;
  dest->len=src->len;
  dest->element_size=src->element_size;
  memset(dest->array,0 ,dest->capacity*sizeof(void*));
  for (size_t i=0; i < src->len; ++i) {
    dest->array[i]=malloc(dest->element_size);
    memcpy(dest->array[i],src->array[i],dest->element_size);
    callback(src->array[i],dest->array[i]);
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
  append(&alloced_mems, cloned);
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
