#include<linux/module.h>
#include<linux/kernel.h>
#include<linux/hrtimer.h>
#include<linux/sched.h>
#include<linux/mm.h>

MODULE_LICENSE("GPL");

static int pid = 0;
module_param(pid, int, 0444);

typedef struct result result;
struct result
{
	// number of pages in the resident set size
	unsigned long RSS;

	// number of pages in the swap
	unsigned long SWAP;
	
	// number of pages in the working set size
	unsigned long WSS;
};

// walk all the virtual pages of the given process pid
// and return the statistics
result walk_memory_and_get_result(int pid);

// a hrtimer to schedule page walk at regular intervals
struct hrtimer page_walk_timer;

// a callback function that gets called every 10 seconds
enum hrtimer_restart page_walk_timer_callback(struct hrtimer* pwtimer)
{
	// used for farwarding the page_walk_timer
	ktime_t currtime, interval;

	// page walk result
	result res;

	// perform a page walk and get result and then print it
	res = walk_memory_and_get_result(pid);
	printk("PID %d: RSS=%lu KB, SWAP=%lu KB, WSS=%lu KB\n", pid, res.RSS << 2, res.SWAP << 2, res.WSS << 2);

	currtime = ktime_get();
	interval = ktime_set(10, 0);

	// forward the expiry of the timer by interval amount of time with respect to currtime
	hrtimer_forward(pwtimer, currtime, interval);

	return HRTIMER_RESTART;
}

int param_check(void)
{
	// pid must be greater than 0
	if(pid <= 0)
		return 0;

	// all parameters are valid
	return 1;
}

int memory_manager_module_init(void)
{
	// return if param check fails
	if(!param_check())
		return -1;

	// init page walk timer
	hrtimer_init(&page_walk_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	page_walk_timer.function = page_walk_timer_callback;

	// start page walk timer, starting now
	hrtimer_start(&page_walk_timer, ktime_set(10, 0), HRTIMER_MODE_REL);

	return 0;
}

void memory_manager_module_exit(void)
{
	// cancel page walk timer
	hrtimer_cancel(&page_walk_timer);
}

module_init(memory_manager_module_init);
module_exit(memory_manager_module_exit);

struct task_struct* get_task_struct_by_pid(int pid)
{
	// loop iterator
	struct task_struct* ts;

	for_each_process(ts)
	{
		// if required task struct is found
		if(ts->pid == pid)
			return ts;
	}

	return NULL;
}

pte_t* get_page_table_entry_pointer(struct mm_struct* mm, unsigned long address)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pmd_t *pmd;
	pud_t *pud;
	pte_t *pte;

	// get pgd from mm and the page address
	pgd = pgd_offset(mm, address);
	// check if pgd is bad or does not exist
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return NULL;

	// get p4d from from pgd and the page address
	p4d = p4d_offset(pgd, address);
	// check if p4d is bad or does not exist
	if (p4d_none(*p4d) || p4d_bad(*p4d))
		return NULL;

	// get pud from from p4d and the page address
	pud = pud_offset(p4d, address);
	// check if pud is bad or does not exist
	if (pud_none(*pud) || pud_bad(*pud))
		return NULL;

	// get pmd from from pud and the page address
	pmd = pmd_offset(pud, address);
	// check if pmd is bad or does not exist
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return NULL;

	// get pte from pmd and the page address
	pte = pte_offset_map(pmd, address);
	// check if pte does not exist
	if (!pte)
		return NULL;
	
	return pte;
}

void walk_memory_of(struct task_struct* ts, result* r)
{
	struct vm_area_struct* vmas = ts->mm->mmap;

	while(vmas)
	{
		// iterate over all the pages in this vmas
		unsigned long page_first_addr = vmas->vm_start;
		while(page_first_addr != vmas->vm_end)
		{
			// get page table entry corresponding to this page
			pte_t* ptep = get_page_table_entry_pointer(ts->mm, page_first_addr);

			if(ptep)
			{
				if(pte_present(*ptep))
					r->RSS++;
				if((!(pte_val(*ptep) & _PAGE_PRESENT)) && ((pte_val(*ptep) & _PAGE_RW)))
					r->SWAP++;
				if((pte_val(*ptep) & _PAGE_PRESENT) && pte_young(*ptep))
				{
					r->WSS++;
					test_and_clear_bit(_PAGE_BIT_ACCESSED, (unsigned long *) (&(ptep->pte)));
				}
			}

			page_first_addr += PAGE_SIZE;
		}

		vmas = vmas->vm_next;
	}
}

result walk_memory_and_get_result(int pid)
{
	// initialize result to all 0
	result r = {};

	// task struct we are concerned about
	struct task_struct* ts = NULL;

	// get task struct using its pid
	ts = get_task_struct_by_pid(pid);

	if(!ts)
		return r;

	// walk all the virtual memory ranges
	walk_memory_of(ts, &r);

	return r;
}
