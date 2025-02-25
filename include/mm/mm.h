#ifndef __MM_H__
#define __MM_H__
#include "std/stddef.h"

void mem_init();
struct page * alloc_pages(uint32 flags, const int order);
struct page * alloc_page(uint32 flags);
void free_pages(struct page *pages, const int order);
void free_page(struct page *page);


void * __alloc_pages(uint32 flags, const int order);
void * __alloc_page(uint32 flags);
void __free_pages(void* addr, const int order);
void __free_page(void* addr);


#endif