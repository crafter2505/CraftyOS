align 4096

pml4_table:
    dq pdpt_table + 0x03
    times 511 dq 0

pdpt_table:
    dq pd_table + 0x03
    times 511 dq 0

pd_table:
%assign i 0
%rep 512
    dq (i* 0x200000) + 0x83
%assign i i+1
%endrep