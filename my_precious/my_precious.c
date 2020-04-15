#include <linux/kernel.h>
#include <../arch/x86/include/asm/current.h>
#include <../include/linux/sched.h>
#include <../include/linux/mm_types.h>
#include <../include/linux/mm.h>
#include <../include/linux/uaccess.h>
#include <../include/linux/slab.h>
#include <../include/linux/syscalls.h>
#include <../arch/x86/include/asm/pgtable.h>
#include <../arch/x86/include/asm/pgtable_64.h>
#include <../arch/x86/include/asm/tlbflush.h>
#include <../include/linux/page_ref.h>
#include <../include/linux/hugetlb.h>

#define Approach 1
int first = 1;
unsigned long page_boundary = 0;
pmd_t *pmd_global;

#if(Approach == 1)
pte_t* get_pte(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte = 0;

	if(first || (page_boundary == 0))
	{
		pgd = pgd_offset(mm, addr);
		if (!pgd_none(*pgd)) {
			p4d = p4d_offset(pgd, addr);
			if (!p4d_none(*p4d)) {
				pud = pud_offset(p4d, addr);
				if (!pud_none(*pud)) {
					pmd = pmd_offset(pud, addr);
					pmd_global = pmd;
					if (!pmd_none(*pmd))
						pte = pte_offset_map(pmd, addr);
				}
			}
		}
	}
	else
	{
				//	printk("entered\t");
					if (!pmd_none(*pmd_global))
						pte = pte_offset_map(pmd_global, addr);
	}
	return pte;
}

int copy(void)
{
	unsigned int no_of_pages;
	unsigned int i;
	pte_t *pte;
	struct vm_area_struct *vm_it;

	if(current == NULL)
	{
		printk("Failed to get a task pointer.\n");
		return -9;
	}
	vm_it = current->mm->mmap;
	if(vm_it == NULL)
	{
		printk("Failed to get a vm_area pointer.\n");
		return -8;
	}

	struct mm_struct *mm = current->mm;
	struct vm_area_struct* vm_it_temp = vm_it;

	//Traverse all vm_area
	for(;vm_it->vm_end< mm->start_stack; vm_it = vm_it->vm_next)
	{
		first = 1;
		// True if vm_area is anonymous and not stack and only if given area is writable.
		if(vma_is_anonymous(vm_it) &&  (!vma_is_stack_for_current(vm_it)) && (vm_it->vm_flags & (VM_WRITE|VM_MAYWRITE)))
		{
		//	printk("Copying for %lx - %lx \n",vm_it->vm_start,vm_it->vm_end);
			register char* pte_ptr_local;
			no_of_pages = (vm_it->vm_end - vm_it->vm_start)/4096;
			
			pte_ptr_local = (char*) kmalloc(no_of_pages,GFP_USER);
			
			if(pte_ptr_local == NULL)
			{
				printk("Failed to obtain kmalloc.\n");
				break;
			}
			// If PTE exists, write protect the page, save the info and flush the TLB.
			register unsigned long addr;
			addr = vm_it->vm_start;
			for(i = 0;i<no_of_pages;i++)
			{
					
				pte = get_pte(vm_it->vm_mm, addr);
				first = 0;

				if(pte_none(*pte))
				{	
					pte_ptr_local[i] = 0;
				}
				else
				{
					pte_ptr_local[i] = 1;
					*pte = pte_wrprotect(*pte);
				}
				addr = addr + 4096;
			//	printk("%lx \n", addr);
				page_boundary = addr&0x1ff000; 
			}
			vm_it->pte_ptr = pte_ptr_local;
			flush_tlb_range(vm_it, vm_it->vm_start, vm_it->vm_end);
		}
	}

	return 0;	
}

int restore(void)
{
	unsigned int no_of_pages;
	unsigned int i;
	pte_t *ptep;
	struct vm_area_struct *vm_it;
	struct page *page;

	if(current == NULL)
	{
		printk("Failed to get a task pointer.\n");
		return -9;
	}
	
	vm_it = current->mm->mmap;
	if(vm_it == NULL)
	{
		printk("Failed to get a vm_area pointer.\n");
		return -8;
	}

	struct my_pre_context* temp = current->mp_ctx_ptr;
	struct my_pre_context* temp2 = current->mp_ctx_ptr;
	while(temp)
	{
		copy_to_user((void *)temp->address, (const void *)temp->data, 4096);
		temp2 = temp;
		temp = temp->next;
		kfree(temp2->data);
		kfree(temp2);
	}
	current->mp_ctx_ptr = NULL;

	struct mm_struct *mm = current->mm;
	struct vm_area_struct* vm_it_temp = vm_it;

	//Traverse all vm_area
	for(;vm_it->vm_end< mm->start_stack; vm_it = vm_it->vm_next)
	{
		// True if vm_area is anonymous and not stack and only if given area is writable.
		first = 1;
		if(vma_is_anonymous(vm_it) &&  (!vma_is_stack_for_current(vm_it)) && (vm_it->vm_flags & (VM_WRITE | VM_MAYWRITE)))
		{
			register char* pte_ptr_local = vm_it->pte_ptr;
			register unsigned long addr = vm_it->vm_start;
			no_of_pages = (vm_it->vm_end - addr)/4096;

			for(i = 0;i<no_of_pages;i++)
			{

				ptep = get_pte(vm_it->vm_mm, addr);
				first = 0;
				/*Case 1: PTE already existed, do nothing.*/
				if(pte_ptr_local[i] == 1)
				{
					//printk("Nothing\n");
				}
				else
				{
					if(ptep != NULL);
					{
						/*Free newly allocated page.*/
						page = pte_page(*ptep);	
	    				set_pte(ptep, native_make_pte(0));
			//			//flush_tlb_page(vm_it,addr);
						__free_page(page);
					}
				}
				addr = addr+4096;
				page_boundary = addr&0x1ff000; 
			}
			
			kfree(vm_it->pte_ptr);
			vm_it->pte_ptr = NULL;
			flush_tlb_range(vm_it, vm_it->vm_start, vm_it->vm_end);
		}
	}


	return 0;
}

SYSCALL_DEFINE1(my_precious, bool, x)
{
	long ret;	
	if((x==0) && (current->mp_flag != 1))
	{	
		ret = copy();
		if(ret == 0)
		{
			current->mp_flag = 1;
		}
	}
	else if((x==1) && (current->mp_flag == 1))
	{
		ret = restore();
		if(ret == 0)
		{
			current->mp_flag = 0;
		}
	}
	else
	{
		ret = -EINVAL;
	}

        //printk("\nmy_precious ++- pid %d\n", current->pid);
        return ret;
}
#endif
#if(Approach == 2)
void copy(unsigned long end, unsigned long start, struct vm_area_struct* vm_it)
{
	unsigned int cpy_success;
	unsigned long size = end - start;
	if(size==0)
	{
		printk("empty vm_area at %lx\n",start);
		return;
	}
	vm_it->pte_ptr = kmalloc(size,GFP_USER);
	if(vm_it->pte_ptr == NULL)
	{
		printk("kernel memory allocation failed for %lx\n", start);
		return;
	}
	//copy_from_user(void *to, const void *from, unsigned long n)
	cpy_success  = copy_from_user((void *)vm_it->pte_ptr, (const void *)start, size);
	if(cpy_success == 0)
	{
		printk("copy successfull for %lx\n", start);
	}
	else 
	{
		printk("copy failed for %lx\n", start);
	}
	return;
}

void restore(unsigned long end, unsigned long start, struct vm_area_struct* vm_it)
{
	// Assuming that address space will not be manipulated between the points of saving and restoring the context.
	// Hence, vm addresses would remain same.
	unsigned int restore_success;
	unsigned long size = end - start;
	if(size==0)
	{
		printk("empty vm_area at %lx\n",start);
		return;
	}

	if(vm_it->pte_ptr == NULL)
	{
		printk("Context was not saved for %lx\n", start);
		return;
	}
	//copy_to_user(void *to, const void *from, unsigned long n)
	restore_success  = copy_to_user((void *)start, (const void *)vm_it->pte_ptr, size);
	if(restore_success == 0)
	{
		printk("restore successfull for %lx\n", start);
	}
	else 
	{
		printk("restore failed for %lx\n", start);
	}
	
	//free kernel memory after restore
	kfree(vm_it->pte_ptr);
	vm_it->pte_ptr = NULL;
}

SYSCALL_DEFINE1(my_precious, bool, x)
{
	struct vm_area_struct *vm_it;
	struct task_struct *task = current;
	if(task == NULL)
	{
		printk("Failed to get a task pointer.\n");
		return -9;
	}
	
	vm_it = current->mm->mmap;
	if(vm_it == NULL)
	{
		printk("Failed to get a vm_area pointer.\n");
		return -8;
	}

	for(;vm_it!=NULL; vm_it = vm_it->vm_next)
	{
		// True if vm_area is anonymous and not stack
		if(vma_is_anonymous(vm_it) &&  !vma_is_stack_for_current(vm_it))
		{
			printk("%lx - %lx \t",vm_it->vm_start,vm_it->vm_end);
			if((x==0)&& (current->mp_flag != 1))
			{
				current->mp_flag = 1;
				//void copy(unsigned long end, unsigned long start, struct vm_area_struct* vm_it)
				copy(vm_it->vm_end, vm_it->vm_start, vm_it);
			        //printk("\nmy_precious called by pid %d with %d.\n", current->pid, x);

			}
			else if((x==1)&& (current->mp_flag == 1))
			{
				current->mp_flag = 0;
				//void restore(unsigned long end, unsigned long start, struct vm_area_struct* vm_it)
				restore(vm_it->vm_end, vm_it->vm_start, vm_it);
			        //printk("\nmy_precious called by pid %d with %d.\n", current->pid, x);

			}
		}
	}

        printk("\nmy_precious !!!!!!!!!!!!id %d.\n", current->pid);
	
        return 0;
}
#endif
