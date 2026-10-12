#ifndef _HCFG_H_
#define _HCFG_H_
#include "hdef.h"


#define HCFG_TYPE_CONF  0
#define HCFG_TYPE_INI   1
#define HCFG_TYPE_END   2


typedef struct hcfg_{
  uint8_t   cfg_type;
  uint32_t  size;
  void*     handle;
}hcfg_t;



HFC_EXPORT int hcfg_init(hcfg_t* cfg,const char *filename,uint8_t cfg_type);
HFC_EXPORT int hcfg_fini(hcfg_t* c);
HFC_EXPORT int hcfg_str(hcfg_t* c,const char* key,char *val,size_t val_size);
// int hcfg_int(hcfg_t* c,const char* key,int *val);
// int hcfg_int64(hcfg_t* c,const char* key,int64_t *val);
// int hcfg_double(hcfg_t* c,const char* key,double *val);






#endif