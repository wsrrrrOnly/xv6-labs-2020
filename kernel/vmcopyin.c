#include "param.h"
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
pte_t *walk(pagetable_t, uint64, int);
//
// This file contains copyin_new() and copyinstr_new(), the
// replacements for copyin and coyinstr in vm.c.
//

static struct stats {
  int ncopyin;
  int ncopyinstr;
} stats;

int
statscopyin(char *buf, int sz) {
  int n;
  n = snprintf(buf, sz, "copyin: %d\n", stats.ncopyin);
  n += snprintf(buf+n, sz, "copyinstr: %d\n", stats.ncopyinstr);
  return n;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int
copyin_new(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{

  stats.ncopyin++;
  uint64 n, offset;
  char *pa;

    // 在 copyin_new 开头
  if (srcva >= MAXVA || srcva + len > MAXVA || srcva + len < srcva) {
      return -1;
  }
  while (len > 0) {
    // 找到当前页的起始虚拟地址
    uint64 va = PGROUNDDOWN(srcva);
    pte_t *pte = walk(pagetable, va, 0);
    if (!pte || (*pte & PTE_V) == 0)
      return -1;  // 页未映射或无效

    pa = (char*)PTE2PA(*pte);        // 物理地址
    offset = srcva - va;             // 页内偏移
    n = PGSIZE - offset;             // 当前页剩余字节数
    if (n > len)
      n = len;

    memmove(dst, pa + offset, n);

    dst += n;
    srcva += n;
    len -= n;
  }

  return 0;
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int
copyinstr_new(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{

  stats.ncopyinstr++;

  uint64 offset;
  char *pa;
  int tot = 0;


  if (srcva >= MAXVA || srcva >= MAXVA - max) {
    return -1;
  }
  while (tot < max) {
    uint64 va = PGROUNDDOWN(srcva);
    pte_t *pte = walk(pagetable, va, 0);
    if (!pte || (*pte & PTE_V) == 0)
      return -1;

    pa = (char*)PTE2PA(*pte);
    offset = srcva - va;

    // 如果字符串跨页，最多读到页尾
    uint64 n = PGSIZE - offset;
    if (tot + n > max)
      n = max - tot;

    for (uint64 i = 0; i < n; i++) {
      dst[tot] = pa[offset + i];
      if (dst[tot] == '\0')
        return 0;  // 成功找到 \0
      tot++;
      srcva++;
    }
  }

  return -1;  // 未找到 \0
}