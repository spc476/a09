/****************************************************************************
*
*   The Unit Test backend
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <errno.h>

#include <mc6809.h>
#include <mc6809dis.h>

#include "a09.h"

/**************************************************************************/

#define MAX_PROG 64

enum vmops
{
  VM_LOR,       /* why yes, these do match enum operator */
  VM_LAND,
  VM_GT,
  VM_GE,
  VM_EQ,
  VM_LE,
  VM_LT,
  VM_NE,
  VM_BOR,
  VM_BEOR,
  VM_BAND,
  VM_SHR,
  VM_SHL,
  VM_SUB,
  VM_ADD,
  VM_MUL,
  VM_DIV,
  VM_MOD,
  VM_NEG,       /* and now new operators */
  VM_NOT,
  VM_LIT,
  VM_AT8,
  VM_AT16,
  VM_CPUCC,
  VM_CPUCCc,
  VM_CPUCCv,
  VM_CPUCCz,
  VM_CPUCCn,
  VM_CPUCCi,
  VM_CPUCCh,
  VM_CPUCCf,
  VM_CPUCCe,
  VM_CPUA,
  VM_CPUB,
  VM_CPUDP,
  VM_CPUD,
  VM_CPUX,
  VM_CPUY,
  VM_CPUU,
  VM_CPUS,
  VM_CPUPC,
  VM_IDX,       /* "/offset,x" == "(/x + offset)" */
  VM_IDY,
  VM_IDS,
  VM_IDU,
  VM_SCMP,
  VM_SEX,
  VM_EXIT,
};

enum
{
  TEST_FAILED = MC6809_FAULT_user,
  TEST_NON_READ_MEM,
  TEST_WEEDS,
  TEST_NON_WRITE_MEM,
  TEST_max,
};

enum protection
{
  PROT_READ,
  PROT_WRITE,
  PROT_EXEC,
  PROT_TRON,
};

struct memprot
{
  bool read  : 1;
  bool write : 1;
  bool exec  : 1;
  bool tron  : 1;
  bool check : 1;
};

struct unittest
{
  uint16_t       addr;
  char const    *filename;
  size_t         line;
  struct buffer  name;
};

struct vmcode
{
  size_t        line;
  enum vmops    prog[MAX_PROG];
  char          tag[133];
  struct buffer str;
};

struct Assert
{
  tree__s        tree;
  uint16_t       here;
  size_t         cnt;
  struct vmcode *Asserts;
};

struct testdata
{
  struct a09      *a09;
  const char      *corefile;
  tree__s         *Asserts;
  struct unittest *units;
  size_t           nunits;
  mc6809__t        cpu;
  mc6809dis__t     dis;
  uint16_t         addr;
  uint16_t         sp;
  mc6809byte__t    fill;
  bool             tron;
  char             errbuf[128];
  mc6809byte__t    memory[65536u];
  struct memprot   prot  [65536u];
};

struct labeltable
{
  label      label;
  enum vmops op;
};

static bool ft_expr(enum vmops [],size_t,size_t *,struct buffer *,struct a09 *,struct buffer *,int);

/**************************************************************************/

static inline struct Assert *tree2Assert(tree__s *tree)
{
  assert(tree != NULL);
#if defined(__clang__)
#  pragma clang diagnostic push "-Wcast-align"
#  pragma clang diasnostic ignored "-Wcast-align"
#endif
  return (struct Assert *)((char *)tree - offsetof(struct Assert,tree));
#if defined(__clang__)
#  pragma clang diagnostic pop "-Wcast-align"
#endif
}

/**************************************************************************/

char const format_test_usage[] =
        "\n"
        "Test format options:\n"
        "\t-S addr\t\taddress of system stack (default=0xFFF0)\n"
        "\t-F byte\t\tfill memory with value (default=0x01, illegal inst)\n"
        "\t-R range\tmark memory read-only (see below)\n"
        "\t-W range\tmark memory write-only (see below)\n"
        "\t-E range\tmark memory as code (see below)\n"
        "\t-T range\ttrace execution of code (see below)\n"
        "\t-D file\t\tcore file (of 6809 VM) name\n"
        "\n"
        "NOTE:\tmemory ranges can be specified as:\n"
        "\t\t<low-address>[ '-' <high-address>]\n"
        "\tmultiple ranges can be specified, separated by a comma\n"
        "\tBy default, all memory is readable, writable and executable,\n"
        "\tbut not traced.  You can also enable tracing with the\n"
        "\t.TRON directive.\n"
        ;
        
/**************************************************************************/

static void range(struct a09 *a09,struct memprot *mem,int *pi,char *argv[],enum protection prot)
{
  assert(mem  != NULL);
  assert(pi   != NULL);
  assert(*pi  >  0);
  assert(argv != NULL);
  
  unsigned long int  low;
  unsigned long int  high;
  char              *r;
  
  if (argv[*pi][2] == '\0')
    r = argv[++(*pi)];
  else
    r = &argv[*pi][2];
    
  while(*r)
  {
    low = strtoul(r,&r,0);
    if (*r == '-')
    {
      r++;
      high = strtoul(r,&r,0);
    }
    else
      high = low;
      
    if ((low > 65535u) || (high > 65535u))
    {
      message(a09,MSG_ERROR,"E0069: address exceeds address space");
      exit(1);
    }
    
    switch(prot)
    {
      case PROT_READ:
           for ( ; low <= high ; low++)
           {
             mem[low].read  = true;
             mem[low].write = false;
             mem[low].exec  = false;
           }
           break;
           
      case PROT_WRITE:
           for ( ; low <= high ; low++)
           {
             mem[low].read  = false;
             mem[low].write = true;
             mem[low].exec  = false;
           }
           break;
           
      case PROT_EXEC:
           for ( ; low <= high ; low++)
             mem[low].exec = true;
           break;
           
      case PROT_TRON:
           for ( ; low <= high ; low++)
             mem[low].tron = true;
           break;
    }
    
    if (*r == ',')
      r++;
  }
}

/**************************************************************************/

static bool ftest_cmdline(union format *fmt,struct a09 *a09,int *pi,char *argv[])
{
  assert(fmt  != NULL);
  assert(a09  != NULL);
  assert(pi   != NULL);
  assert(*pi  >  0);
  assert(argv != NULL);
  assert(fmt->backend == BACKEND_TEST);
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  int                 i    = *pi;
  unsigned long int   value;
  
  switch(argv[i][1])
  {
    case 'S':
         if (argv[i][2] == '\0')
           value = strtoul(argv[++i],NULL,0);
         else
           value = strtoul(&argv[i][2],NULL,0);
           
         if (value > 65535u)
         {
           message(a09,MSG_ERROR,"E0069: address exceeds address space");
           exit(1);
         }
         
         data->sp = value;
         break;
         
    case 'D':
         if (argv[i][2] == '\0')
           data->corefile = argv[++i];
         else
           data->corefile = &argv[i][2];
         break;
         
    case 'F':
         if (argv[i][2] == '\0')
           value = strtoul(argv[++i],NULL,0);
         else
           value = strtoul(&argv[i][2],NULL,0);
           
         if (value > 255)
         {
           message(a09,MSG_ERROR,"E0072: byte should be 0..255");
           exit(1);
         }
         
         data->fill = value;
         break;
         
    case 'R':
         range(a09,data->prot,pi,argv,PROT_READ);
         break;
         
    case 'W':
         range(a09,data->prot,pi,argv,PROT_WRITE);
         break;
         
    case 'E':
         range(a09,data->prot,pi,argv,PROT_EXEC);
         break;
         
    case 'T':
         range(a09,data->prot,pi,argv,PROT_TRON);
         break;
         
    default:
         return false;
  }
  
  *pi = i;
  return true;
}

/**************************************************************************/

static int Assertaddrcmp(void const *restrict needle,void const *restrict haystack)
{
  uint16_t      const *key   = needle;
  struct Assert const *value = haystack;
  
  if (*key < value->here)
    return -1;
  else if (*key > value->here)
    return  1;
  else
    return  0;
}

/**************************************************************************/

static int Asserttreecmp(void const *restrict needle,void const *restrict haystack)
{
  struct Assert const *key   = needle;
  struct Assert const *value = haystack;
  
  if (key->here < value->here)
    return -1;
  else if (key->here > value->here)
    return  1;
  else
    return  0;
}

/**************************************************************************/

static bool runvm(struct a09 *a09,mc6809__t *cpu,struct vmcode *test)
{
  assert(a09  != NULL);
  assert(cpu  != NULL);
  assert(test != NULL);
  
  struct format_test *bend = cpu->user;
  struct testdata    *data = bend->data;
  uint16_t            stack[15];
  uint16_t            result;
  uint16_t            addr;
  size_t              sp = sizeof(stack) / sizeof(stack[0]);
  size_t              ip = 0;
  int                 rc;
  
  /*--------------------------------------------------------------
  ; I control the code generation, so I can skip checks that would
  ; otherwise have to be made.
  ;---------------------------------------------------------------*/
  
  while(true)
  {
    switch(test->prog[ip++])
    {
      case VM_LOR:
           result      = stack[sp + 1] || stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_LAND:
           result      = stack[sp + 1] && stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_GT:
           result      = stack[sp + 1] > stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_GE:
           result      = stack[sp + 1] >= stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_EQ:
           result      = stack[sp + 1] == stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_LE:
           result      = stack[sp + 1] <= stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_LT:
           result      = stack[sp + 1] < stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_NE:
           result      = stack[sp + 1] != stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_BOR:
           result      = stack[sp + 1] | stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_BEOR:
           result      = stack[sp + 1] ^ stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_BAND:
           result      = stack[sp + 1] & stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_SHR:
           result      = stack[sp + 1] >> stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_SHL:
           result      = stack[sp + 1] << stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_SUB:
           result      = stack[sp + 1] - stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_ADD:
           result      = stack[sp + 1] + stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_MUL:
           result      = stack[sp + 1] * stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_DIV:
           if (stack[sp] == 0)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           result      = stack[sp + 1] / stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_MOD:
           if (stack[sp] == 0)
             return message(a09,MSG_ERROR,"E0008: divide by 0 error");
           result      = stack[sp + 1] % stack[sp];
           stack[++sp] = result;
           break;
           
      case VM_NEG:
           stack[sp] = -stack[sp];
           break;
           
      case VM_NOT:
           stack[sp] = ~stack[sp];
           break;
           
      case VM_LIT:
           stack[--sp] = test->prog[ip++];
           break;
           
      case VM_AT8:
           addr      = stack[sp];
           stack[sp] = data->memory[addr];
           break;
           
      case VM_AT16:
           addr      = stack[sp];
           stack[sp] = (data->memory[addr] << 8) | data->memory[addr + 1];
           break;
           
      case VM_CPUCC:
           stack[--sp] = mc6809_cctobyte(cpu);
           break;
           
      case VM_CPUCCc:
           stack[--sp] = cpu->cc.c;
           break;
           
      case VM_CPUCCv:
           stack[--sp] = cpu->cc.v;
           break;
           
      case VM_CPUCCz:
           stack[--sp] = cpu->cc.z;
           break;
           
      case VM_CPUCCn:
           stack[--sp] = cpu->cc.n;
           break;
           
      case VM_CPUCCi:
           stack[--sp] = cpu->cc.i;
           break;
           
      case VM_CPUCCh:
           stack[--sp] = cpu->cc.h;
           break;
           
      case VM_CPUCCf:
           stack[--sp] = cpu->cc.f;
           break;
           
      case VM_CPUCCe:
           stack[--sp] = cpu->cc.e;
           break;
           
      case VM_CPUA:
           stack[--sp] = cpu->A;
           break;
           
      case VM_CPUB:
           stack[--sp] = cpu->B;
           break;
           
      case VM_CPUDP:
           stack[--sp] = cpu->dp;
           break;
           
      case VM_CPUD:
           stack[--sp] = cpu->d.w;
           break;
           
      case VM_CPUX:
           stack[--sp] = cpu->X.w;
           break;
           
      case VM_CPUY:
           stack[--sp] = cpu->Y.w;
           break;
           
      case VM_CPUU:
           stack[--sp] = cpu->U.w;
           break;
           
      case VM_CPUS:
           stack[--sp] = cpu->S.w;
           break;
           
      case VM_CPUPC:
           stack[--sp] = cpu->pc.w;
           break;
           
      case VM_IDX:
           stack[sp] += cpu->X.w;
           break;
           
      case VM_IDY:
           stack[sp] += cpu->Y.w;
           break;
           
      case VM_IDS:
           stack[sp] += cpu->S.w;
           break;
           
      case VM_IDU:
           stack[sp] += cpu->U.w;
           break;
           
      case VM_SCMP:
           rc = memcmp(&data->memory[stack[sp]],test->str.buf,test->str.widx);
           stack[sp] = 0;
           if (rc < 0)
             stack[--sp] = -1;
           else if (rc > 0)
             stack[--sp] =  1;
           else
             stack[--sp] =  0;
           break;
           
      case VM_SEX:
           if (stack[sp] >= 0x80)
             stack[sp] |= 0xFF00;
           break;
           
      case VM_EXIT:
           assert(sp == (sizeof(stack) / sizeof(stack[0]) - 1));
           return stack[sp] != 0;
    }
  }
}

/**************************************************************************/

static mc6809byte__t ft_cpu_read(mc6809__t *cpu,mc6809addr__t addr,bool inst)
{
  assert(cpu       != NULL);
  assert(cpu->user != NULL);
  (void)inst;
  
  struct format_test *test = cpu->user;
  struct testdata    *data = test->data;
  
  if (!data->prot[addr].read)
  {
    snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X addr=%04X",cpu->pc.w,addr);
    longjmp(cpu->err,TEST_NON_READ_MEM);
  }
  
  if (inst && !data->prot[addr].exec)
  {
    snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X addr=%04X",cpu->pc.w,addr);
    longjmp(cpu->err,TEST_WEEDS);
  }
  
  return data->memory[addr];
}

/**************************************************************************/

static void ft_cpu_write(mc6809__t *cpu,mc6809addr__t addr,mc6809byte__t byte)
{
  assert(cpu       != NULL);
  assert(cpu->user != NULL);
  
  struct format_test *test = cpu->user;
  struct testdata    *data = test->data;
  
  if (!data->prot[addr].write)
  {
    snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X addr=%04X",cpu->pc.w,addr);
    longjmp(cpu->err,TEST_NON_WRITE_MEM);
  }
  if (data->prot[addr].exec)
    message(data->a09,MSG_WARNING,"W0014: possible self-modifying code");
  if (data->prot[addr].tron)
    message(data->a09,MSG_WARNING,"W0016: memory write of %02X to %04X",byte,addr);
  data->memory[addr] = byte;
}

/**************************************************************************/

static void ft_cpu_fault(mc6809__t *cpu,mc6809fault__t fault)
{
  assert(cpu       != NULL);
  assert(cpu->user != NULL);
  
  struct format_test *test = cpu->user;
  struct testdata    *data = test->data;
  
  snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X",cpu->pc.w - 1);
  longjmp(cpu->err,fault);
}

/**************************************************************************/

static mc6809byte__t ft_dis_read(mc6809dis__t *dis,mc6809addr__t addr)
{
  assert(dis       != NULL);
  assert(dis->user != NULL);
  
  struct format_test *test = dis->user;
  struct testdata    *data = test->data;
  
  return data->memory[addr];
}

/**************************************************************************/

static void ft_dis_fault(mc6809dis__t *dis,mc6809fault__t fault)
{
  assert(dis != NULL);
  longjmp(dis->err,fault);
}

/**************************************************************************/

static void upper_label(label *res)
{
  assert(res != NULL);
  for (size_t i = 0 ; i < res->s ; i++)
    res->text[i] = toupper(res->text[i]);
}

/**************************************************************************/

static int regsearch(void const *needle,void const *haystack)
{
  label             const *key   = needle;
  struct labeltable const *value = haystack;
  int                      rc    = memcmp(key->text,value->label.text,min(key->s,value->label.s));
  
  if (rc == 0)
  {
    if (key->s < value->label.s)
      rc = -1;
    else if (key->s > value->label.s)
      rc =  1;
  }
  return rc;
}

/**************************************************************************/

static struct labeltable const mregisters[] =
{
  { .label = { .text = "A"    , .s = 1 } , .op = VM_CPUA   } ,
  { .label = { .text = "B"    , .s = 1 } , .op = VM_CPUB   } ,
  { .label = { .text = "CC"   , .s = 2 } , .op = VM_CPUCC  } ,
  { .label = { .text = "CC.C" , .s = 4 } , .op = VM_CPUCCc } ,
  { .label = { .text = "CC.E" , .s = 4 } , .op = VM_CPUCCe } ,
  { .label = { .text = "CC.F" , .s = 4 } , .op = VM_CPUCCf } ,
  { .label = { .text = "CC.H" , .s = 4 } , .op = VM_CPUCCh } ,
  { .label = { .text = "CC.I" , .s = 4 } , .op = VM_CPUCCi } ,
  { .label = { .text = "CC.N" , .s = 4 } , .op = VM_CPUCCn } ,
  { .label = { .text = "CC.V" , .s = 4 } , .op = VM_CPUCCv } ,
  { .label = { .text = "CC.Z" , .s = 4 } , .op = VM_CPUCCz } ,
  { .label = { .text = "D"    , .s = 1 } , .op = VM_CPUD   } ,
  { .label = { .text = "DP"   , .s = 2 } , .op = VM_CPUDP  } ,
  { .label = { .text = "PC"   , .s = 2 } , .op = VM_CPUPC  } ,
  { .label = { .text = "S"    , .s = 1 } , .op = VM_CPUS   } ,
  { .label = { .text = "U"    , .s = 1 } , .op = VM_CPUU   } ,
  { .label = { .text = "X"    , .s = 1 } , .op = VM_CPUX   } ,
  { .label = { .text = "Y"    , .s = 1 } , .op = VM_CPUY   } ,
};

/**************************************************************************/

static bool ft_index_register(
        enum vmops     prog[],
        size_t         max,
        size_t        *pvip,
        struct a09    *a09,
        struct buffer *buffer,
        int            pass
)
{
  assert(prog   != NULL);
  assert(max    == MAX_PROG);
  assert(pvip   != NULL);
  assert(*pvip  <  max);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert(pass   == 2);
  (void)max;
  
  struct labeltable const *vmreg;
  label                    reg;
  
  skip_space(buffer);
  buffer->ridx--;
  if (!parse_label(&reg,buffer,a09,pass))
    return false;
    
  upper_label(&reg);
  vmreg = bsearch(
            &reg,
            mregisters,
            sizeof(mregisters)/sizeof(mregisters[0]),
            sizeof(mregisters[0]),
            regsearch
          );
  if (vmreg == NULL)
    return message(a09,MSG_ERROR,"E0073: bad register");
    
  switch(vmreg->op)
  {
    case VM_CPUX: prog[(*pvip)++] = VM_IDX; break;
    case VM_CPUY: prog[(*pvip)++] = VM_IDY; break;
    case VM_CPUS: prog[(*pvip)++] = VM_IDS; break;
    case VM_CPUU: prog[(*pvip)++] = VM_IDU; break;
    default: return message(a09,MSG_ERROR,"E0080: not an index register");
  }
  
  return true;
}

/**************************************************************************/

static bool ft_register(
        enum vmops     prog[],
        size_t         max,
        size_t        *pvip,
        struct a09    *a09,
        struct buffer *buffer,
        int            pass
)
{
  assert(prog   != NULL);
  assert(max    == MAX_PROG);
  assert(pvip   != NULL);
  assert(*pvip  <  max);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert(pass   == 2);
  
  struct labeltable const *vmreg;
  label                    reg;
  char                     c;
  bool                     earlycomma = false;
  
  /*--------------------------------------------------
  ; handle "/,x" as if it was "/x", because it is.
  ;---------------------------------------------------*/
  
  if (buffer->buf[buffer->ridx] == ',')
  {
    earlycomma = true;
    buffer->ridx++;
  }
  
  if (parse_label(&reg,buffer,a09,pass))
  {  
    upper_label(&reg);
    vmreg = bsearch(
              &reg,
              mregisters,
              sizeof(mregisters)/sizeof(mregisters[0]),
              sizeof(mregisters[0]),
              regsearch
            );
            
    /*----------------------------------------------------------------
    ; No register, so assume an expression, and then look for a comma,
    ; and then an index register.
    ;-----------------------------------------------------------------*/
    
    if (vmreg == NULL)
    {
      struct buffer str = { .buf[0] = '\0' , .widx = 0 , .ridx = 0 };
      if (!ft_expr(prog,max,pvip,&str,a09,buffer,pass))
        return false;
      c = skip_space(buffer);
      if (c != ',')
      {
        return earlycomma
               ? message(a09,MSG_ERROR,"E0082: missing index register")
               : message(a09,MSG_ERROR,"E0023: missing expected comma")
               ;
      }
      return ft_index_register(prog,max,pvip,a09,buffer,pass);
    }
      
    /*---------------------------------------------------------------------
    ; We might have just a regiser, or the accum,index mode.  Choose here.
    ; In either case, we compile a VM_CPUaccum op here.
    ;---------------------------------------------------------------------*/
    
    prog[(*pvip)++] = vmreg->op;
    c               = skip_space(buffer);
    
    if (c == ',')
    {
      /*------------------------------------------------------------------
      ; If the label didn't specify 'A', 'B', or 'D', here, it's an error.
      ; If it's A or B, we need to sign extend it, so yet another VM op.
      ;-------------------------------------------------------------------*/
      
      switch(vmreg->op)
      {
        case VM_CPUA:
        case VM_CPUB: prog[(*pvip)++] = VM_SEX;
        case VM_CPUD: break;
        default: return message(a09,MSG_ERROR,"E0081: missing A, B, or D register");
      }
      return ft_index_register(prog,max,pvip,a09,buffer,pass);
    }
    else
      buffer->ridx--;
  }  
  else
  {
    struct buffer str = { .buf[0] = '\0' , .widx = 0 , .ridx = 0 };
    if (!ft_expr(prog,max,pvip,&str,a09,buffer,pass))
      return false;
    c = skip_space(buffer);
    if (c != ',')
    {
      return earlycomma
           ? message(a09,MSG_ERROR,"E0082: missing index register")
           : message(a09,MSG_ERROR,"E0023: missing expected comma");
           ;
    }
    return ft_index_register(prog,max,pvip,a09,buffer,pass);
  }
  
  return true;
}

/**************************************************************************/

static bool ft_value(
        enum vmops     prog[],
        size_t         max,
        size_t        *pvip,
        struct buffer *str,
        struct a09    *a09,
        struct buffer *buffer,
        int            pass
)
{
  assert(prog   != NULL);
  assert(max    == MAX_PROG);
  assert(pvip   != NULL);
  assert(*pvip  <  max);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert(pass   == 2);
  
  uint16_t v;
  bool     neg = false;
  bool     not = false;
  bool     rc  = true;
  char     c   = skip_space(buffer);
  
  if (c == '/')
    return ft_register(prog,max,pvip,a09,buffer,pass);
  else if (c == '(')
  {
    c = skip_space(buffer);
    if (c == '\0')
      return message(a09,MSG_ERROR,"E0010: unexpected end of input");
    buffer->ridx--;
    if (!ft_expr(prog,max,pvip,str,a09,buffer,pass))
      return false;
    c = skip_space(buffer);
    if (c != ')')
      return message(a09,MSG_ERROR,"E0011: missing right parenthesis");
    return true;
  }
  
  if (*pvip >= max - 2)
    return message(a09,MSG_ERROR,"E0074: not enough space for expression");
    
  if (c == '-')
  {
    neg = true;
    c = buffer->buf[buffer->ridx++];
  }
  else if (c == '~')
  {
    not = true;
    c = buffer->buf[buffer->ridx++];
  }
  else if (c == '+')
   c = buffer->buf[buffer->ridx++];
   
  if (c == '$')
    rc = s2num(a09,&v,buffer,16);
  else if (c == '&')
    rc = s2num(a09,&v,buffer,8);
  else if (c == '%')
    rc = s2num(a09,&v,buffer,2);
  else if (isdigit(c))
  {
    buffer->ridx--;
    rc = s2num(a09,&v,buffer,10);
  }
  else if ((c == '.') || (c == '.') || isalpha(c))
  {
    struct symbol *sym;
    label          label;
    
    buffer->ridx--;
    if (!parse_label(&label,buffer,a09,pass))
      return false;
    sym = symbol_find(a09,&label);
    if (sym == NULL)
      return message(a09,MSG_ERROR,"E0004: unknown symbol '%.*s'",label.s,label.text);
    v = sym->value;
    sym->refs++;
  }
  else if ((c == '"') || (c == '\''))
  {
    if (neg || not)
      return message(a09,MSG_ERROR,"E0075: can't negate or complement a string");
    if (str->widx != 0)
      return message(a09,MSG_ERROR,"E0076: string slot already filled");
    if (!collect_esc_string(a09,str,buffer,c))
      return false;
    prog[(*pvip)++] = VM_SCMP;
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0006: not a value");
    
  if (neg)
    v = -v;
  else if (not)
    v = ~v;
    
  prog[(*pvip)++] = VM_LIT;
  prog[(*pvip)++] = v;
  
  return rc;
}

/**************************************************************************/

static bool ft_factor(
        enum vmops     prog[],
        size_t         max,
        size_t        *pvip,
        struct buffer *str,
        struct a09    *a09,
        struct buffer *buffer,
        int            pass
)
{
  bool fetchbyte = false;
  bool fetchword = false;
  bool neg       = false;
  bool not       = false;
  
  assert(prog   != NULL);
  assert(max    == MAX_PROG);
  assert(pvip   != NULL);
  assert(*pvip  <  max);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert(pass   == 2);
  
  char c = skip_space(buffer);
  if ((c == '\0') || (c == ';'))
    return message(a09,MSG_ERROR,"E0010: unexpected end of input");
    
  if (c == '-')
  {
    neg = true;
    c   = skip_space(buffer);
    
  }
  else if (c == '~')
  {
    not = true;
    c   = skip_space(buffer);
  }
  
  if (c == '@')
  {
    if (buffer->buf[buffer->ridx] == '@')
    {
      buffer->ridx++;
      fetchword = true;
    }
    else
      fetchbyte = true;
    skip_space(buffer);
    buffer->ridx--;
  }
  else
    buffer->ridx--;
    
  if (!ft_value(prog,max,pvip,str,a09,buffer,pass))
    return false;
    
  if (fetchbyte)
  {
    if (*pvip == max)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
    prog[(*pvip)++] = VM_AT8;
  }
  else if (fetchword)
  {
    if (*pvip == max)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
    prog[(*pvip)++] = VM_AT16;
  }
  
  if (neg)
  {
    if (*pvip == max)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
    prog[(*pvip)++] = VM_NEG;
  }
  else if (not)
  {
    if (*pvip == max)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
    prog[(*pvip)++] = VM_NOT;
  }
  
  return true;
}

/**************************************************************************/

static bool ft_expr(
        enum vmops     prog[],
        size_t         max,
        size_t        *pvip,
        struct buffer *str,
        struct a09    *a09,
        struct buffer *buffer,
        int            pass
)
{
  assert(prog   != NULL);
  assert(max    == MAX_PROG);
  assert(pvip   != NULL);
  assert(*pvip  <  max);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert(pass   == 2);
  
  struct optable const *op;
  struct optable const *ostack[15];
  size_t                osp = sizeof(ostack) / sizeof(ostack[0]);
  
  if (!ft_factor(prog,max,pvip,str,a09,buffer,pass))
    return false;
    
  while(true)
  {
    if ((op = get_op(buffer)) == NULL)
      break;
      
    while(osp < sizeof(ostack) / sizeof(ostack[0]))
    {
      if (
               (ostack[osp]->pri >  op->pri)
           || ((ostack[osp]->pri == op->pri) && (op->pri == AS_LEFT))
         )
      {
        if (*pvip == max)
          return message(a09,MSG_ERROR,"E0066: expression too complex");
        prog[(*pvip)++] = ostack[osp++]->op;
      }
      else
        break;
    }
    
    if (osp == 0)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
    ostack[--osp] = op;
    if (*pvip == max)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
    if (!ft_factor(prog,max,pvip,str,a09,buffer,pass))
      return false;
  }
  
  while(osp < sizeof(ostack) / sizeof(ostack[0]))
  {
    if (*pvip == max)
      return message(a09,MSG_ERROR,"E0065: Internal error---expression parser mismatch");
    prog[(*pvip)++] = ostack[osp++]->op;
  }
  
  assert(osp == sizeof(ostack) / sizeof(ostack[0]));
  return true;
}

/**************************************************************************/

static bool ft_compile(
        struct a09    *a09,
        struct buffer  *restrict name,
        struct Assert  *Assert,
        struct buffer  *restrict buffer,
        int             pass
)
{
  enum vmops    program[MAX_PROG];
  struct buffer str = { .buf[0] = '\0' , .widx = 0 , .ridx = 0 };
  struct buffer tmp = { .buf[0] = '\0' , .widx = 0 , .ridx = 0 };
  size_t        vip = 0;
  char          c;
  
  assert(a09    != NULL);
  assert(name   != NULL);
  assert(Assert != NULL);
  assert(buffer != NULL);
  assert(pass   == 2);
  
  if (!ft_expr(program,sizeof(program)/sizeof(program[0]),&vip,&str,a09,buffer,pass))
    return false;
    
  if (vip == sizeof(program) / sizeof(program[0]))
    return message(a09,MSG_ERROR,"E0066: expression too complex");
    
  program[vip++] = VM_EXIT;
  struct vmcode *new = realloc(Assert->Asserts,(Assert->cnt + 1) * sizeof(struct vmcode));
  
  if (new == NULL)
    return message(a09,MSG_ERROR,"E0046: out of memory");
    
  Assert->Asserts = new;
  memcpy(Assert->Asserts[Assert->cnt].prog,program,vip * sizeof(enum vmops));
  Assert->Asserts[Assert->cnt].str = str;
  
  c = skip_space(buffer);
  if (c == ',')
  {
    c = skip_space(buffer);
    if ((c == '"') || (c == '\''))
    {
      if (!collect_esc_string(a09,&tmp,buffer,c))
        return false;
    }
  }
  
  snprintf(
       Assert->Asserts[Assert->cnt].tag,
       sizeof(Assert->Asserts[Assert->cnt].tag),
       "%.*s:%zu %.*s",
       (int)name->widx,name->buf,
       a09->lnum,
       (int)tmp.widx,tmp.buf
    );
  Assert->cnt++;
  return true;
}

/**************************************************************************/

static bool ftest_pass_start(union format *fmt,struct a09 *a09,int pass)
{
  assert(fmt != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert((pass == 1) || (pass == 2));
  (void)a09;
  (void)pass;
  
  struct format_test *test = &fmt->test;
  test->intest = false;
  return true;
}

/**************************************************************************/

static bool ftest_pass_end(union format *fmt,struct a09 *a09,int pass)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert((pass == 1) || (pass == 2));
  (void)pass;
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  
  if (test->intest)
    return message(a09,MSG_ERROR,"E0077: missing .ENDTST directive");
    
  if (pass == 2)
  {
    for (size_t i = 0 ; i < data->nunits ; i++)
    {
      struct unittest *unit = &data->units[i];
      char const      *tag  = "";
      int              rc;
      
      /*--------------------------------------------------------------------
      ; only run tests in this file.  We could get prematurely triggered if
      ; we INCLUDEd a file.
      ;---------------------------------------------------------------------*/
      
      if (unit->filename != a09->infile)
        return true;
        
      data->cpu.pc.w = unit->addr;
      data->cpu.S.w  = data->sp - 2;
      
      /*----------------------------------------------------
      ; initialize other registers with semi-random data
      ;-----------------------------------------------------*/
      
      data->cpu.U.w  = data->cpu.pc.w ^ data->cpu.S.w;
      data->cpu.Y.w  = data->cpu.U.w;
      data->cpu.X.w  = data->cpu.Y.w;
      data->cpu.d.w  = data->cpu.X.w;
      a09->lnum      = unit->line;
      message(a09,MSG_DEBUG,"Running test %s",unit->name.buf);
      
      do
      {
        if (data->memory[data->cpu.pc.w] == data->fill)
        {
          snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X",data->cpu.pc.w);
          rc = TEST_WEEDS;
          break;
        }
        
        if (data->prot[data->cpu.pc.w].tron)
        {
          char inst[128];
          char regs[128];
          
          data->dis.pc = data->cpu.pc.w;
          rc = mc6809dis_step(&data->dis,&data->cpu);
          if (rc != 0)
            break;
          mc6809dis_format(&data->dis,inst,sizeof(inst));
          mc6809dis_registers(&data->cpu,regs,sizeof(regs));
          printf("%s | %s\n",regs,inst);
        }
        
        if (data->prot[data->cpu.pc.w].check)
        {
          bool     okay = false;
          uint16_t addr = data->cpu.pc.w;
          tree__s *tree = tree_find(data->Asserts,&addr,Assertaddrcmp);
          
          if (tree != NULL)
          {
            struct Assert *Assert = tree2Assert(tree);
            
            assert(Assert->here == addr);
            for (size_t i = 0 ; i < Assert->cnt ; i++)
            {
              message(a09,MSG_DEBUG,"checking %s",Assert->Asserts[i].tag);
              okay = runvm(data->a09,&data->cpu,&Assert->Asserts[i]);
              if (!okay)
              {
                tag = Assert->Asserts[i].tag;
                break;
              }
            }
          }
          
          if (!okay)
          {
            rc = TEST_FAILED;
            break;
          }
        }
        
        rc = mc6809_step(&data->cpu);
      }
      while((rc == 0) && (data->cpu.S.w != data->sp));
      
      if (rc != 0)
      {
        static char const *const mfaults[] =
        {
          NULL,
          "an internal error inside the MC6809 emulator",
          "an illegal instruction was encountered",
          "an illegal addressing mode was encountered",
          "an undefined combination of registers was being exchanged",
          "an undefined combination of registers was being transfered",
          "test failed",
          "reading from non-readable memory",
          "code went into the weeds",
          "writing to non-writable memory",
        };
        
        assert(rc < TEST_max);
        message(a09,MSG_WARNING,"W0015: %s: %s: %s",tag,mfaults[rc],data->errbuf);
      }
    }
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_inst_write(union format *fmt,struct opcdata *opd)
{
  assert(fmt       != NULL);
  assert(opd       != NULL);
  assert(opd->pass == 2);
  assert(fmt->backend == BACKEND_TEST);
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  memcpy(&data->memory[data->addr],opd->bytes,opd->sz);
  
  for (size_t i = 0 ; i < opd->sz ; i++)
  {
    data->prot[data->addr].exec  = true;
    data->prot[data->addr].tron |= data->tron;
    data->addr++;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_data_write(
        union format   *fmt,
        struct opcdata *opd,
        char const     *buffer,
        size_t          len
)
{
  assert(fmt       != NULL);
  assert(opd       != NULL);
  assert(buffer    != NULL);
  assert(opd->pass == 2);
  assert(fmt->backend == BACKEND_TEST);
  (void)opd;
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  
  memcpy(&data->memory[data->addr],buffer,len);
  for (size_t i = 0 ; i < len ; i++)
    data->prot[data->addr++].tron |= data->tron;
  return true;
}

/**************************************************************************/

static bool ftest_align(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_TEST);
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    data->addr += opd->datasz;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_org(
        union format   *fmt,
        struct opcdata *opd,
        uint16_t        start,
        uint16_t        last
)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_TEST);
  (void)last;
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    data->addr = start;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_rmb(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_TEST);
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    
    data->addr += opd->value.value;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_test(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_TEST);
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  
  if (test->intest)
    return message(opd->a09,MSG_ERROR,"E0078: cannot nest .TEST");
  test->intest = true;
  
  if (opd->pass == 2)
  {
    char c;
    
    struct unittest *new = realloc(data->units,(data->nunits + 1) * sizeof(struct unittest));
    if (new == NULL)
      return message(opd->a09,MSG_ERROR,"E0046: out of memory");
    data->units                        = new;
    data->units[data->nunits].addr     = opd->a09->pc;
    data->units[data->nunits].filename = opd->a09->infile;
    data->units[data->nunits].line     = opd->a09->lnum;
    
    c = skip_space(opd->buffer);
    if ((c == '"') || (c == '\''))
    {
      if (!collect_esc_string(opd->a09,&data->units[data->nunits].name,opd->buffer,c))
        return false;
    }
    else if ((c == ';') || (c == '\0'))
    {
      memcpy(data->units[data->nunits].name.buf,opd->a09->label.text,opd->a09->label.s);
      data->units[data->nunits].name.widx = opd->a09->label.s;
    }
    else
      return message(opd->a09,MSG_ERROR,"E0060: syntax error");
    data->nunits++;
  }
  return true;
}

/**************************************************************************/

static bool ftest_tron(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_TEST);
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    data->tron               = true;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_troff(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_TEST);
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    data->tron               = false;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_Assert(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_TEST);
  
  if (opd->pass == 2)
  {
    struct format_test *test = &fmt->test;
    struct testdata    *data = test->data;
    
    data->prot[opd->a09->pc].check = true;
    
    struct Assert *Assert;
    tree__s       *tree = tree_find(data->Asserts,&opd->a09->pc,Assertaddrcmp);
    
    if (tree == NULL)
    {
      Assert = malloc(sizeof(struct Assert));
      if (Assert == NULL)
        return message(opd->a09,MSG_ERROR,"E0046: out of memory");
      Assert->tree.left   = NULL;
      Assert->tree.right  = NULL;
      Assert->tree.height = 0;
      Assert->here        = opd->a09->pc;
      Assert->cnt         = 0;
      Assert->Asserts     = NULL;
      data->Asserts        = tree_insert(data->Asserts,&Assert->tree,Asserttreecmp);
    }
    else
      Assert = tree2Assert(tree);
      
    if (data->nunits == 0)
    {
      struct buffer name;
      
      /*-----------------------------------------------------------------
      ; This .ASSERT directive was outside a .TEST directive, so use the
      ; last non-local label as the "name".
      ;------------------------------------------------------------------*/
      
      memcpy(name.buf,opd->a09->label.text,opd->a09->label.s);
      name.widx = opd->a09->label.s;
      if (!ft_compile(opd->a09,&name,Assert,opd->buffer,opd->pass))
        return false;
    }
    else
    {
      if (!ft_compile(opd->a09,&data->units[data->nunits-1].name,Assert,opd->buffer,opd->pass))
        return false;
    }
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_endtst(union format *fmt,struct opcdata *opd)
{
  assert(fmt != NULL);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  assert(fmt->backend == BACKEND_TEST);
  
  struct format_test *test = &fmt->test;
  
  if (!test->intest)
    return message(opd->a09,MSG_ERROR,"E0079: no matching .TEST");
    
  test->intest = false;
  return true;
}

/**************************************************************************/

static void free_Asserts(tree__s *tree)
{
  if (tree != NULL)
  {
    if (tree->left)
      free_Asserts(tree->left);
    if (tree->right)
      free_Asserts(tree->right);
    struct Assert *Assert = tree2Assert(tree);
    free(Assert->Asserts);
    free(tree);
  }
}

/**************************************************************************/

static bool ftest_fini(union format *fmt,struct a09 *a09)
{
  assert(fmt          != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(a09          != NULL);
  (void)a09;
  
  struct format_test *test = &fmt->test;
  struct testdata    *data = test->data;
  
  if (data->corefile)
  {
    FILE *fp = fopen(data->corefile,"wb");
    if (fp != NULL)
    {
      fwrite(data->memory,1,65535u,fp);
      fputc(mc6809_cctobyte(&data->cpu),fp);
      fwrite(&data->cpu.A,1,1,fp);
      fwrite(&data->cpu.B,1,1,fp);
      fwrite(&data->cpu.dp,1,1,fp);
      fwrite(&data->cpu.X.b[MSB],1,1,fp);
      fwrite(&data->cpu.X.b[LSB],1,1,fp);
      fwrite(&data->cpu.Y.b[MSB],1,1,fp);
      fwrite(&data->cpu.Y.b[LSB],1,1,fp);
      fwrite(&data->cpu.U.b[MSB],1,1,fp);
      fwrite(&data->cpu.U.b[LSB],1,1,fp);
      fwrite(&data->cpu.S.b[MSB],1,1,fp);
      fwrite(&data->cpu.S.b[LSB],1,1,fp);
      fwrite(&data->cpu.pc.b[MSB],1,1,fp);
      fwrite(&data->cpu.pc.b[LSB],1,1,fp);
      fclose(fp);
    }
    else
      message(a09,MSG_ERROR,"E0070: %s %s",test->data->corefile,strerror(errno));
  }
  
  free_Asserts(test->data->Asserts);
  free(test->data->units);
  free(test->data);
  return true;
}

/**************************************************************************/

bool format_test_init(struct format_test *fmt,struct a09 *a09)
{
  assert(fmt != NULL);
  assert(a09 != NULL);
  (void)a09;
  
  fmt->backend    = BACKEND_TEST;
  fmt->cmdline    = ftest_cmdline;
  fmt->pass_start = ftest_pass_start;
  fmt->pass_end   = ftest_pass_end;
  fmt->inst_write = ftest_inst_write;
  fmt->data_write = ftest_data_write;
  fmt->dp         = fdefault;
  fmt->code       = fdefault;
  fmt->align      = ftest_align;
  fmt->end        = fdefault_end;
  fmt->org        = ftest_org;
  fmt->rmb        = ftest_rmb;
  fmt->setdp      = fdefault;
  fmt->test       = ftest_test;
  fmt->tron       = ftest_tron;
  fmt->troff      = ftest_troff;
  fmt->Assert     = ftest_Assert;
  fmt->endtst     = ftest_endtst;
  fmt->fini       = ftest_fini;
  fmt->intest     = false;
  fmt->data       = malloc(sizeof(struct testdata));
  
  if (fmt->data != NULL)
  {
    union
    {
      struct memprot p;
      bool           b;
    } init =
    {
      .p =
      {
        .read  = true  ,
        .write = true  ,
        .exec  = false ,
        .tron  = false ,
        .check = false ,
      }
    };
    
    memset(&fmt->data->dis,0,sizeof(fmt->data->dis));
    
    fmt->data->a09       = a09;
    fmt->data->corefile  = NULL;
    fmt->data->Asserts   = NULL;
    fmt->data->units     = NULL;
    fmt->data->nunits    = 0;
    fmt->data->cpu.user  = fmt;
    fmt->data->cpu.read  = ft_cpu_read;
    fmt->data->cpu.write = ft_cpu_write;
    fmt->data->cpu.fault = ft_cpu_fault;
    fmt->data->dis.user  = fmt;
    fmt->data->dis.read  = ft_dis_read;
    fmt->data->dis.fault = ft_dis_fault;
    fmt->data->addr      = 0;
    fmt->data->sp        = 0xFFF0;
    fmt->data->fill      = 0x01; // illegal instruction
    fmt->data->tron      = false;
    fmt->data->errbuf[0] = '\0';
    
    memset(fmt->data->memory,fmt->data->fill,65536u);
    memset(fmt->data->prot,init.b,65536u);
    mc6809_reset(&fmt->data->cpu);
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0046: out of memory");
}

/**************************************************************************/
