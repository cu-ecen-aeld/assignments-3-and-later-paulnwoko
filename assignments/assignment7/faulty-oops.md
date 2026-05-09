
Ran ```echo “hello_world” > /dev/faulty```   
Result:
```
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
  ESR = 0x0000000096000045
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0
  EA = 0, S1PTW = 0
  FSC = 0x05: level 1 translation fault
Data abort info:
  ISV = 0, ISS = 0x00000045
  CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=0000000041bc8000
[0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 [#2] SMP
Modules linked in: scull(O) faulty(O) hello(O) [last unloaded: scull(O)]
CPU: 0 PID: 150 Comm: sh Tainted: G      D    O       6.1.44 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 [faulty]
lr : vfs_write+0xc8/0x390
sp : ffffffc008cf3d20
x29: ffffffc008cf3d80 x28: ffffff8001aa1a80 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000000000012 x22: 0000000000000012 x21: ffffffc008cf3dc0
x20: 0000005572417470 x19: ffffff8001c03800 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc000785000 x3 : ffffffc008cf3dc0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
 faulty_write+0x10/0x20 [faulty]
 ksys_write+0x74/0x110
 __arm64_sys_write+0x1c/0x30
 invoke_syscall+0x54/0x130
 el0_svc_common.constprop.0+0x44/0xf0
 do_el0_svc+0x2c/0xc0
 el0_svc+0x2c/0x90
 el0t_64_sync_handler+0xf4/0x120
 el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233 f d50323bf (b900003f) 
---[ end trace 0000000000000000 ]---
```

## Trobleshooting   
This is an oops message that happened because we attemped to dereference a NULL pointer
```pc : faulty_write+0x10/0x20 [faulty].``` The crash happened exactly 16 bytes (0x10) into the faulty_write function.

Since we have the offset (+0x10), we can map this back to our C code using one of these two methods:   
1. Using gdb:
```gdb faulty.ko
(gdb) list *(faulty_write+0x10)
```
This will print the specific line of C code that caused the crash.   

2. Using objdump   

In our buildroot case, you will find our cross objdump utility at buildroot/output/host/bin/aarc64-linux-objdump   
To disassemble the module to see the assembly alongside the C code, run:   ```aarc64-linux-objdump -S faulty.ko ```
                     
Look for the `<faulty_write>` section and find the instruction at offset 10:.   

