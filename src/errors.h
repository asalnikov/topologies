#ifndef TOPOLOGIES_ERROR_H
# define TOPOLOGIES_ERROR_H

#define TOP_E_FOPEN 1
#define TOP_E_FSTAT 2
#define TOP_E_FMMAP 3
#define TOP_E_EVAL 4
#define TOP_E_CONN 5
#define TOP_E_NOMOD 6
#define TOP_E_NONET 7
#define TOP_E_BADGATE 8
#define TOP_E_ALLOC 9
#define TOP_E_JSON 10
#define TOP_E_TOKEN 11
#define TOP_E_LOOP 12
#define TOP_E_REGEX 13
#define TOP_E_ROOT 14

int
return_error (char *buf, size_t size, int e, const char *errmsg, ...);

#endif
