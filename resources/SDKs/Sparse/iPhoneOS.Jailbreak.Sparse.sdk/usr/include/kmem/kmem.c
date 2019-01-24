#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <mach/mach.h>

#include "kmem.h"
#include <kutils/kutils.h>
#include <common.h>

// the exploit bootstraps the full kernel memory read/write with a fake
// task which just allows reading via the bsd_info->pid trick
// this first port is kmem_read_port
mach_port_t kmem_read_port = MACH_PORT_NULL;
void prepare_rk_via_kmem_read_port(mach_port_t port) {
    kmem_read_port = port;
}

mach_port_t tfp0 = MACH_PORT_NULL;

//void sync_tfp0(mach_port_t *task_faux_pid0)
//{
//	if (*task_faux_pid0 == MACH_PORT_NULL)
//	{
//		task_faux_pid0 = &tfp0;
//	}
//	else
//	{
//		tfp0 = *task_faux_pid0;
//	}
//}

void prepare_rwk_via_tfp0(mach_port_t port) {
    tfp0 = port;
}

void prepare_for_rw_with_fake_tfp0(mach_port_t fake_tfp0) {
  tfp0 = fake_tfp0;
}

int have_kmem_read() {
    return (kmem_read_port != MACH_PORT_NULL) || (tfp0 != MACH_PORT_NULL);
}

int have_kmem_write() {
    return (tfp0 != MACH_PORT_NULL);
}

size_t kread(uint64_t where, void *p, size_t size) {
    int rv;
    size_t offset = 0;
    while (offset < size) {
        mach_vm_size_t sz, chunk = 2048;
        if (chunk > size - offset) {
            chunk = size - offset;
        }
        rv = mach_vm_read_overwrite(tfp0, where + offset, chunk, (mach_vm_address_t)p + offset, &sz);
        if (rv || sz == 0) {
            LOG("[e] error reading kernel @%p\n", (void *)(offset + where));
            break;
        }
        offset += sz;
    }
    return offset;
}

size_t kwrite(uint64_t where, const void *p, size_t size) {
    int rv;
    size_t offset = 0;
    while (offset < size) {
        size_t chunk = 2048;
        if (chunk > size - offset) {
            chunk = size - offset;
        }
        rv = mach_vm_write(tfp0, where + offset, (mach_vm_offset_t)p + offset, (mach_msg_type_number_t)chunk);
        if (rv) {
            LOG("[e] error writing kernel @%p\n", (void *)(offset + where));
            break;
        }
        offset += chunk;
    }
    return offset;
}

void WriteAnywhere32(uint64_t kaddr, uint32_t val) {
  if (tfp0 == MACH_PORT_NULL) {
    LOG("attempt to write to kernel memory before any kernel memory write primitives available\n");
    sleep(3);
    return;
  }
  
  kern_return_t err;
  err = mach_vm_write(tfp0,
                      (mach_vm_address_t)kaddr,
                      (vm_offset_t)&val,
                      (mach_msg_type_number_t)sizeof(uint32_t));
  
  if (err != KERN_SUCCESS) {
    LOG("tfp0 write failed: %s %x\n", mach_error_string(err), err);
    return;
  }
}

void WriteAnywhere64(uint64_t kaddr, uint64_t val) {
  uint32_t lower = (uint32_t)(val & 0xffffffff);
  uint32_t higher = (uint32_t)(val >> 32);
  WriteAnywhere32(kaddr, lower);
  WriteAnywhere32(kaddr+4, higher);
}

uint32_t rk32_via_kmem_read_port(uint64_t kaddr) {
    kern_return_t err;
    if (kmem_read_port == MACH_PORT_NULL) {
        LOG("kmem_read_port not set, have you called prepare_rk?\n");
        sleep(10);
        exit(EXIT_FAILURE);
    }
    
    mach_port_context_t context = (mach_port_context_t)kaddr - 0x10;
    err = mach_port_set_context(mach_task_self(), kmem_read_port, context);
    if (err != KERN_SUCCESS) {
        LOG("error setting context off of dangling port: %x %s\n", err, mach_error_string(err));
        sleep(10);
        exit(EXIT_FAILURE);
    }
    
    // now do the read:
    uint32_t val = 0;
    err = pid_for_task(kmem_read_port, (int*)&val);
    if (err != KERN_SUCCESS) {
        LOG("error calling pid_for_task %x %s", err, mach_error_string(err));
        sleep(10);
        exit(EXIT_FAILURE);
    }
    
    return val;
}

uint32_t rk32_via_tfp0(uint64_t kaddr) {
    kern_return_t err;
    uint32_t val = 0;
    mach_vm_size_t outsize = 0;
    err = mach_vm_read_overwrite(tfp0,
                                 (mach_vm_address_t)kaddr,
                                 (mach_vm_size_t)sizeof(uint32_t),
                                 (mach_vm_address_t)&val,
                                 &outsize);
    if (err != KERN_SUCCESS){
        LOG("tfp0 read failed %s addr: 0x%llx err:%x port:%x\n", mach_error_string(err), kaddr, err, tfp0);
        sleep(3);
        return 0;
    }
    
    if (outsize != sizeof(uint32_t)){
        LOG("tfp0 read was short (expected %lx, got %llx\n", sizeof(uint32_t), outsize);
        sleep(3);
        return 0;
    }
    return val;
}

uint32_t ReadAnywhere32(uint64_t kaddr) {
    if (tfp0 != MACH_PORT_NULL) {
        return rk32_via_tfp0(kaddr);
    }
    
    if (kmem_read_port != MACH_PORT_NULL) {
        return rk32_via_kmem_read_port(kaddr);
    }
    
    LOG("attempt to read kernel memory but no kernel memory read primitives available\n");
    sleep(3);
    
    return 0;
}

uint64_t ReadAnywhere64(uint64_t kaddr) {
  uint64_t lower = ReadAnywhere32(kaddr);
  uint64_t higher = ReadAnywhere32(kaddr+4);
  uint64_t full = ((higher<<32) | lower);
  return full;
}

void wkbuffer(uint64_t kaddr, void* buffer, uint32_t length) {
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to write to kernel memory before any kernel memory write primitives available\n");
        sleep(3);
        return;
    }
    
    kern_return_t err;
    err = mach_vm_write(tfp0,
                        (mach_vm_address_t)kaddr,
                        (vm_offset_t)buffer,
                        (mach_msg_type_number_t)length);
    
    if (err != KERN_SUCCESS) {
        LOG("tfp0 write failed: %s %x\n", mach_error_string(err), err);
        return;
    }
}

void rkbuffer(uint64_t kaddr, void* buffer, uint32_t length) {
    kern_return_t err;
    mach_vm_size_t outsize = 0;
    err = mach_vm_read_overwrite(tfp0,
                                 (mach_vm_address_t)kaddr,
                                 (mach_vm_size_t)length,
                                 (mach_vm_address_t)buffer,
                                 &outsize);
    if (err != KERN_SUCCESS){
        LOG("tfp0 read failed %s addr: 0x%llx err:%x port:%x\n", mach_error_string(err), kaddr, err, tfp0);
        sleep(3);
        return;
    }
    
    if (outsize != length){
        LOG("tfp0 read was short (expected %lx, got %llx\n", sizeof(uint32_t), outsize);
        sleep(3);
        return;
    }
}

const uint64_t kernel_address_space_base = 0xffff000000000000;
void kmemcpy(uint64_t dest, uint64_t src, uint32_t length) {
    if (dest >= kernel_address_space_base) {
        // copy to kernel:
        wkbuffer(dest, (void*) src, length);
    } else {
        // copy from kernel
        rkbuffer(src, (void*)dest, length);
    }
}

uint64_t kmem_alloc(uint64_t size) {
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to allocate kernel memory before any kernel memory write primitives available\n");
        sleep(3);
        return 0;
    }
    
    kern_return_t err;
    mach_vm_address_t addr = 0;
    mach_vm_size_t ksize = round_page_kernel(size);
    err = mach_vm_allocate(tfp0, &addr, ksize, VM_FLAGS_ANYWHERE);
    if (err != KERN_SUCCESS) {
        LOG("unable to allocate kernel memory via tfp0: %s %x\n", mach_error_string(err), err);
        sleep(3);
        return 0;
    }
    return addr;
}

uint64_t kmem_alloc_wired(uint64_t size) {
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to allocate kernel memory before any kernel memory write primitives available\n");
        sleep(3);
        return 0;
    }
    
    kern_return_t err;
    mach_vm_address_t addr = 0;
    mach_vm_size_t ksize = round_page_kernel(size);
    
    LOG("vm_kernel_page_size: %lx\n", vm_kernel_page_size);
    
    err = mach_vm_allocate(tfp0, &addr, ksize+0x4000, VM_FLAGS_ANYWHERE);
    if (err != KERN_SUCCESS) {
        LOG("unable to allocate kernel memory via tfp0: %s %x\n", mach_error_string(err), err);
        sleep(3);
        return 0;
    }
    
    LOG("allocated address: %llx\n", addr);
    
    addr += 0x3fff;
    addr &= ~0x3fffull;
    
    LOG("address to wire: %llx\n", addr);
    
    err = mach_vm_wire(fake_host_priv(), tfp0, addr, ksize, VM_PROT_READ|VM_PROT_WRITE);
    if (err != KERN_SUCCESS) {
        LOG("unable to wire kernel memory via tfp0: %s %x\n", mach_error_string(err), err);
        sleep(3);
        return 0;
    }
    return addr;
}

void kmem_free(uint64_t kaddr, uint64_t size) {
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to deallocate kernel memory before any kernel memory write primitives available\n");
        sleep(3);
        return;
    }
    
    kern_return_t err;
    mach_vm_size_t ksize = round_page_kernel(size);
    err = mach_vm_deallocate(tfp0, kaddr, ksize);
    if (err != KERN_SUCCESS) {
        LOG("unable to deallocate kernel memory via tfp0: %s %x\n", mach_error_string(err), err);
        sleep(3);
        return;
    }
}

void kmem_protect(uint64_t kaddr, uint32_t size, int prot) {
    if (tfp0 == MACH_PORT_NULL) {
        LOG("attempt to change protection of kernel memory before any kernel memory write primitives available\n");
        sleep(3);
        return;
    }
    kern_return_t err;
    err = mach_vm_protect(tfp0, (mach_vm_address_t)kaddr, (mach_vm_size_t)size, 0, (vm_prot_t)prot);
    if (err != KERN_SUCCESS) {
        LOG("unable to change protection of kernel memory via tfp0: %s %x\n", mach_error_string(err), err);
        sleep(3);
        return;
    }
}
