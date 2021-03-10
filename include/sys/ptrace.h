#ifndef _SYS_PTRACE_H
#define _SYS_PTRACE_H

#define EBX 0
#define ECX 1
#define EDX 2
#define ESI 3
#define EDI 4
#define EBP 5
#define EAX 6
#define DS 7
#define ES 8
#define FS 9
#define GS 10
#define ORIG_EAX 11
#define EIP 12
#define CS  13
#define EFL 14
#define UESP 15
#define SS   16

struct pt_regs {
  long ebx;
  long ecx;
  long edx;
  long esi;
  long edi;
  long ebp;
  long eax;
  long ds;
  long es;
  long fs;
  long gs;
  long orig_eax;
  long eip;
  long cs;
  long eflags;
  long esp;
  long ss;
};

#endif