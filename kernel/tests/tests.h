#ifndef TESTS_H
#define TESTS_H

void test_paging(void);
void test_buddy(void);
void test_kmalloc(void);
void test_thread(void);
void test_switch(void);
void test_sched(void);
void test_block(void);
void test_vmm(void);

void run_all_tests(void);

#endif