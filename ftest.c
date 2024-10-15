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
#include <time.h>

#include <mc6809.h>
#include <mc6809dis.h>

#include "a09.h"

#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wmissing-noreturn"
#  pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#if defined(__GNUC__) && __GNUC__ >= 11
#  pragma GCC diagnostic ignored "-Wenum-conversion"
#endif

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
  VM_EXP,
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
  VM_TIMEON,
  VM_TIMEOFF,
  VM_FALSE,
  VM_TRUE,
  VM_TO8,
  VM_TO16,
  VM_PROT,
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
  size_t     line;
  enum vmops prog[MAX_PROG];
  char       tag[133];
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
  unsigned long    icount;
  mc6809__t        cpu;
  mc6809dis__t     dis;
  uint16_t         addr;
  uint16_t         sp;
  uint16_t         stacksize;
  uint16_t         inittestpc;
  uint16_t         testpc;
  uint16_t         resumepc;
  uint8_t          fill;
  bool             tron;
  bool             timing;
  bool             rndorder;
  bool             intest;
  char             errbuf[128];
  mc6809byte__t    memory[65536u];
  struct memprot   prot  [65536u];
};

struct labeltable
{
  label      label;
  enum vmops op;
};

static bool ft_expr(enum vmops [],size_t,size_t *,struct testdata *,struct a09 *,struct buffer *,int);

/**************************************************************************/

static inline struct Assert *tree2Assert(tree__s *tree)
{
  assert(tree != NULL);
#if defined(__clang__)
#  pragma clang diagnostic push "-Wcast-align"
#  pragma clang diagnostic ignored "-Wcast-align"
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
        "\t-A addr\t\taddress of test code (default=0xE000)\n"
        "\t-D file\t\tcore file (of 6809 VM) name\n"
        "\t-E range\tmark memory as code (see below)\n"
        "\t-F byte\t\tfill memory with value (default=0x01, illegal inst)\n"
        "\t-R range\tmark memory read-only (see below)\n"
        "\t-S addr\t\taddress of system stack and string pool (default=0xFFF0)\n"
        "\t-T range\ttrace execution of code (see below)\n"
        "\t-W range\tmark memory write-only (see below)\n"
        "\t-Z size\t\tsize of system stack (min=2, max=4096, default=1024)\n"
        "\t-r\t\trandomize the testing order\n"
        "\n"
        "NOTE:\tmemory ranges can be specified as:\n"
        "\t\t<low-address>[ '-' <high-address>]\n"
        "\tmultiple ranges can be specified, separated by a comma\n"
        "\tBy default, memory has no permissions (addresses outside of\n"
        "\tassembled file), read/execute or read/write, but never trace\n"
        "\tby default.  You can also enable tracing with the .TRON\n"
        "\tdirective.\n"
        ;
        
/**************************************************************************/

static bool range(
        struct a09     *a09,
        struct memprot *mem,
        int            *pi,
        int             argc,
        char           *argv[],
        struct memprot  prot
)
{
  assert(mem  != NULL);
  assert(pi   != NULL);
  assert(argc >  0);
  assert(argv != NULL);
  assert(*pi  >  0);
  assert(*pi  <  argc);
  
  unsigned long int  low;
  unsigned long int  high;
  char              *r = cmd_opt(pi,argc,argv);
  
  if (r == NULL)
    return message(a09,MSG_ERROR,"E0068: missing option argument");
    
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
      return message(a09,MSG_ERROR,"E0069: address exceeds address space");
      
    for ( ; low <= high ; low++)
    {
      mem[low].read  |= prot.read;
      mem[low].write |= prot.write;
      mem[low].exec  |= prot.exec;
      mem[low].tron  |= prot.tron;
      mem[low].check |= prot.check;
    }
    
    if (*r == ',')
      r++;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_cmdline(struct format *fmt,struct a09 *a09,int argc,int *pi,char *argv[])
{
  assert(fmt  != NULL);
  assert(a09  != NULL);
  assert(argc >  0);
  assert(pi   != NULL);
  assert(*pi  >  0);
  assert(argv != NULL);
  assert(fmt->backend == BACKEND_TEST);
  
  struct testdata *data = fmt->data;
  
  switch(argv[*pi][1])
  {
    case 'S':
         if (!cmd_uint16_t(&data->sp,pi,argc,argv,0,65535u))
           return message(a09,MSG_ERROR,"E0069: address exceeds address space");
         break;
         
    case 'D':
         if ((data->corefile = cmd_opt(pi,argc,argv)) == NULL)
           return message(a09,MSG_ERROR,"E0068: missing option argument");
         break;
         
    case 'F':
         if (!cmd_uint8_t(&data->fill,pi,argc,argv,0,255))
           return message(a09,MSG_ERROR,"E0072: byte should be 0..255");
         memset(data->memory,data->fill,sizeof(data->memory));
         break;
         
    case 'R':
         if (!range(a09,data->prot,pi,argc,argv,(struct memprot){ .read = true , .write = false , .exec = false , .tron = false , .check = false }))
           return false;
         break;
         
    case 'W':
         if (!range(a09,data->prot,pi,argc,argv,(struct memprot){ .read = false , .write = true , .exec = false , .tron = false , .check = false }))
           return false;
         break;
         
    case 'E':
         if (!range(a09,data->prot,pi,argc,argv,(struct memprot){ .read = false , .write = false , .exec = true , .tron = false , .check = false }))
           return false;
         break;
         
    case 'T':
         if (!range(a09,data->prot,pi,argc,argv,(struct memprot){ .read = false , .write = false , .exec = false , .tron = true , .check = false }))
           return false;
         break;
         
    case 'Z':
         if (!cmd_uint16_t(&data->stacksize,pi,argc,argv,2,4096))
           return message(a09,MSG_ERROR,"E0088: stack size must be between 2 and 4096");
         break;
         
    case 'r':
         data->rndorder = true;
         break;
         
    case 'A':
         if (!cmd_uint16_t(&data->inittestpc,pi,argc,argv,0,65535u))
           return message(a09,MSG_ERROR,"E0069: address exceeds address space");
          break;
          
    default:
         usage(argv[0]);
         return false;
  }
  
  return true;
}

/**************************************************************************/

static struct vmcode *new_program(struct Assert *Assert)
{
  assert(Assert != NULL);
  
  struct vmcode *new = realloc(Assert->Asserts,(Assert->cnt + 1) * sizeof(struct vmcode));
  if (new != NULL)
  {
    Assert->Asserts = new;
    new             = &Assert->Asserts[Assert->cnt++];
    new->line       = 0;
    new->prog[0]    = VM_EXIT;
    new->tag[0]     = '\0';
  }
  
  return new;
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

static struct Assert *get_Assert(struct a09 *a09,struct testdata *data,uint16_t here)
{
  assert(a09  != NULL);
  assert(data != NULL);
  
  struct Assert *Assert;
  tree__s       *tree = tree_find(data->Asserts,&here,Assertaddrcmp);
  
  if (tree == NULL)
  {
    Assert = malloc(sizeof(struct Assert));
    if (Assert == NULL)
    {
      message(a09,MSG_ERROR,"E0046: out of memory");
      return NULL;
    }
    
    Assert->tree.left   = NULL;
    Assert->tree.right  = NULL;
    Assert->tree.height = 0;
    Assert->here        = here;
    Assert->cnt         = 0;
    Assert->Asserts     = NULL;
    data->Asserts       = tree_insert(data->Asserts,&Assert->tree,Asserttreecmp);
  }
  else
    Assert = tree2Assert(tree);
    
  return Assert;
}

/**************************************************************************/

static bool runvm(struct a09 *a09,mc6809__t *cpu,struct vmcode *test)
{
  assert(a09  != NULL);
  assert(cpu  != NULL);
  assert(test != NULL);
  
  struct testdata *data = cpu->user;
  struct memprot   prot;
  uint16_t         stack[15];
  uint16_t         result;
  uint16_t         value;
  uint16_t         addr;
  size_t           sp = sizeof(stack) / sizeof(stack[0]);
  size_t           ip = 0;
  int              rc;
  
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
           
      case VM_EXP:
           if (stack[sp] > 0x7FFF)
             return message(a09,MSG_ERROR,"E0091: negative exponents for integers not supported");
           else if (stack[sp] == 0)
             stack[++sp] = 1;
           else
           {
             while(--stack[sp])
               stack[sp+1] = stack[sp+1] * stack[sp+1];
             result = stack[sp + 1];
             stack[++sp] = result;
           }
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
           {
             uint16_t len = stack[sp++];
             uint16_t dst = stack[sp++];
             uint16_t src = stack[sp++];
             rc = memcmp(&data->memory[src],&data->memory[dst],len);
             stack[--sp] = 0;
             if (rc < 0)
               stack[--sp] = -1;
             else if (rc > 0)
               stack[--sp] =  1;
             else
               stack[--sp] =  0;
           }
           break;
           
      case VM_SEX:
           if (stack[sp] >= 0x80)
             stack[sp] |= 0xFF00;
           break;
           
      case VM_TIMEON:
           cpu->cycles  = 0;
           data->icount = 0;
           break;
           
      case VM_TIMEOFF:
           printf("%s: cycles=%lu instructions=%lu cpi=%.2f\n",test->tag,cpu->cycles,data->icount,(double)cpu->cycles/(double)data->icount);
           break;
           
      case VM_FALSE:
           stack[--sp] = false;
           break;
           
      case VM_TRUE:
           stack[--sp] = true;
           break;
           
      case VM_TO8:
           addr               = stack[sp++];
           value              = stack[sp++];
           data->memory[addr] = value & 255;
           break;
           
      case VM_TO16:
           addr                   = stack[sp++];
           value                  = stack[sp++];
           data->memory[addr]     = value >> 8;
           data->memory[addr + 1] = value & 255;
           break;
           
      case VM_PROT:
           assert(sizeof(struct memprot) <= sizeof(enum vmops));
           
           addr  = stack[sp++];
           value = stack[sp++];
           memcpy(&prot,&stack[sp++],sizeof(prot));
           
           for (size_t a = addr ; a <= value ; a++)
             data->prot[a] = prot;
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
  
  struct testdata *data = cpu->user;
  
  if (!data->prot[addr].read)
  {
    snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X addr=%04X",cpu->instpc,addr);
    longjmp(cpu->err,TEST_NON_READ_MEM);
  }
  
  if (inst && !data->prot[addr].exec)
  {
    snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X addr=%04X",cpu->instpc,addr);
    longjmp(cpu->err,TEST_WEEDS);
  }
  
  return data->memory[addr];
}

/**************************************************************************/

static void ft_cpu_write(mc6809__t *cpu,mc6809addr__t addr,mc6809byte__t byte)
{
  assert(cpu       != NULL);
  assert(cpu->user != NULL);
  
  struct testdata *data = cpu->user;
  
  if (!data->prot[addr].write)
  {
    snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X addr=%04X",cpu->instpc,addr);
    longjmp(cpu->err,TEST_NON_WRITE_MEM);
  }
  if (data->prot[addr].exec)
    message(data->a09,MSG_WARNING,"W0014: possible self-modifying code @ %04X",cpu->instpc);
  if (data->prot[addr].tron)
    message(data->a09,MSG_WARNING,"W0016: memory write of %02X to %04X @ %04X",byte,addr,cpu->instpc);
  data->memory[addr] = byte;
}

/**************************************************************************/

static void ft_cpu_fault(mc6809__t *cpu,mc6809fault__t fault)
{
  assert(cpu       != NULL);
  assert(cpu->user != NULL);
  
  struct testdata *data = cpu->user;
  
  snprintf(data->errbuf,sizeof(data->errbuf),"PC=%04X",cpu->instpc);
  longjmp(cpu->err,fault);
}

/**************************************************************************/

static mc6809byte__t ft_dis_read(mc6809dis__t *dis,mc6809addr__t addr)
{
  assert(dis       != NULL);
  assert(dis->user != NULL);
  
  struct testdata *data = dis->user;
  
  return data->memory[addr];
}

/**************************************************************************/

static void ft_dis_fault(mc6809dis__t *dis,mc6809fault__t fault)
{
  assert(dis != NULL);
  longjmp(dis->err,fault);
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
        enum vmops     prog[static MAX_PROG],
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
  
  if (*pvip == max)
    return message(a09,MSG_ERROR,"E0066: expression too complex");
    
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
        enum vmops       prog[static MAX_PROG],
        size_t           max,
        size_t          *pvip,
        struct testdata *data,
        struct a09      *a09,
        struct buffer   *buffer,
        int              pass
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
      if (!ft_expr(prog,max,pvip,data,a09,buffer,pass))
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
    
    if (*pvip == max)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
      
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
        case VM_CPUB:
             if (*pvip == max)
               return message(a09,MSG_ERROR,"E0066: expression too complex");
             prog[(*pvip)++] = VM_SEX;
             break;
        case VM_CPUD:
             break;
        default: return message(a09,MSG_ERROR,"E0081: missing A, B, or D register");
      }
      return ft_index_register(prog,max,pvip,a09,buffer,pass);
    }
    else
      buffer->ridx--;
  }
  else
  {
    if (!ft_expr(prog,max,pvip,data,a09,buffer,pass))
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
  
  return true;
}

/**************************************************************************/

static bool ft_value(
        enum vmops       prog[static MAX_PROG],
        size_t           max,
        size_t          *pvip,
        struct testdata *data,
        struct a09      *a09,
        struct buffer   *buffer,
        int              pass
)
{
  assert(prog   != NULL);
  assert(max    == MAX_PROG);
  assert(pvip   != NULL);
  assert(*pvip  <  max);
  assert(data   != NULL);
  assert(a09    != NULL);
  assert(buffer != NULL);
  assert(pass   == 2);
  
  uint16_t v;
  bool     neg = false;
  bool     not = false;
  bool     rc  = true;
  char     c   = skip_space(buffer);
  
  if (c == '/')
    return ft_register(prog,max,pvip,data,a09,buffer,pass);
    
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
    struct buffer text;
    
    if (neg || not)
      return message(a09,MSG_ERROR,"E0075: can't negate or complement a string");
    if (!collect_esc_string(a09,&text,buffer,c))
      return false;
    data->sp               -= text.widx;
    memcpy(&data->memory[data->sp],text.buf,text.widx);
    if (*pvip + 5 == max)
      return message(a09,MSG_ERROR,"E0066: expression too complex");
      
    prog[(*pvip)++] = VM_LIT;
    prog[(*pvip)++] = data->sp;
    prog[(*pvip)++] = VM_LIT;
    prog[(*pvip)++] = text.widx;
    prog[(*pvip)++] = VM_SCMP;
    return true;
  }
  else if (c == '?')
    v  = data->fill;
  else
    return message(a09,MSG_ERROR,"E0006: not a value");
    
  if (neg)
    v = -v;
  else if (not)
    v = ~v;
    
  if (*pvip >= max - 2)
    return message(a09,MSG_ERROR,"E0066: expression too complex");
    
  prog[(*pvip)++] = VM_LIT;
  prog[(*pvip)++] = v;
  
  return rc;
}

/**************************************************************************/

static bool ft_factor(
        enum vmops       prog[static MAX_PROG],
        size_t           max,
        size_t          *pvip,
        struct testdata *data,
        struct a09      *a09,
        struct buffer   *buffer,
        int              pass
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
  else if (c == '+')
    c = buffer->buf[buffer->ridx]++;
    
  if (c == '@')
  {
    if (buffer->buf[buffer->ridx] == '@')
    {
      c = buffer->buf[buffer->ridx++];
      fetchword = true;
    }
    else
      fetchbyte = true;
    c = skip_space(buffer);
  }
  
  if (c == '(')
  {
    c = skip_space(buffer);
    if (c == '\0')
      return message(a09,MSG_ERROR,"E0010: unexpected end of input");
    buffer->ridx--;
    if (!ft_expr(prog,max,pvip,data,a09,buffer,pass))
      return false;
    c = skip_space(buffer);
    if (c != ')')
      return message(a09,MSG_ERROR,"E0011: missing right parenthesis");
  }
  else
  {
    buffer->ridx--;
    if (!ft_value(prog,max,pvip,data,a09,buffer,pass))
      return false;
  }
  
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
        enum vmops       prog[],
        size_t           max,
        size_t          *pvip,
        struct testdata *data,
        struct a09      *a09,
        struct buffer   *buffer,
        int              pass
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
  
  if (!ft_factor(prog,max,pvip,data,a09,buffer,pass))
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
    if (!ft_factor(prog,max,pvip,data,a09,buffer,pass))
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
        struct a09      *a09,
        struct buffer   *restrict name,
        struct testdata *data,
        struct Assert   *Assert,
        struct buffer   *restrict buffer,
        int              pass
)
{
  enum vmops    program[MAX_PROG];
  struct buffer tmp = { .buf[0] = '\0' , .widx = 0 , .ridx = 0 };
  size_t        vip = 0;
  char          c;
  
  assert(a09    != NULL);
  assert(name   != NULL);
  assert(Assert != NULL);
  assert(buffer != NULL);
  assert(pass   == 2);
  
  if (!ft_expr(program,sizeof(program)/sizeof(program[0]),&vip,data,a09,buffer,pass))
    return false;
    
  if (vip == sizeof(program) / sizeof(program[0]))
    return message(a09,MSG_ERROR,"E0066: expression too complex");
    
  program[vip++] = VM_EXIT;
  struct vmcode *new = new_program(Assert);
  
  if (new == NULL)
    return message(a09,MSG_ERROR,"E0046: out of memory");
    
  memcpy(new->prog,program,vip * sizeof(enum vmops));
  
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
       new->tag,
       sizeof(new->tag),
       "%.*s:%zu %.*s",
       (int)name->widx,name->buf,
       a09->lnum,
       (int)tmp.widx,tmp.buf
    );
  return true;
}

/**************************************************************************/

static bool ftest_pass_start(struct format *fmt,struct a09 *a09,int pass)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert((pass == 1) || (pass == 2));
  (void)a09;
  (void)pass;
  
  struct testdata *test = fmt->data;
  test->intest          = false;
  test->testpc          = test->inittestpc;
  test->resumepc        = 0x0000;
  if (pass == 1)
    a09->outfile = "/dev/null";
  return true;
}

/**************************************************************************/

static bool ftest_pass_end(struct format *fmt,struct a09 *a09,int pass)
{
  assert(fmt         != NULL);
  assert(fmt->data   != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(a09          != NULL);
  assert((pass == 1) || (pass == 2));
  (void)pass;
  
  struct testdata *data = fmt->data;
  
  if (data->intest)
    return message(a09,MSG_ERROR,"E0077: missing .ENDTST directive");
    
  if (pass == 2)
  {
    /*-----------------------------------------------------------------------
    ; A simple way to randomize the tests array, based upon:
    ; https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle
    ; We check if we have more than one test; 0 or 1 tests, there's no need
    ; for this step at all.
    ;------------------------------------------------------------------------*/
    
    if ((data->rndorder) && (data->nunits > 1))
    {
      message(a09,MSG_DEBUG,"Randomizing tests");
      srand(time(NULL)); /* XXX is there a better way? */
      for (size_t i = data->nunits ; i > 1 ; i--)
      {
        size_t j = rand() % i;
        if (j != i - 1)
        {
          struct unittest tmp = data->units[i-1];
          data->units[i-1]    = data->units[j];
          data->units[j]      = tmp;
        }
      }
    }
    
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
        
      for (size_t j = 0 ; j < data->stacksize ; j++)
      {
        data->prot[data->sp - j].read  = true;
        data->prot[data->sp - j].write = true;
      }
      
      data->cpu.pc.w = unit->addr;
      data->cpu.S.w  = data->sp - 2;
      data->cpu.dp   = a09->dp;
      
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
            for (size_t j = 0 ; j < Assert->cnt ; j++)
            {
              message(a09,MSG_DEBUG,"checking %s",Assert->Asserts[j].tag);
              okay = runvm(data->a09,&data->cpu,&Assert->Asserts[j]);
              if (!okay)
              {
                tag = Assert->Asserts[j].tag;
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
        
        data->icount++;
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

static bool ftest_write(
        struct format   *fmt,
        struct opcdata *opd,
        void const     *buffer,
        size_t          len,
        bool            instruction
)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd          != NULL);
  assert(buffer       != NULL);
  assert(opd->pass    == 2);
  
  (void)opd;
  
  struct testdata *data = fmt->data;
  
  memcpy(&data->memory[data->addr],buffer,len);
  for (size_t i = 0 ; i < len ; i++)
  {
    data->prot[data->addr].read   = true;
    data->prot[data->addr].write  = !instruction;
    data->prot[data->addr].exec   = instruction;
    data->prot[data->addr].tron |= data->tron;
    data->addr++;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_opt(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct testdata *data = fmt->data;
    char             c    = skip_space(opd->buffer);
    label            tmp;
    
    read_label(opd->buffer,&tmp,c);
    upper_label(&tmp);
    if ((tmp.s != 4) || (memcmp(tmp.text,"TEST",4) != 0)) /* not for us, ignore */
      return true;
      
    c = skip_space(opd->buffer);
    read_label(opd->buffer,&tmp,c);
    upper_label(&tmp);
    
    if ((tmp.s == 4) && (memcmp(tmp.text,"PROT",4) == 0))
    {
      struct memprot prot =
      {
        .read  = false ,
        .write = false ,
        .exec  = false ,
        .tron  = false ,
        .check = false ,
      };
      struct value low;
      struct value high;
      
      c = skip_space(opd->buffer);
      read_label(opd->buffer,&tmp,c);
      
      for (size_t i = 0 ; i < tmp.s ; i++)
      {
        switch(toupper(tmp.text[i]))
        {
          case 'R': prot.read  = true; break;
          case 'W': prot.write = true; break;
          case 'X': prot.exec  = true; break;
          case 'T': prot.tron  = true; break;
          case 'N':                    break;
          default: return message(opd->a09,MSG_ERROR,"E0084: undefined protection bit '%c'",c);
        }
      }
      
      c = skip_space(opd->buffer);
      if (c != ',')
        return message(opd->a09,MSG_ERROR,"E0023: missing expected comma");
        
      if (!expr(&low,opd->a09,opd->buffer,opd->pass))
        return false;
      c = skip_space(opd->buffer);
      if ((c == ';') || (c == '\0'))
        high = low;
      else if (c != ',')
        return message(opd->a09,MSG_ERROR,"E0023: missing expected comma");
      else
      {
        if (!expr(&high,opd->a09,opd->buffer,opd->pass))
          return false;
      }
      
      message(opd->a09,MSG_DEBUG,
        "LOW=%04X HIGH=%04X r=%s w=%s x=%s t=%s",
        low.value,
        high.value,
        prot.read  ? "true " : "false",
        prot.write ? "true " : "false",
        prot.exec  ? "true " : "false",
        prot.tron  ? "true " : "false"
      );
      
      if (data->intest)
      {
        struct Assert *Assert = get_Assert(opd->a09,data,opd->a09->pc);
        if (Assert == NULL)
          return false;
          
        struct vmcode *new = new_program(Assert);
        if (new == NULL)
          return false;
          
        assert(sizeof(struct memprot) <= sizeof(enum vmops));
        
        new->prog[0]                   = VM_LIT;
        memcpy(&new->prog[1],&prot,sizeof(prot));
        new->prog[2]                   = VM_LIT;
        new->prog[3]                   = high.value;
        new->prog[4]                   = VM_LIT;
        new->prog[5]                   = low.value;
        new->prog[6]                   = VM_PROT;
        new->prog[7]                   = VM_TRUE;
        new->prog[8]                   = VM_EXIT;
        data->prot[opd->a09->pc].check = true;
      }
      else
      {
        for (size_t a = low.value ; a <= high.value ; a++)
          data->prot[a] = prot;
      }
    }
    
    else if ((tmp.s == 5) && (memcmp(tmp.text,"POKEW",5) == 0))
    {
      struct value addr;
      struct value word;
      
      if (!expr(&addr,opd->a09,opd->buffer,opd->pass))
        return false;
      c = skip_space(opd->buffer);
      if (c != ',')
        return message(opd->a09,MSG_ERROR,"E0023: missing expected comma");
      if (!expr(&word,opd->a09,opd->buffer,opd->pass))
        return false;
        
      if (data->intest)
      {
        struct Assert *Assert = get_Assert(opd->a09,data,opd->a09->pc);
        if (Assert == NULL)
          return false;
          
        struct vmcode *new = new_program(Assert);
        if (new == NULL)
          return false;
          
        new->prog[0]                   = VM_LIT;
        new->prog[1]                   = word.value;
        new->prog[2]                   = VM_LIT;
        new->prog[3]                   = addr.value;
        new->prog[4]                   = VM_TO16;
        new->prog[5]                   = VM_TRUE;
        new->prog[6]                   = VM_EXIT;
        data->prot[opd->a09->pc].check = true;
      }
      else
      {
        data->memory[addr.value]     = word.value >> 8;
        data->memory[addr.value + 1] = word.value & 255;
      }
    }
    
    else if ((tmp.s == 4) && (memcmp(tmp.text,"POKE",4) == 0))
    {
      struct value addr;
      struct value byte;
      
      if (!expr(&addr,opd->a09,opd->buffer,opd->pass))
        return false;
      c = skip_space(opd->buffer);
      if (c != ',')
        return message(opd->a09,MSG_ERROR,"E0023: missing expected comma");
      if (!expr(&byte,opd->a09,opd->buffer,opd->pass))
        return false;
        
      if (data->intest)
      {
        struct Assert *Assert = get_Assert(opd->a09,data,opd->a09->pc);
        if (Assert == NULL)
          return false;
          
        struct vmcode *new = new_program(Assert);
        if (new == NULL)
          return false;
          
        new->prog[0]                   = VM_LIT;
        new->prog[1]                   = byte.value;
        new->prog[2]                   = VM_LIT;
        new->prog[3]                   = addr.value;
        new->prog[4]                   = VM_TO8;
        new->prog[5]                   = VM_TRUE;
        new->prog[6]                   = VM_EXIT;
        data->prot[opd->a09->pc].check = true;
      }
      else
        data->memory[addr.value] = byte.value & 255;
    }
    
    else if ((tmp.s == 5) && (memcmp(tmp.text,"STACK",5) == 0))
    {
      struct value sp;
      
      if (!expr(&sp,opd->a09,opd->buffer,opd->pass))
        return false;
        
      if (data->intest)
        message(opd->a09,MSG_WARNING,"W0017: cannot assign the stack address within .TEST directive");
        
      data->sp = sp.value;
    }
    
    else if ((tmp.s == 9) && (memcmp(tmp.text,"STACKSIZE",9) == 0))
    {
      struct value size;
      
      if (!expr(&size,opd->a09,opd->buffer,opd->pass))
        return false;
        
      if (data->intest)
        message(opd->a09,MSG_WARNING,"W0018: cannot assign the stack size within .TEST directive");
        
      data->stacksize = size.value;
    }
    
    else if ((tmp.s == 9) && (memcmp(tmp.text,"RANDOMIZE",9) == 0))
    {
      if (data->intest)
        return message(opd->a09,MSG_ERROR,"E0089: can only set outside a .TEST directive");
      data->rndorder = true;
    }
    
    else if ((tmp.s == 9) && (memcmp(tmp.text,"LOADTESTS",9) == 0))
    {
      struct value addr;
      
      if (!expr(&addr,opd->a09,opd->buffer,opd->pass))
        return false;
        
      if (data->intest)
        return message(opd->a09,MSG_ERROR,"E0100: Can only assign test code outside a .TEST directive");
      
      data->inittestpc = addr.value;
    }
    
    else
      return message(opd->a09,MSG_ERROR,"E0087: option '%.*s' not supported",tmp.s,tmp.text);
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_align(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct testdata *data = fmt->data;
    data->addr += opd->datasz;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_org(
        struct format   *fmt,
        struct opcdata *opd
)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct testdata *data = fmt->data;
    data->addr            = opd->value.value;
  }
  
  opd->a09->pc = opd->value.value;
  return true;
}

/**************************************************************************/

static bool ftest_rmb(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct testdata *data = fmt->data;
    
    if (opd->value.value == 0)
      return message(opd->a09,MSG_ERROR,"E0099: Can't reserve 0 bytes of memory");
      
    for (size_t i = 0 ; i < opd->value.value ; i++)
    {
      data->prot[data->addr + i].read  = true;
      data->prot[data->addr + i].write = true;
    }
    
    data->addr += opd->value.value;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_test(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  struct testdata *data = fmt->data;
  
  if (data->intest)
    return message(opd->a09,MSG_ERROR,"E0078: cannot nest .TEST");
  data->intest = true;
  
  data->resumepc   = opd->a09->pc;
  opd->value.value = data->testpc;
  if (!ftest_org(fmt,opd))
    return false;
    
  message(opd->a09,MSG_DEBUG,"test resume=%04X new=%04X",data->resumepc,data->testpc);
  
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

static bool ft_timingp(struct buffer *buffer)
{
  assert(buffer != NULL);
  char c = skip_space(buffer);
  if ((c == ';') || (c == '\0'))
    return false;
  buffer->ridx--;
  return strncmp(buffer->buf,"timing",6);
}

/**************************************************************************/

static bool ftest_tron(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct testdata *data = fmt->data;
    
    if (ft_timingp(opd->buffer))
    {
      struct vmcode *new;
      struct Assert *Assert = get_Assert(opd->a09,data,opd->a09->pc);
      
      if (Assert == NULL)
        return false;
      new = new_program(Assert);
      if (new == NULL)
        return message(opd->a09,MSG_ERROR,"E0046: out of memory");
        
      if (data->nunits == 0)
      {
        snprintf(
                  new->tag,
                  sizeof(new->tag),
                  "%s:%zu",
                  opd->a09->infile,
                  opd->a09->lnum
                );
      }
      else
      {
        snprintf(
                  new->tag,
                  sizeof(new->tag),
                  "%.*s:%zu",
                  (int)data->units[data->nunits-1].name.widx,
                  data->units[data->nunits-1].name.buf,
                  opd->a09->lnum
                );
      }
      
      new->line                      = opd->a09->lnum;
      new->prog[0]                   = VM_TIMEON;
      new->prog[1]                   = VM_TRUE;
      new->prog[2]                   = VM_EXIT;
      data->prot[opd->a09->pc].check = true;
      data->timing                   = true;
    }
    else
      data->tron = true;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_troff(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd          != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct testdata *data = fmt->data;
    
    if (data->timing || ft_timingp(opd->buffer))
    {
      struct vmcode *new;
      struct Assert *Assert = get_Assert(opd->a09,data,opd->a09->pc);
      if (Assert == NULL)
        return false;
        
      new = new_program(Assert);
      if (new == NULL)
        return message(opd->a09,MSG_ERROR,"E0046: out of memory");
        
      if (data->nunits == 0)
      {
        snprintf(
                  new->tag,
                  sizeof(new->tag),
                  "%s:%zu",
                  opd->a09->infile,
                  opd->a09->lnum
                );
      }
      else
      {
        snprintf(
                  new->tag,
                  sizeof(new->tag),
                  "%.*s:%zu",
                  (int)data->units[data->nunits-1].name.widx,
                  data->units[data->nunits-1].name.buf,
                  opd->a09->lnum
                );
      }
      
      new->line                      = opd->a09->lnum;
      new->prog[0]                   = VM_TIMEOFF;
      new->prog[1]                   = VM_TRUE;
      new->prog[2]                   = VM_EXIT;
      data->prot[opd->a09->pc].check = true;
    }
    else
      data->tron = false;
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_Assert(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  if (opd->pass == 2)
  {
    struct testdata    *data       = fmt->data;
    data->prot[opd->a09->pc].check = true;
    struct Assert *Assert          = get_Assert(opd->a09,data,opd->a09->pc);
    
    if (Assert == NULL)
      return false;
      
    if (data->nunits == 0)
    {
      struct buffer name;
      
      /*-----------------------------------------------------------------
      ; This .ASSERT directive was outside a .TEST directive, so use the
      ; last non-local label as the "name".
      ;------------------------------------------------------------------*/
      
      memcpy(name.buf,opd->a09->label.text,opd->a09->label.s);
      name.widx = opd->a09->label.s;
      if (!ft_compile(opd->a09,&name,data,Assert,opd->buffer,opd->pass))
        return false;
    }
    else
    {
      if (!ft_compile(opd->a09,&data->units[data->nunits-1].name,data,Assert,opd->buffer,opd->pass))
        return false;
    }
  }
  
  return true;
}

/**************************************************************************/

static bool ftest_endtst(struct format *fmt,struct opcdata *opd)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(opd != NULL);
  assert((opd->pass == 1) || (opd->pass == 2));
  
  struct testdata *test = fmt->data;
  
  if (!test->intest)
    return message(opd->a09,MSG_ERROR,"E0079: no matching .TEST");
    
  test->testpc     = opd->a09->pc;
  opd->value.value = test->resumepc;
  if (!ftest_org(fmt,opd))
    return false;
  message(opd->a09,MSG_DEBUG,"endtst pc=%04X",opd->a09->pc);
  
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

static bool ftest_fini(struct format *fmt,struct a09 *a09)
{
  assert(fmt          != NULL);
  assert(fmt->data    != NULL);
  assert(fmt->backend == BACKEND_TEST);
  assert(a09          != NULL);
  (void)a09;
  
  struct testdata *data = fmt->data;
  
  if (data->corefile)
  {
    FILE *fp = fopen(data->corefile,"wb");
    if (fp != NULL)
    {
      fwrite(data->memory,1,65536u,fp);
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
      message(a09,MSG_ERROR,"E0070: %s: %s",data->corefile,strerror(errno));
  }
  
  free_Asserts(data->Asserts);
  free(data->units);
  free(data);
  return true;
}

/**************************************************************************/

bool format_test_init(struct a09 *a09)
{
  static struct format const callbacks =
  {
    .backend    = BACKEND_TEST,
    .cmdline    = ftest_cmdline,
    .pass_start = ftest_pass_start,
    .pass_end   = ftest_pass_end,
    .write      = ftest_write,
    .opt        = ftest_opt,
    .dp         = fdefault,
    .code       = fdefault,
    .align      = ftest_align,
    .end        = fdefault_end,
    .org        = ftest_org,
    .rmb        = ftest_rmb,
    .setdp      = fdefault,
    .test       = ftest_test,
    .tron       = ftest_tron,
    .troff      = ftest_troff,
    .Assert     = ftest_Assert,
    .endtst     = ftest_endtst,
    .Float      = freal_ieee,
    .fini       = ftest_fini,
    .data       = NULL,
  };
  
  /*------------------------------------------------------------------------
  ; C99 doesn't have static asserts, so this will have to do for now.  This
  ; will at least double check out assumptions about the values used.
  ;-------------------------------------------------------------------------*/
  
  assert((int)VM_LOR  == (int)OP_LOR);
  assert((int)VM_LAND == (int)OP_LAND);
  assert((int)VM_GT   == (int)OP_GT);
  assert((int)VM_GE   == (int)OP_GE);
  assert((int)VM_EQ   == (int)OP_EQ);
  assert((int)VM_LE   == (int)OP_LE);
  assert((int)VM_LT   == (int)OP_LT);
  assert((int)VM_NE   == (int)OP_NE);
  assert((int)VM_BOR  == (int)OP_BOR);
  assert((int)VM_BEOR == (int)OP_BEOR);
  assert((int)VM_BAND == (int)OP_BAND);
  assert((int)VM_SHR  == (int)OP_SHR);
  assert((int)VM_SHL  == (int)OP_SHL);
  assert((int)VM_SUB  == (int)OP_SUB);
  assert((int)VM_ADD  == (int)OP_ADD);
  assert((int)VM_MUL  == (int)OP_MUL);
  assert((int)VM_DIV  == (int)OP_DIV);
  assert((int)VM_MOD  == (int)OP_MOD);
  assert((int)VM_EXP  == (int)OP_EXP);
  
  assert(a09 != NULL);
  
  struct testdata *data = malloc(sizeof(struct testdata));
  if (data != NULL)
  {
    data->a09        = a09;
    data->corefile   = NULL;
    data->Asserts    = NULL;
    data->units      = NULL;
    data->nunits     = 0;
    data->icount     = 0;
    data->cpu.user   = data;
    data->cpu.read   = ft_cpu_read;
    data->cpu.write  = ft_cpu_write;
    data->cpu.fault  = ft_cpu_fault;
    data->dis.user   = data;
    data->dis.read   = ft_dis_read;
    data->dis.fault  = ft_dis_fault;
    data->addr       = 0;
    data->sp         = 0xFFF0;
    data->stacksize = 1024;
    data->inittestpc = 0xE000;
    data->testpc     = 0xE000;
    data->resumepc   = 0x0000;
    data->fill       = 0x01; // illegal instruction
    data->tron       = false;
    data->timing     = false;
    data->rndorder   = false;
    data->errbuf[0]  = '\0';
    data->intest     = false;
    
    memset(data->memory,data->fill,sizeof(data->memory));
    memset(data->prot,0,sizeof(data->prot));
    for (size_t addr = MC6809_VECTOR_SWI3 ; addr <= MC6809_VECTOR_RESET + 1 ; addr++)
      data->prot[addr].read = true;
    mc6809_reset(&data->cpu);
    
    a09->format      = callbacks;
    a09->format.data = data;
    return true;
  }
  else
    return message(a09,MSG_ERROR,"E0046: out of memory");
}

/**************************************************************************/
