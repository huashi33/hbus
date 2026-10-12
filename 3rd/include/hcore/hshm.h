#ifndef _HSHM_H_
#define _HSHM_H_

#include "hdef.h"
// #include "hds.h"
/*

#define HSHM_INIT_TYPE_ATTACH 0
#define HSHM_INIT_TYPE_CREATE 1

typedef struct hshm_ {
  int8_t init_type;
  char name[256 - sizeof(int8_t)];
  size_t size;
  
  void* addr;
} hshm_t;

int hshm_init(hshm_t* shm, const char* name, size_t size, int init_type);
int hshm_deinit(hshm_t* shm);

int hshm_write(hshm_t* shm, const void* data, size_t size);
int hshm_read(hshm_t* shm, void* data, size_t size);


int hshm_realloc(hshm_t* shm, size_t new_size);





*/
#endif