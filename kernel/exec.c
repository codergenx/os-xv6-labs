#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "elf.h"


#define MAXARG 32
static int loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint64 offset, uint64 sz);
// Load an ELF program segment into pagetable at virtual address va.
// va must be page-aligned and the pages must already be mapped.
// Returns 0 on success, -1 on failure.
static int
loadseg(pagetable_t pagetable, uint64 va, struct inode *ip, uint64 offset, uint64 sz)
{
    uint64 i;
    uint64 n;
    char buf[512];

    for(i = 0; i < sz; i += sizeof(buf)){
        n = sz - i;
        if(n > sizeof(buf))
            n = sizeof(buf);
        if(readi(ip, 0, (uint64)buf, offset + i, n) != n)
            return -1;
        if(copyout(pagetable, va + i, buf, n) < 0)
            return -1;
    }
    return 0;
}

int
kexec(char *path, char **argv)
{
    struct proc *p = myproc();
    struct inode *ip;
    struct elfhdr elf;
    pagetable_t pagetable;
    uint64 sz = 0, sp;
    int argcount;

    // open executable
    if((ip = namei(path)) == 0)
        return -1;
    ilock(ip);

    if(readi(ip, 0, (uint64)&elf, 0, sizeof(elf)) != sizeof(elf)){
        iunlockput(ip);
        return -1;
    }

    if(elf.magic != ELF_MAGIC){
        iunlockput(ip);
        return -1;
    }

    if((pagetable = proc_pagetable(p)) == 0){
        iunlockput(ip);
        return -1;
    }

    // Load program segments
    struct proghdr ph;
    uint64 off;
    for(int i = 0; i < elf.phnum; i++){
        off = elf.phoff + i * sizeof(ph);
        if(readi(ip, 0, (uint64)&ph, off, sizeof(ph)) != sizeof(ph)){
            uvmfree(pagetable, 0);
            iunlockput(ip);
            return -1;
        }
        if(ph.type != ELF_PROG_LOAD)
            continue;
        if(uvmalloc(pagetable, ph.vaddr, ph.vaddr + ph.memsz, PTE_R | PTE_W | PTE_X) == 0){
            uvmfree(pagetable, 0);
            iunlockput(ip);
            return -1;
        }
        if(loadseg(pagetable, ph.vaddr, ip, ph.offset, ph.filesz) < 0){
            uvmfree(pagetable, 0);
            iunlockput(ip);
            return -1;
        }
        if(ph.vaddr + ph.memsz > sz)
            sz = ph.vaddr + ph.memsz;
    }

    iunlockput(ip);
    end_op();

    // Allocate stack
    sp = PGROUNDUP(sz);
    for(argcount = 0; argv[argcount]; argcount++){
        sp -= strlen(argv[argcount]) + 1;
        if(copyout(pagetable, sp, argv[argcount], strlen(argv[argcount]) + 1) < 0){
            uvmfree(pagetable, 0);
            return -1;
        }
    }

    sp -= sp % 16;

    // TASK 1: USYSCALL page
    char *pa = kalloc();
    if(!pa)
        panic("kexec: out of memory for USYSCALL");

    struct usyscall *us = (struct usyscall*)pa;
    us->pid = p->pid;

    uint64 phys = V2P(pa);
    if(mappages(pagetable, USYSCALL, PGSIZE, phys, PTE_R | PTE_U) < 0)
        panic("kexec: failed to map USYSCALL page");

    // Commit to process
    pagetable_t oldpagetable = p->pagetable;
    p->trapframe->sp = sp;
    p->pagetable = pagetable;
    printf("DEBUG: sp=%p, pagetable=%p, entry=%p\n",
        (void*)sp, (void*)pagetable, (void*)elf.entry);



    p->trapframe->epc = elf.entry;
    p->trapframe->sp = sp;
    proc_freepagetable(oldpagetable, 0);
    if(p->pid == 1)
    	vmprint(p->pagetable);


    return argcount;
}

