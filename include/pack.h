/*
  Copyright (c) 2007 Electronic Theatre Controls, Inc
  All rights reserved. 
  
  Description:
    Header file for pack.c
*/

#ifndef PACK_H
#define PACK_H

#include "types.h"

#define MAKEIP(a,b,c,d)    (a<<24|b<<16|c<<8|d)

#ifdef __cplusplus
extern "C" {
#endif

char *packSTR(char *charptr, char *val);
char *packSTRlen(char *charptr, char *val, uint32_t len);
char *packMEM(char *charptr, char *val, uint32_t cnt);
char *packUINT8(char *charptr, uint8_t val);
char *packUINT16(char *charptr, uint16_t val);
char *packUINT24(char *charptr, uint32_t val);
char *packUINT32(char *charptr, uint32_t val);

char *unpackUINT8(char *charptr, uint8_t *val);
char *unpackUINT16(char *charptr, uint16_t *val);
char *unpackUINT24(char *charptr, uint32_t *val);
char *unpackUINT32(char *charptr, uint32_t *val);

#ifdef __cplusplus
}
#endif

#endif // PACK_H
