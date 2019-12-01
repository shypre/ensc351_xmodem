/* PLEASE NOTE:
 * A .config file is necessary before compiling you kernel module.  Run the
 * appropriate target (menuconfig, xconfig, etc.) in your kernel source 
 * directory before building the module.
 */

#include <linux/init.h>
#include <linux/module.h>  
#include <linux/tty.h>

#ifdef BREAKPOINT
#define BREAKPOINT_MESSAGE "Break in init...\n"
#else
#define BREAKPOINT()
#define BREAKPOINT_MESSAGE "Programmatic breakpoints not supported.\n"
#endif /*BREAKPOINT*/

static int break_in_init;

module_param(break_in_init, int, 0);

int init(void)
{
  if (break_in_init) {
  	/* Programmatically controlled breakpoint. */
  	printk(BREAKPOINT_MESSAGE);
  	BREAKPOINT();
  }
  printk("Hello, embedded world!\n");
  return 0;
}

void cleanup(void)
{
  printk("Goodbye!\n");
}

module_init(init);
module_exit(cleanup);
