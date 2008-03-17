/*
  Copyright (c) 2007 Electronic Theatre Controls, Inc
  All rights reserved. 
  
  Description:
    Header file for pack.c
*/

#ifndef PACK_H
#define PACK_H

#include "arch/cc.h"

#define MAKEIP(a,b,c,d)    (a<<24|b<<16|c<<8|d)

char *packSTR(char *charptr, char *val);
char *packSTRlen(char *charptr, char *val, u32_t len);
char *packMEM(char *charptr, char *val, u32_t cnt);
char *packUINT8(char *charptr, u8_t val);
char *packUINT16(char *charptr, u16_t val);
char *packUINT24(char *charptr, u32_t val);
char *packUINT32(char *charptr, u32_t val);

char *unpackUINT8(char *charptr, u8_t *val);
char *unpackUINT16(char *charptr, u16_t *val);
char *unpackUINT24(char *charptr, u32_t *val);
char *unpackUINT32(char *charptr, u32_t *val);

#endif // PACK_H
