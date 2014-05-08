/*P:400
 * This contains run_guest() which actually calls into the Host<->Guest
 * Switcher and analyzes the return, such as determining if the Guest wants the
 * Host to do something.  This file also contains useful helper routines.
:*/
#include <linux/module.h>
#include <linux/stringify.h>
#include <linux/stddef.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/cpu.h>
#include <linux/freezer.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <asm/paravirt.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/poll.h>
#include <asm/asm-offsets.h>
#include <asm/segment.h>
#include <asm/uaccess.h>
#include "lg.h"

unsigned long switcher_addr;
struct page **lg_switcher_pages;
static struct vm_struct *switcher_vma;
static char page_buffer[PAGE_SIZE];

/* This One Big lock protects all inter-guest data structures. */
DEFINE_MUTEX(lguest_lock);

// File ops thanks to http://stackoverflow.com/questions/1184274/how-to-read-write-files-within-a-linux-kernel-module
struct file* file_open(const char* path, int flags, int rights) {
    struct file* filp = NULL;
    mm_segment_t oldfs;
    int err = 0;

    oldfs = get_fs();
    set_fs(get_ds());
    filp = filp_open(path, flags, rights);
    set_fs(oldfs);
    if(IS_ERR(filp)) {
        err = PTR_ERR(filp);
        return NULL;
    }
    return filp;
}

// File ops thanks to http://stackoverflow.com/questions/1184274/how-to-read-write-files-within-a-linux-kernel-module
void file_close(struct file* file) {
    filp_close(file, NULL);
}

// File ops thanks to http://stackoverflow.com/questions/1184274/how-to-read-write-files-within-a-linux-kernel-module
int file_read(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_read(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}

// File ops thanks to http://stackoverflow.com/questions/1184274/how-to-read-write-files-within-a-linux-kernel-module
int file_write(struct file* file, unsigned long long offset, unsigned char* data, unsigned int size) {
    mm_segment_t oldfs;
    int ret;

    oldfs = get_fs();
    set_fs(get_ds());

    ret = vfs_write(file, data, size, &offset);

    set_fs(oldfs);
    return ret;
}

void dump_cpu_regs(struct lg_cpu *cpu) {
	struct lguest_regs *regs = cpu->regs;

	printk("eax: %ld, ebx: %ld, ecx: %ld, edx: %ld\n", regs->eax, regs->ebx, regs->ecx, regs->edx);
	printk("esi: %ld, edi: %ld, ebp: %ld\n", regs->esi, regs->edi, regs->ebp);
	printk("gs: %ld, fs: %ld, ds: %ld, es: %ld\n", regs->gs, regs->fs, regs->ds, regs->es);
	printk("trapnum: %ld, errcode: %ld\n", regs->trapnum, regs->errcode);
	printk("eip: %ld, cs: %ld, eflags: %ld, esp: %ld, ss: %ld\n", regs->eip, regs->cs, regs->eflags, regs->esp, regs->ss);
}

static void read_cpu_regs(struct lg_cpu *cpu) {
	struct file *regs;
	regs = file_open("/tmp/lgregs", O_RDWR, 0644);
	file_read(regs, 0, (void *) cpu->regs, sizeof(struct lguest_regs));
	file_close(regs);
}

static void write_cpu_regs(struct lg_cpu *cpu) {
	struct file *regs;
	regs = file_open("/tmp/lgregs", O_RDWR | O_CREAT | O_TRUNC, 0644);
	file_write(regs, 0, (void *) cpu->regs, sizeof(struct lguest_regs));
	file_close(regs);
}

static void read_guest_memory(struct lg_cpu *cpu) {
	int i;
	struct file *memory;
	memory = file_open("/tmp/lgmemory", O_RDWR, 0644);
	for (i = 0; i < cpu->lg->pfn_limit; i++) {
		file_read(memory, i * PAGE_SIZE, page_buffer, PAGE_SIZE);
		__lgwrite(cpu, i * PAGE_SIZE, page_buffer, PAGE_SIZE);
	}
	file_close(memory);
}

static void write_guest_memory(struct lg_cpu *cpu) {
	int i;
	struct file *memory;
	memory = file_open("/tmp/lgmemory", O_RDWR | O_CREAT | O_TRUNC, 0644);
	for (i = 0; i < cpu->lg->pfn_limit; i++) {
		__lgread(cpu, page_buffer, i * PAGE_SIZE, PAGE_SIZE);
		file_write(memory, i * PAGE_SIZE, page_buffer, PAGE_SIZE);
	}
	file_close(memory);
}

void flatten_lg_cpu(struct lg_cpu *cpu) {
	struct file *lg_cpu;
	lg_cpu = file_open("/tmp/lg_cpu", O_RDWR | O_CREAT | O_TRUNC, 0644);
	// Write out the top structure of lg_cpu
	file_write(lg_cpu, 0, (void*)cpu, sizeof(struct lg_cpu));
	file_close(lg_cpu);
}

void load_lg_cpu(struct lg_cpu *cpu) {
	struct file *lg_cpu;
	lg_cpu = file_open("/tmp/lg_cpu", O_RDWR, 0644);
	file_read(lg_cpu, 0, (void *) cpu, sizeof(struct lg_cpu));
	file_close(lg_cpu);
}

void write_snapshot(struct lg_cpu *cpu) {
	
	printk("starting snapshot...\n");

	// This might be useful later, keeping it around
	// printk("=== DUMPING REGISTERS SNAPSHOT ===\n");
	// dump_cpu_regs(cpu);

	// Write out the top level of lgcpu
	flatten_lg_cpu(cpu);

	// Write guest memory
	write_guest_memory(cpu);

	// ERROR: Remove this when done testing guest memory transplants
	// read_guest_memory(cpu);

	// Write guest cpu regs
	write_cpu_regs(cpu);

	// ERROR: Remove this when done testing cpu regs transplants
	// read_cpu_regs(cpu);

	// Write shadow page tables
	write_shadow_page_table(cpu);

	// ERROR: Remove this when done testing shadow page table transplants
	// read_shadow_page_table(cpu);

	// ERROR: Remove this when done testing shadow page table transplants
	// remap_physical_pages(cpu);

	printk("snapshot done\n");
}

static void fixup_cpu(struct lg_cpu *cpu, struct lg_cpu *old_cpu) {
	cpu->id = old_cpu->id;
	cpu->cr2 = old_cpu->cr2;
	cpu->ts = old_cpu->ts;
	cpu->esp1 = old_cpu->esp1;
	cpu->ss1 = old_cpu->ss1;
	cpu->changed = old_cpu->changed;
	cpu->pending_notify = old_cpu->pending_notify;
	cpu->regs_page = old_cpu->regs_page;
	cpu->linear_pages = old_cpu->linear_pages;
	cpu->cpu_pgd = old_cpu->cpu_pgd;
	cpu->next_hcall = old_cpu->next_hcall;
	cpu->hrt = old_cpu->hrt;
	cpu->halted = old_cpu->halted;
	cpu->arch = old_cpu->arch;
	cpu->suspended = old_cpu->suspended;
	cpu->suspend_lock = old_cpu->suspend_lock;
}

void rollback(struct lg_cpu *cpu) {
	struct lg_cpu old;
	printk("Attempting to rollback...\n");
	// First load back the top level of lgcpu
	load_lg_cpu(&old);
	// Try to load in previous guest state
	read_guest_memory(cpu);
	read_cpu_regs(cpu);
	// Attempt to fix cpu
	// fixup_cpu(cpu, &old);
	printk("Rollback completed...\n");
}

/*H:010
 * We need to set up the Switcher at a high virtual address.  Remember the
 * Switcher is a few hundred bytes of assembler code which actually changes the
 * CPU to run the Guest, and then changes back to the Host when a trap or
 * interrupt happens.
 *
 * The Switcher code must be at the same virtual address in the Guest as the
 * Host since it will be running as the switchover occurs.
 *
 * Trying to map memory at a particular address is an unusual thing to do, so
 * it's not a simple one-liner.
 */
static __init int map_switcher(void)
{
	int i, err;
	struct page **pagep;

	/*
	 * Map the Switcher in to high memory.
	 *
	 * It turns out that if we choose the address 0xFFC00000 (4MB under the
	 * top virtual address), it makes setting up the page tables really
	 * easy.
	 */

	/* We assume Switcher text fits into a single page. */
	if (end_switcher_text - start_switcher_text > PAGE_SIZE) {
		printk(KERN_ERR "lguest: switcher text too large (%zu)\n",
		       end_switcher_text - start_switcher_text);
		return -EINVAL;
	}

	/*
	 * We allocate an array of struct page pointers.  map_vm_area() wants
	 * this, rather than just an array of pages.
	 */
	lg_switcher_pages = kmalloc(sizeof(lg_switcher_pages[0])
				    * TOTAL_SWITCHER_PAGES,
				    GFP_KERNEL);
	if (!lg_switcher_pages) {
		err = -ENOMEM;
		goto out;
	}

	/*
	 * Now we actually allocate the pages.  The Guest will see these pages,
	 * so we make sure they're zeroed.
	 */
	for (i = 0; i < TOTAL_SWITCHER_PAGES; i++) {
		lg_switcher_pages[i] = alloc_page(GFP_KERNEL|__GFP_ZERO);
		if (!lg_switcher_pages[i]) {
			err = -ENOMEM;
			goto free_some_pages;
		}
	}

	/*
	 * We place the Switcher underneath the fixmap area, which is the
	 * highest virtual address we can get.  This is important, since we
	 * tell the Guest it can't access this memory, so we want its ceiling
	 * as high as possible.
	 */
	switcher_addr = FIXADDR_START - (TOTAL_SWITCHER_PAGES+1)*PAGE_SIZE;

	/*
	 * Now we reserve the "virtual memory area" we want.  We might
	 * not get it in theory, but in practice it's worked so far.
	 * The end address needs +1 because __get_vm_area allocates an
	 * extra guard page, so we need space for that.
	 */
	switcher_vma = __get_vm_area(TOTAL_SWITCHER_PAGES * PAGE_SIZE,
				     VM_ALLOC, switcher_addr, switcher_addr
				     + (TOTAL_SWITCHER_PAGES+1) * PAGE_SIZE);
	if (!switcher_vma) {
		err = -ENOMEM;
		printk("lguest: could not map switcher pages high\n");
		goto free_pages;
	}

	/*
	 * This code actually sets up the pages we've allocated to appear at
	 * switcher_addr.  map_vm_area() takes the vma we allocated above, the
	 * kind of pages we're mapping (kernel pages), and a pointer to our
	 * array of struct pages.  It increments that pointer, but we don't
	 * care.
	 */
	pagep = lg_switcher_pages;
	err = map_vm_area(switcher_vma, PAGE_KERNEL_EXEC, &pagep);
	if (err) {
		printk("lguest: map_vm_area failed: %i\n", err);
		goto free_vma;
	}

	/*
	 * Now the Switcher is mapped at the right address, we can't fail!
	 * Copy in the compiled-in Switcher code (from x86/switcher_32.S).
	 */
	memcpy(switcher_vma->addr, start_switcher_text,
	       end_switcher_text - start_switcher_text);

	printk(KERN_INFO "lguest: mapped switcher at %p\n",
	       switcher_vma->addr);
	/* And we succeeded... */
	return 0;

free_vma:
	vunmap(switcher_vma->addr);
free_pages:
	i = TOTAL_SWITCHER_PAGES;
free_some_pages:
	for (--i; i >= 0; i--)
		__free_pages(lg_switcher_pages[i], 0);
	kfree(lg_switcher_pages);
out:
	return err;
}
/*:*/

/* Cleaning up the mapping when the module is unloaded is almost... too easy. */
static void unmap_switcher(void)
{
	unsigned int i;

	/* vunmap() undoes *both* map_vm_area() and __get_vm_area(). */
	vunmap(switcher_vma->addr);
	/* Now we just need to free the pages we copied the switcher into */
	for (i = 0; i < TOTAL_SWITCHER_PAGES; i++)
		__free_pages(lg_switcher_pages[i], 0);
	kfree(lg_switcher_pages);
}

/*H:032
 * Dealing With Guest Memory.
 *
 * Before we go too much further into the Host, we need to grok the routines
 * we use to deal with Guest memory.
 *
 * When the Guest gives us (what it thinks is) a physical address, we can use
 * the normal copy_from_user() & copy_to_user() on the corresponding place in
 * the memory region allocated by the Launcher.
 *
 * But we can't trust the Guest: it might be trying to access the Launcher
 * code.  We have to check that the range is below the pfn_limit the Launcher
 * gave us.  We have to make sure that addr + len doesn't give us a false
 * positive by overflowing, too.
 */
bool lguest_address_ok(const struct lguest *lg,
		       unsigned long addr, unsigned long len)
{
	return (addr+len-1) / PAGE_SIZE < lg->pfn_limit && (addr+len >= addr);
}

/*
 * This routine copies memory from the Guest.  Here we can see how useful the
 * kill_lguest() routine we met in the Launcher can be: we return a random
 * value (all zeroes) instead of needing to return an error.
 */
void __lgread(struct lg_cpu *cpu, void *b, unsigned long addr, unsigned bytes)
{
	if (!lguest_address_ok(cpu->lg, addr, bytes)
	    || copy_from_user(b, cpu->lg->mem_base + addr, bytes) != 0) {
		/* copy_from_user should do this, but as we rely on it... */
		memset(b, 0, bytes);
		kill_guest(cpu, "bad read address %#lx len %u", addr, bytes);
	}
}

/* This is the write (copy into Guest) version. */
void __lgwrite(struct lg_cpu *cpu, unsigned long addr, const void *b,
	       unsigned bytes)
{
	if (!lguest_address_ok(cpu->lg, addr, bytes)
	    || copy_to_user(cpu->lg->mem_base + addr, b, bytes) != 0)
		kill_guest(cpu, "bad write address %#lx len %u", addr, bytes);
}
/*:*/

/*H:030
 * Let's jump straight to the the main loop which runs the Guest.
 * Remember, this is called by the Launcher reading /dev/lguest, and we keep
 * going around and around until something interesting happens.
 */
int run_guest(struct lg_cpu *cpu, unsigned long __user *user)
{
	/* We stop running once the Guest is dead. */
	while (!cpu->lg->dead) {
		unsigned int irq;
		bool more;

		/* First we run any hypercalls the Guest wants done. */
		if (cpu->hcall) {
			// printk("=== DUMPING REGISTERS HYPERCALL ===\n");
			// dump_cpu_regs(cpu);
			do_hypercalls(cpu);
		}

		/*
		 * It's possible the Guest did a NOTIFY hypercall to the
		 * Launcher.
		 */
		if (cpu->pending_notify) {
			/*
			 * Does it just needs to write to a registered
			 * eventfd (ie. the appropriate virtqueue thread)?
			 */
			if (!send_notify_to_eventfd(cpu)) {
				/* OK, we tell the main Launcher. */
				if (put_user(cpu->pending_notify, user))
					return -EFAULT;
				return sizeof(cpu->pending_notify);
			}
		}

		/*
		 * All long-lived kernel loops need to check with this horrible
		 * thing called the freezer.  If the Host is trying to suspend,
		 * it stops us.
		 */
		try_to_freeze();

		/* Check for signals */
		if (signal_pending(current))
			return -ERESTARTSYS;

		/*
		 * Check if there are any interrupts which can be delivered now:
		 * if so, this sets up the hander to be executed when we next
		 * run the Guest.
		 */
		irq = interrupt_pending(cpu, &more);
		if (irq < LGUEST_IRQS)
			try_deliver_interrupt(cpu, irq, more);

		/*
		 * Just make absolutely sure the Guest is still alive.  One of
		 * those hypercalls could have been fatal, for example.
		 */
		if (cpu->lg->dead)
			break;

		/**
		 * If the guest is suspended skip.
		 */
		// printk("Checking for suspend\n");
		if(cpu->suspended) {
			// Sleep
			down_interruptible(&cpu->suspend_lock);
			// Unlock the lock since we no longer need it
			up(&cpu->suspend_lock);
			// set_current_state(TASK_INTERRUPTIBLE);
			// cond_resched();
			// schedule();
			// printk("Suspended\n");
			// continue;
			// TODO: Attempt to fix clock skew and nmi here?
			init_clockdev(cpu);
		}

		/*
		 * If the Guest asked to be stopped, we sleep.  The Guest's
		 * clock timer will wake us.
		 */
		if (cpu->halted) {
			set_current_state(TASK_INTERRUPTIBLE);
			/*
			 * Just before we sleep, make sure no interrupt snuck in
			 * which we should be doing.
			 */
			if (interrupt_pending(cpu, &more) < LGUEST_IRQS)
				set_current_state(TASK_RUNNING);
			else 
				schedule();
			continue;
		}

		/*
		 * OK, now we're ready to jump into the Guest.  First we put up
		 * the "Do Not Disturb" sign:
		 */
		local_irq_disable();

		/* Actually run the Guest until something happens. */
		lguest_arch_run_guest(cpu);

		/* Now we're ready to be interrupted or moved to other CPUs */
		local_irq_enable();

		/* Now we deal with whatever happened to the Guest. */
		lguest_arch_handle_trap(cpu);
	}

	/* Special case: Guest is 'dead' but wants a reboot. */
	if (cpu->lg->dead == ERR_PTR(-ERESTART))
		return -ERESTART;

	/* The Guest is dead => "No such file or directory" */
	return -ENOENT;
}

/*H:000
 * Welcome to the Host!
 *
 * By this point your brain has been tickled by the Guest code and numbed by
 * the Launcher code; prepare for it to be stretched by the Host code.  This is
 * the heart.  Let's begin at the initialization routine for the Host's lg
 * module.
 */
static int __init init(void)
{
	int err;

	printk("Do I work?\n");
	/* Lguest can't run under Xen, VMI or itself.  It does Tricky Stuff. */
	if (get_kernel_rpl() != 0) {
		printk("lguest is afraid of being a guest\n");
		return -EPERM;
	}

	/* First we put the Switcher up in very high virtual memory. */
	err = map_switcher();
	if (err)
		goto out;

	/* We might need to reserve an interrupt vector. */
	err = init_interrupts();
	if (err)
		goto unmap;

	/* /dev/lguest needs to be registered. */
	err = lguest_device_init();
	if (err)
		goto free_interrupts;

	/* Finally we do some architecture-specific setup. */
	lguest_arch_host_init();

	/* All good! */
	return 0;

free_interrupts:
	free_interrupts();
unmap:
	unmap_switcher();
out:
	return err;
}

/* Cleaning up is just the same code, backwards.  With a little French. */
static void __exit fini(void)
{
	lguest_device_remove();
	free_interrupts();
	unmap_switcher();

	lguest_arch_host_fini();
}
/*:*/

/*
 * The Host side of lguest can be a module.  This is a nice way for people to
 * play with it.
 */
module_init(init);
module_exit(fini);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Rusty Russell <rusty@rustcorp.com.au>");
