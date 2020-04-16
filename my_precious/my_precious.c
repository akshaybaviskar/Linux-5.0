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

pte_t* get_pte(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte = 0;

	pgd = pgd_offset(mm, addr);
	if (!pgd_none(*pgd)) {
		p4d = p4d_offset(pgd, addr);
		if (!p4d_none(*p4d)) {
			pud = pud_offset(p4d, addr);
			if (!pud_none(*pud)) {
				pmd = pmd_offset(pud, addr);
				if (!pmd_none(*pmd))
					pte = pte_offset_map(pmd, addr);
			}
		}
	}
	return pte;
}

int save_context(void)
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

	//Traverse all vm_area
	for(;vm_it!=NULL; vm_it = vm_it->vm_next)
	{
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
			}
			vm_it->pte_ptr = pte_ptr_local;
			flush_tlb_range(vm_it, vm_it->vm_start, vm_it->vm_end);
		}
	}

	return 0;	
}

int restore_context(void)
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
		copy_to_user((void *)temp->v_address, (const void *)temp->k_address, 4096);
		temp2 = temp;
		temp = temp->next;
		kfree(temp2->k_address);
		kfree(temp2);
	}
	current->mp_ctx_ptr = NULL;

	//Traverse all vm_area
	for(;vm_it!=NULL; vm_it = vm_it->vm_next)
	{
		// True if vm_area is anonymous and not stack and only if given area is writable.
		if(vma_is_anonymous(vm_it) &&  (!vma_is_stack_for_current(vm_it)) && (vm_it->vm_flags & (VM_WRITE | VM_MAYWRITE)))
		{
			register char* pte_ptr_local = vm_it->pte_ptr;
			register unsigned long addr = vm_it->vm_start;
			no_of_pages = (vm_it->vm_end - addr)/4096;

			for(i = 0;i<no_of_pages;i++)
			{

				/*PTE already existed, do nothing.*/
				if(pte_ptr_local[i] == 1)
				{
					//printk("Nothing\n");
				}
				else
				{
					ptep = get_pte(vm_it->vm_mm, addr);
					if(ptep != NULL)
					{
						/*Free newly allocated page.*/
						page = pte_page(*ptep);	
	    				set_pte(ptep, native_make_pte(0));
						__free_page(page);
					}
				}
				addr = addr+4096;
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
		ret = save_context();
		if(ret == 0)
		{
			current->mp_flag = 1;
		}
	}
	else if((x==1) && (current->mp_flag == 1))
	{
		ret = restore_context();
		if(ret == 0)
		{
			current->mp_flag = 0;
		}
	}
	else
	{
		ret = -EINVAL;
	}

    //    printk("\nmy_precious ++- pid %d\n", current->pid);
        return ret;
}
