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
#include <limits.h>
#include <assert.h>

#include <cgilib7/tree.h>

enum
{
  BYTE,
  WORD,
};

enum
{
  DATA,
  INSTRUCTION
};

enum backend
{
  BACKEND_BIN,
  BACKEND_RSDOS,
  BACKEND_SREC,
  BACKEND_BASIC,
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
  OP_EXP,
};

typedef struct label
{
  unsigned char s;
  char          text[63];
} label;

struct buffer
{
  char   buf[133];
  size_t widx;
  size_t ridx;
};

struct a09;
struct opcdata;
struct symbol;
struct testdata;
struct arg;

struct format
{
  enum backend backend;
  bool (*cmdline)   (struct format *,struct a09 *,struct arg *,char);
  bool (*pass_start)(struct format *,struct a09 *,int);
  bool (*pass_end)  (struct format *,struct a09 *,int);
  bool (*write)     (struct format *,struct opcdata *,void const *,size_t,bool);
  bool (*opt)       (struct format *,struct opcdata *,label *);
  bool (*dp)        (struct format *,struct opcdata *);
  bool (*code)      (struct format *,struct opcdata *);
  bool (*align)     (struct format *,struct opcdata *);
  bool (*end)       (struct format *,struct opcdata *,struct symbol const *);
  bool (*org)       (struct format *,struct opcdata *);
  bool (*rmb)       (struct format *,struct opcdata *);
  bool (*setdp)     (struct format *,struct opcdata *);
  bool (*test)      (struct format *,struct opcdata *);
  bool (*tron)      (struct format *,struct opcdata *);
  bool (*troff)     (struct format *,struct opcdata *);
  bool (*Assert)    (struct format *,struct opcdata *);
  bool (*endtst)    (struct format *,struct opcdata *);
  bool (*Float)     (struct format *,struct opcdata *);
  bool (*fini)      (struct format *,struct a09 *);
  void  *data;
};

struct a09
{
  char const       *infile;
  char const       *outfile;
  char const       *listfile;
  char const       *corefile;
  char            **deps;
  char            **includes;
  size_t            ndeps;
  size_t            nincs;
  FILE             *in;
  FILE             *out;
  FILE             *list;
  struct testdata  *tests;
  struct format     format;
  struct buffer     inbuf;
  size_t            lnum;
  tree__s          *symtab;
  unsigned char     nowarn[10000 / CHAR_BIT];
  label             label;
  uint16_t          pc;
  unsigned char     dp;
  bool              debug;
  bool              mkdeps;
  bool              obj;
  bool              runtests;
  bool              rndtests;
  bool              tapout;
  bool              cc;
  bool              cycles;
};

struct symbol
{
  tree__s       tree;
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

struct fvalue
{
  union { float f; double d; } value;
  bool unknownpass1;
  bool defined;
  bool external;
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
  char            flag[8];
  bool          (*func)(struct opcdata *);
  unsigned char   cycles; /* imm---dir +2, idx +2, ext +3 */
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
  bool          bit16;
};

struct optable
{
  enum operator    op;
  enum associative as;
  unsigned int     pri;
};

struct arg
{
  char   **argv;
  size_t   argc;
  size_t   ci;
  size_t   si;
};

/**************************************************************************/

extern char const MSG_DEBUG[];
extern char const MSG_WARNING[];
extern char const MSG_ERROR[];

extern char const format_bin_usage[];
extern char const format_rsdos_usage[];
extern char const format_srec_usage[];
extern char const format_basic_usage[];

extern void                  arg_init           (struct arg *,char **,int);
extern char                  arg_next           (struct arg *);
extern char                 *arg_arg            (struct arg *);
extern int                   arg_done           (struct arg *);
extern bool                  arg_unsigned_long  (unsigned long int *,struct arg *,unsigned long int,unsigned long int);
extern bool                  arg_size_t         (size_t            *,struct arg *,unsigned long int,unsigned long int);
extern bool                  arg_uint16_t       (uint16_t          *,struct arg *,unsigned long int,unsigned long int);
extern bool                  arg_uint8_t        (uint8_t           *,struct arg *,unsigned long int,unsigned long int);
extern bool                  enable_warning     (struct a09 *,char const *);
extern bool                  disable_warning    (struct a09 *,char const *);
extern bool                  message            (struct a09 *,char const *restrict,char const *restrict,...) __attribute__((format(printf,3,4)));
extern char                 *add_file_dep       (struct a09 *,char const *);
extern bool                  read_line          (FILE *,struct buffer *);
extern bool                  collect_esc_string (struct a09 *,struct buffer *restirct,struct buffer *restrict,char);
extern bool                  parse_string       (struct a09 *,struct buffer *restrict,struct buffer *restrict);
extern bool                  read_label         (struct buffer *,label *,char);
extern bool                  parse_label        (label *,struct buffer *,struct a09 *,int);
extern void                  upper_label        (label *);
extern bool                  parse_op           (struct buffer *,struct opcode const **);
extern char                  skip_space         (struct buffer *);
extern bool                  print_list         (struct a09 *,struct opcdata *,bool);
extern bool                  assemble_pass      (struct a09 *,int);
extern struct opcode const  *op_find            (char const *);
extern bool                  s2num              (struct a09 *,uint16_t *,struct buffer *,uint16_t);
extern struct optable const *get_op             (struct buffer *);
extern bool                  expr               (struct value  *,struct a09 *,struct buffer *,int);
extern bool                  rexpr              (struct fvalue *,struct a09 *,struct buffer *,int,bool);
extern int                   symstrcmp          (void const *restrict,void const *restrict);
extern struct symbol        *symbol_add         (struct a09 *,label const *,uint16_t);
extern void                  symbol_freetable   (tree__s *);
extern bool                  format_bin_init    (struct a09 *);
extern bool                  format_rsdos_init  (struct a09 *);
extern bool                  format_srec_init   (struct a09 *);
extern bool                  format_basic_init  (struct a09 *);
extern bool                  fdefault           (struct format *,struct opcdata *);
extern bool                  fdefault_end       (struct format *,struct opcdata *,struct symbol const *);
extern bool                  fdefault_cmdline   (struct format *,struct a09 *,struct arg *,char);
extern bool                  fdefault_pass      (struct format *,struct a09 *,int);
extern bool                  fdefault_write     (struct format *,struct opcdata *,void const *,size_t,bool);
extern bool                  fdefault__opt      (struct format *,struct opcdata *,label *);
extern bool                  fdefault__test     (struct format *,struct opcdata *);
extern bool                  fdefault_fini      (struct format *,struct a09 *);
extern bool                  freal__ieee        (struct format *,struct opcdata *);
extern bool                  freal__msfp        (struct format *,struct opcdata *);
extern bool                  freal__lbfp        (struct format *,struct opcdata *);
extern bool                  test_init          (struct a09 *);
extern bool                  test_pass_start    (struct a09 *,int);
extern bool                  test_pass_end      (struct a09 *,int);
extern bool                  test_align         (struct opcdata *);
extern bool                  test_org           (struct opcdata *);
extern bool                  test_rmb           (struct opcdata *);
extern bool                  test__opt          (struct opcdata *);
extern void                  test_run           (struct a09 *);
extern bool                  test_fini          (struct a09 *);

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

static inline struct symbol *symbol_find(struct a09 *a09,label const *name)
{
  assert(a09  != NULL);
  assert(name != NULL);
  
  tree__s *tree = tree_find(a09->symtab,name,symstrcmp);
  if (tree != NULL)
    return tree2sym(tree);
  else
    return NULL;
}

/**************************************************************************/

#endif
