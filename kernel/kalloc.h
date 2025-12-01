// kernel/kalloc.h
#ifndef KALLOC_H
#define KALLOC_H

void kinit(void);          // initialize allocator
char* kalloc(void);        // allocate one page
void kfree(char*);         // free one page

#endif

