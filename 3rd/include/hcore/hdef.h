#ifndef _HDEF_H_
#define _HDEF_H_


#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// DEF-INTERFACE
#define HFC_DYNAMICLIB
#if defined(HFC_STATICLIB) || defined(HFC_SOURCE)
  #define HFC_EXPORT
#elif defined(_MSC_VER)
  #if defined(HFC_DYNAMICLIB) || defined(HFC_EXPORTS)
      #define HFC_EXPORT __declspec(dllexport)
  #else
      #define HFC_EXPORT  __declspec(dllimport)
  #endif
#elif defined(__GNUC__)
  #define HFC_EXPORT __attribute__((visibility("default")))
#else
  #define HFC_EXPORT
#endif

// DEF-RETURN
#define HC_RET_OK        0x00000000
#define HC_RET_FILE      0x00000001
#define HC_RET_NOSPACE   0x00000002
#define HC_RET_NOTFOUND  0x00000003
#define HC_RET_HASEXIST  0x00000004
#define HC_RET_MALLOC    0x00000005
#define HC_RET_INIT      0x00000006
#define HC_RET_PARAM_NUM 0x00000007
#define HC_RET_LIB       0x00000008

#define HC_RET_PARAM(N) (0x000000F0|(N))
#define HC_RET(BASECODE,DETAILCODE) ((BASECODE)|(DETAILCODE))


#define HC_RET_BASE_DS   0x00000010
#define HC_RET_BASE_LOG  0x00000020
#define HC_RET_BASE_CFG  0x00000030
#define HC_RET_BASE_TIME 0x00000040
#define HC_RET_BASE_SHM  0x00000050


//define for tool 
#define HC_ALLOC(S) malloc(S)
#define HC_FREE(P) free(P)
#define HC_REALLOC(P,S) realloc(P,S)
#define HC_GETMINPOW(V,INITV) while (INITV < V) INITV <<= 1
#define HC_RET_WHEN(VAL,RETCODE) if(VAL) return RETCODE
#define HC_EXEC_RET_WHEN(VAL,CMD,RETCODE) if(VAL){CMD;return RETCODE;} 



#endif