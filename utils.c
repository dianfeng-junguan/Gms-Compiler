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
  if(list->len>0)
    list->len--;
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

void free_list(list_t* list){
  for (size_t i=0; i < list->capacity; ++i) {
    if (list->array[i]!=0) { 
      free(list->array[i]);
    }
  }
  free(list->array);
}
void list_copy(list_t* dest, list_t* src){
  dest->array=realloc(dest->array, src->capacity*sizeof(void*));
  dest->capacity=src->capacity;
  dest->len=src->len;
  memset(dest->array,0 ,dest->capacity*sizeof(void*));
  for (size_t i=0; i < src->len; ++i) {
    dest->array[i]=src->array[i];
  }
}
void init_list(list_t* list, size_t capacity, size_t element_size){
  *list=create_list(capacity, element_size);
}
