#include <linux/kernel.h>
#include <../include/linux/syscalls.h>

SYSCALL_DEFINE1(my_precious, bool, x)
{
	if(x == 0)
	printk("my_precious = 0----------\n");

	if(x==1)
	printk("my_precious = 1----------\n");

	return 0;
}
