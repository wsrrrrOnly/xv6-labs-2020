#include "types.h"        // 基础类型：uint64, uchar 等
#include "param.h"        // NVMA, NPROC, NOFILE, **NDIRECT?**
#include "fs.h"           // ← 关键！NDIRECT 定义在这里（不是 param.h！）
#include "riscv.h"        // pagetable_t, PGSIZE, PTE_*, PGROUNDDOWN
#include "memlayout.h"    // 可选，但建议保留
#include "spinlock.h"     // struct spinlock
#include "sleeplock.h"    // ← 关键！必须在 file.h 之前
#include "file.h"         // struct file（依赖 sleeplock 和 NDIRECT）
#include "proc.h"         // struct proc（依赖 file.h 如果用了 struct file*）
#include "fcntl.h"        // PROT_READ/WRITE/EXEC
#include "defs.h"         // 函数声明（最后包含）

int
mmap_handler(uint64 va, int cause)
{
  struct proc *p = myproc();
  int i;

  // 查找包含 va 的 VMA
  for(i = 0; i < NVMA; i++) {
    if(p->vma[i].used &&
       va >= p->vma[i].addr &&
       va < p->vma[i].addr + p->vma[i].len) {
      break;
    }
  }
  if(i == NVMA)
    return -1;  // 不属于任何 mmap 区域

  // 权限检查
  struct file *f = p->vma[i].vfile;
  if(cause == 15 && !(p->vma[i].prot & PROT_WRITE))
    return -1;
  if(cause == 13 && !(p->vma[i].prot & PROT_READ))
    return -1;

  // 分配物理页
  char *pa = kalloc();
  if(pa == 0)
    return -1;
  memset(pa, 0, PGSIZE);

  // 从文件读取数据
  ilock(f->ip);
  uint64 offset = p->vma[i].offset + (PGROUNDDOWN(va) - p->vma[i].addr);
  int n = readi(f->ip, 0, (uint64)pa, offset, PGSIZE);
  iunlock(f->ip);

  if(n < 0) {
    kfree(pa);
    return -1;
  }

  // 设置 PTE 权限
  int perm = PTE_U;
  if(p->vma[i].prot & PROT_READ) perm |= PTE_R;
  if(p->vma[i].prot & PROT_WRITE) perm |= PTE_W;
  if(p->vma[i].prot & PROT_EXEC) perm |= PTE_X;

  // 映射到页表
  if(mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)pa, perm) < 0) {
    kfree(pa);
    return -1;
  }

  return 0;
}