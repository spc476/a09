/****************************************************************************
*
*   Structures and function definitions for as09 (6809 assembler)
*   Copyright (C) 2023 Sean Conner
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*   Comments, questions and criticisms can be sent to: sean@conman.org
*
****************************************************************************/

#ifndef I_CBED9C6E_E5CA_55A4_A099_890E6F362B3C
#define I_CBED9C6E_E5CA_55A4_A099_890E6F362B3C

#if !defined(__GNUC__) && !defined(__clang__)
#  define __attribute__(x)
#endif

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wpadded"
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wdeclaration-after-statement"
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include <cgilib6/nodelist.h>
#include <cgilib6/tree.h>
#include <mc6809.h>
#include <mc6809dis.h>

enum backend
{
  BACKEND_BIN,
  BACKEND_RSDOS,
  BACKEND_SREC,
  BACKEND_TEST,
};

enum admode
{
  AM_IMMED,
  AM_DIRECT,
  AM_INDEX,
  AM_EXTENDED,
  AM_INHERENT,
  AM_BRANCH,
};

enum symtype
{
  SYM_UNDEF,
  SYM_ADDRESS,
  SYM_EQU,
  SYM_SET,
  SYM_PUBLIC,
  SYM_EXTERN,
};

enum associative
{
  AS_LEFT,
  AS_RIGHT,
};

enum operator
{
  OP_LOR,
  OP_LAND,
  OP_GT,
  OP_GE,
  OP_EQ,
  OP_LE,
  OP_LT,
  OP_NE,
  OP_BOR,
  OP_BEOR,
  OP_BAND,
  OP_SHR,
  OP_SHL,
  OP_SUB,
  OP_ADD,
  OP_MUL,
  OP_DIV,
  OP_MOD,
};

typedef struct label
{
  unsigned char s;
  char          text[63];
} label;

struct buffer
{
  char    buf[133];
  size_t  widx;
  size_t  ridx;
};

struct a09;
struct opcdata;
struct symbol;
union format;

struct format_default
{
  enum backend backend;
  bool (*cmdline)   (union format *,int *,char *[]);
  bool (*pass_start)(union format *,struct a09 *,int);
  bool (*pass_end)  (union format *,struct a09 *,int);
  bool (*inst_write)(union format *,struct opcdata *);
  bool (*data_write)(union format *,struct opcdata *,char const *,size_t);
  bool (*dp)        (union format *,struct opcdata *);
  bool (*code)      (union format *,struct opcdata *);
  bool (*align)     (union format *,struct opcdata *);
  bool (*end)       (union format *,struct opcdata *,struct symbol const *);
  bool (*org)       (union format *,struct opcdata *,uint16_t,uint16_t);
  bool (*rmb)       (union format *,struct opcdata *);
  bool (*setdp)     (union format *,struct opcdata *);
  bool (*test)      (union format *,struct opcdata *);
  bool (*tron)      (union format *,struct opcdata *);
  bool (*troff)     (union format *,struct opcdata *);
  bool (*trigger)   (union format *,struct opcdata *);
  bool (*endtst)    (union format *,struct opcdata *);
  bool (*fini)      (union format *,struct a09 *);
};

struct format_bin
{
  enum backend backend;
  bool (*cmdline)   (union format *,int *,char *[]);
  bool (*pass_start)(union format *,struct a09 *,int);
  bool (*pass_end)  (union format *,struct a09 *,int);
  bool (*inst_write)(union format *,struct opcdata *);
  bool (*data_write)(union format *,struct opcdata *,char const *,size_t);
  bool (*dp)        (union format *,struct opcdata *);
  bool (*code)      (union format *,struct opcdata *);
  bool (*align)     (union format *,struct opcdata *);
  bool (*end)       (union format *,struct opcdata *,struct symbol const *);
  bool (*org)       (union format *,struct opcdata *,uint16_t,uint16_t);
  bool (*rmb)       (union format *,struct opcdata *);
  bool (*setdp)     (union format *,struct opcdata *);
  bool (*test)      (union format *,struct opcdata *);
  bool (*tron)      (union format *,struct opcdata *);
  bool (*troff)     (union format *,struct opcdata *);
  bool (*trigger)   (union format *,struct opcdata *);
  bool (*endtst)    (union format *,struct opcdata *);
  bool (*fini)      (union format *,struct a09 *);
  bool   first;
};

struct format_rsdos
{
  enum backend backend;
  bool (*cmdline)   (union format *,int *,char *[]);
  bool (*pass_start)(union format *,struct a09 *,int);
  bool (*pass_end)  (union format *,struct a09 *,int);
  bool (*inst_write)(union format *,struct opcdata *);
  bool (*data_write)(union format *,struct opcdata *,char const *,size_t);
  bool (*dp)        (union format *,struct opcdata *);
  bool (*code)      (union format *,struct opcdata *);
  bool (*align)     (union format *,struct opcdata *);
  bool (*end)       (union format *,struct opcdata *,struct symbol const *);
  bool (*org)       (union format *,struct opcdata *,uint16_t,uint16_t);
  bool (*rmb)       (union format *,struct opcdata *);
  bool (*setdp)     (union format *,struct opcdata *);
  bool (*test)      (union format *,struct opcdata *);
  bool (*tron)      (union format *,struct opcdata *);
  bool (*troff)     (union format *,struct opcdata *);
  bool (*trigger)   (union format *,struct opcdata *);
  bool (*endtst)    (union format *,struct opcdata *);
  bool (*fini)      (union format *,struct a09 *);
  long     section_hdr;
  long     section_start;
  bool     endf;
  uint16_t entry;
};

struct format_srec
{
  enum backend backend;
  bool (*cmdline)   (union format *,int *,char *[]);
  bool (*pass_start)(union format *,struct a09 *,int);
  bool (*pass_end)  (union format *,struct a09 *,int);
  bool (*inst_write)(union format *,struct opcdata *);
  bool (*data_write)(union format *,struct opcdata *,char const *,size_t);
  bool (*dp)        (union format *,struct opcdata *);
  bool (*code)      (union format *,struct opcdata *);
  bool (*align)     (union format *,struct opcdata *);
  bool (*end)       (union format *,struct opcdata *,struct symbol const *);
  bool (*org)       (union format *,struct opcdata *,uint16_t,uint16_t);
  bool (*rmb)       (union format *,struct opcdata *);
  bool (*setdp)     (union format *,struct opcdata *);
  bool (*test)      (union format *,struct opcdata *);
  bool (*tron)      (union format *,struct opcdata *);
  bool (*troff)     (union format *,struct opcdata *);
  bool (*trigger)   (union format *,struct opcdata *);
  bool (*endtst)    (union format *,struct opcdata *);
  bool (*fini)      (union format *,struct a09 *);
  char const    *S0file;
  uint16_t       addr;
  uint16_t       exec;
  size_t         recsize;
  size_t         idx;
  bool           endf;
  bool           execf;
  bool           override;
  unsigned char  buffer[252];
};

struct testdata;

struct format_test
{
  enum backend backend;
  bool (*cmdline)   (union format *,int *,char *[]);
  bool (*pass_start)(union format *,struct a09 *,int);
  bool (*pass_end)  (union format *,struct a09 *,int);
  bool (*inst_write)(union format *,struct opcdata *);
  bool (*data_write)(union format *,struct opcdata *,char const *,size_t);
  bool (*dp)        (union format *,struct opcdata *);
  bool (*code)      (union format *,struct opcdata *);
  bool (*align)     (union format *,struct opcdata *);
  bool (*end)       (union format *,struct opcdata *,struct symbol const *);
  bool (*org)       (union format *,struct opcdata *,uint16_t,uint16_t);
  bool (*rmb)       (union format *,struct opcdata *);
  bool (*setdp)     (union format *,struct opcdata *);
  bool (*test)      (union format *,struct opcdata *);
  bool (*tron)      (union format *,struct opcdata *);
  bool (*troff)     (union format *,struct opcdata *);
  bool (*trigger)   (union format *,struct opcdata *);
  bool (*endtst)    (union format *,struct opcdata *);
  bool (*fini)      (union format *,struct a09 *);
  bool             intest;
  struct testdata *data;
};

union format
{
  enum backend          backend;
  struct format_default def;
  struct format_bin     bin;
  struct format_rsdos   rsdos;
  struct format_srec    srec;
  struct format_test    test;
};

struct nowarn
{
  char tag[5];
};

struct a09
{
  char const    *infile;
  char const    *outfile;
  char const    *listfile;
  FILE          *in;
  FILE          *out;
  FILE          *list;
  union format   format;
  struct buffer  inbuf;
  size_t         lnum;
  tree__s       *symtab;
  List           symbols;
  struct nowarn *nowarn;
  size_t         nowsize;
  label          label;
  uint16_t       pc;
  unsigned char  dp;
  bool           debug;
  bool           mkdeps;
  int            mkdlen;
};

struct symbol
{
  tree__s       tree;
  Node          node;
  label         name;
  enum symtype  type;
  uint16_t      value;
  char const   *filename;
  size_t        ldef;
  size_t        bits;
  size_t        refs;
};

struct value
{
  uint16_t      value;
  size_t        bits;
  unsigned char postbyte;
  bool          unknownpass1;
  bool          defined;
  bool          external;
};

struct opcdata
{
  struct a09          *a09;
  struct opcode const *op;
  struct buffer       *buffer;
  label                label;
  int                  pass;
  uint16_t             sz;
  unsigned char        bytes[6];
  bool                 data;
  size_t               datasz;
  enum admode          mode;
  struct value         value;
  size_t               bits;
  bool                 pcrel;
  
  /*--------------------------------------------------------------------------
  ; This is solely for the use of the INCLUDE pseudo-op.  We want to include
  ; the line in the listing file before the included file appears in the
  ; listing file, but that's it.  This is here to prevent the INCLUDE line
  ; being listed twice.  If it's set, then print_list() won't print the
  ; line.
  ;--------------------------------------------------------------------------*/
  
  bool                 includehack;
};

struct opcode
{
  char            name[8];
  bool          (*func)(struct opcdata *);
  unsigned char   opcode;
  unsigned char   page;
  bool            bit16;
};

struct indexregs
{
  char          reg[3];
  unsigned char pushpull;
  unsigned char tehi;
  unsigned char telo;
  bool          b16;
};

struct optable
{
  enum operator    op;
  enum associative as;
  unsigned int     pri;
};

/**************************************************************************/

extern char const MSG_DEBUG[];
extern char const MSG_WARNING[];
extern char const MSG_ERROR[];

extern char const format_bin_usage[];
extern char const format_rsdos_usage[];
extern char const format_srec_usage[];
extern char const format_test_usage[];

extern bool                  message            (struct a09 *,char const *restrict,char const *restrict,...) __attribute__((format(printf,3,4)));
extern void                  add_file_dep       (struct a09 *,char const *);
extern bool                  read_line          (FILE *,struct buffer *);
extern bool                  parse_string       (struct a09 *,struct buffer *restrict,struct buffer *restrict);
extern bool                  parse_label        (label *,struct buffer *,struct a09 *,int);
extern bool                  parse_op           (struct buffer *,struct opcode const **);
extern char                  skip_space         (struct buffer *);
extern bool                  print_list         (struct a09 *,struct opcdata *,bool);
extern bool                  assemble_pass      (struct a09 *,int);
extern struct opcode const  *op_find            (char const *);
extern bool                  s2num              (struct a09 *,uint16_t *,struct buffer *,uint16_t);
extern struct optable const *get_op             (struct buffer *);
extern bool                  expr               (struct value *,struct a09 *,struct buffer *,int);
extern struct symbol        *symbol_find        (struct a09 *,label const *);
extern struct symbol        *symbol_add         (struct a09 *,label const *,uint16_t);
extern bool                  format_bin_init    (struct format_bin   *,struct a09 *);
extern bool                  format_rsdos_init  (struct format_rsdos *,struct a09 *);
extern bool                  format_srec_init   (struct format_srec  *,struct a09 *);
extern bool                  format_test_init   (struct format_test  *,struct a09 *);
extern bool                  fdefault           (union format *,struct opcdata *);
extern bool                  fdefault_end       (union format *,struct opcdata *,struct symbol const *);
extern bool                  fdefault_org       (union format *,struct opcdata *,uint16_t,uint16_t);
extern bool                  fdefault_cmdline   (union format *,int *,char *[]);
extern bool                  fdefault_pass      (union format *,struct a09 *,int);
extern bool                  fdefault_inst_write(union format *,struct opcdata *);
extern bool                  fdefault_data_write(union format *,struct opcdata *,char const *,size_t);
extern bool                  fdefault_test      (union format *,struct opcdata *);
extern bool                  fdefault_fini      (union format *,struct a09 *);

/**************************************************************************/

static inline size_t min(size_t a,size_t b)
{
  return a < b ? a : b;
}

/**************************************************************************/

static inline struct symbol *tree2sym(tree__s *tree)
{
  assert(tree != NULL);
#if defined(__clang__)
#  pragma clang diagnostic push "-Wcast-align"
#  pragma clang diagnostic ignored "-Wcast-align"
#endif
  return (struct symbol *)((char *)tree - offsetof(struct symbol,tree));
#if defined(__clang__)
#  pragma clang diagnostic pop "-Wcast-align"
#endif
}

/**************************************************************************/

static inline struct symbol *node2sym(Node *node)
{
  assert(node != NULL);
#if defined(__clang__)
#  pragma clang diagnostic push "-Wcast-align"
#  pragma clang diagnostic ignored "-Wcast-align"
#endif
  return (struct symbol *)((char *)node - offsetof(struct symbol,node));
#if defined(__clang__)
#  pragma clang diagnostic pop "-Wcast-align"
#endif
}

/**************************************************************************/

#endif
