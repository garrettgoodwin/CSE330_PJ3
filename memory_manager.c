#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/sched/signal.h>
#include <linux/semaphore.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/mm_types.h>
#include <linux/pgtable.h>
#include <linux/hrtimer.h>
#include <linux/mm.h>

/ Timer Variables static struct hrtimer hr_timer;
static struct hrtimer no_restart_hr_timer;
unsigned long timer_interval_ns = 10e9;
static int timer_count = 0;

// Declare variables as readable and rritasble
int pid = 0;
module_param(pid, int, S_IRUSR | S_IWUSR);

int ptep_test_and_clear_young(struct vm_area_struct *vma, unsigned long addr, pte_t *ptep)
{
	int ret = 0;
	if (pte_young(*ptep))
		ret = test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *)&ptep->pte);
	return ret;
}

void page_walk(void)
{
	// Current Executing process = task
	struct task_struct *task = pid_task(find_vpid(pid), PIDTYPE_PID);

	// Memory Management of task
	struct mm_struct *mm = task->mm;

	// Virtual Memory Area
	struct vm_area_struct *vma = task->mm->mmap;

	// Five-Level Page Table
	pgd_t *pgd;
	p4d_t *p4d;
	pmd_t *pmd;
	pud_t *pud;
	pte_t *ptep, pte;

	unsigned long rss = 0;
	unsigned long wss = 0;
	unsigned long swap = 0;

	printk(KERN_INFO "Entering Page_Walk\n");

	while (vma)
	{
		unsigned long address;

		// For loop traverses each page in the VMA
		for (address = vma->vm_start; address < vma->vm_end; address += PAGE_SIZE)
		{
			// Checks if pgd, p4d, pud, and pmd is bad or exists
			pgd = pgd_offset(task->mm, address);
			if (pgd_none(*pgd) || pgd_bad(*pgd))
			{
				break;
			}

			p4d = p4d_offset(pgd, address);
			if (p4d_none(*p4d) || p4d_bad(*p4d))
			{
				break;
			}

			pud = pud_offset(p4d, address);
			if (pud_none(*pud) || pud_bad(*pud))
			{
				break;
			}

			pmd = pmd_offset(pud, address);
			if (pmd_none(*pmd) || pmd_bad(*pmd))
			{
				break;
			}

			// Gets and stores the pte
			ptep = pte_offset_map(pmd, address);
			pte = *ptep;

			// Check if pte exists and if it has been accessed recently
			if (ptep_test_and_clear_young(vma, address, ptep))
			{
				wss += PAGE_SIZE;
			}
			else
			{
				swap += PAGE_SIZE;
			}
			rss += PAGE_SIZE;
			// Unamp virtual memoory
			pte_unmap(ptep);
		}
		printk(KERN_INFO "Adding to RSS\n");
		// rss += vma->vm_end - vma->vm_start;
		vma = vma->vm_next;
	}
	printk(KERN_INFO "Leaving page walk");
	printk(KERN_INFO "PID %d: RSS=%lu KB, SWAP=%d KB, WSS=%lu KB\n", pid, rss / 1024, swap / 1024, wss / 1024);
}

enum hrtimer_restart timer_callback(struct hrtimer *timer)
{
	if (timer_count < 3)
	{
		page_walk();
		hrtimer_forward_now(&hr_timer, ms_to_ktime(10000));
		timer_count += 1;
		return HRTIMER_RESTART;
	}
	else
	{
		return HRTIMER_NORESTART;
	}
}

void timer_init(void)
{
	// for first two test cases
	hrtimer_init(&hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	hr_timer.function = &timer_callback;
	hrtimer_start(&hr_timer, ms_to_ktime(10000), HRTIMER_MODE_REL);
}

static int ModuleInit(void)
{
	printk(KERN_INFO "CSE330 POroject-2 Kernel Module Inserted\n");

	// Call Timer Method
	timer_init();

	// printk(KERN_INFO "Got to the end\n");

	return 0;
}

static void ModuleExit(void)
{
	hrtimer_cancel(&hr_timer);
	printk(KERN_INFO "CSE330 POroject-2 Kernel Module Removed\n");
}

module_init(ModuleInit);
module_exit(ModuleExit);
MODULE_LICENSE("GPL");
