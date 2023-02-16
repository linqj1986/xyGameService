#ifndef _DES_H_
#define _DES_H_

#include "globals.h"

void DES3( BYTE* key, const int nKeyLen, BYTE *text, BYTE *mtext );
void _DES3( BYTE* key, const int nKeyLen, BYTE *text, BYTE *mtext );
void DES(BYTE *key,BYTE *text,BYTE *mtext);
void _DES(BYTE *key,BYTE *text,BYTE *mtext);
void encrypt0(BYTE *text,BYTE *mtext);
void discrypt0(BYTE *mtext,BYTE *text);
void expand0(BYTE *in,BYTE *out);
void compress0(BYTE *out,BYTE *in);
void compress016(char *out,BYTE *in);
void setkeystar(BYTE bits[64]);
void LS(BYTE *bits,BYTE *buffer,int count);
void son(BYTE *cc,BYTE *dd,BYTE *kk);
void ip(BYTE *text,BYTE *ll,BYTE *rr);
void _ip(BYTE *text,BYTE *ll,BYTE *rr);
void F(int n,BYTE *ll,BYTE *rr,BYTE *LL,BYTE *RR);
void s_box(char *aa,char *bb);

#endif