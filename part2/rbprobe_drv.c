#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/string.h>
#include <linux/device.h>
#include <linux/jiffies.h>
#include<linux/kprobes.h>
#include <linux/kallsyms.h>
#include <linux/sched.h>
#include<linux/init.h>
#include<linux/moduleparam.h>
#include <linux/ptrace.h>
#include<linux/thread_info.h>
#include <linux/cdev.h>

#include "rb_structures.h"
#include "rbtree_rbt530.h"

#define DEVICE_NAME "RBprobe"     // device name to be created and registered

/* per device structure */
struct rbprobe_dev {
	struct cdev cdev;             			/* The cdev structure */
	char name[20];                			/* Name of device*/
	int num_probes;			      			/* Number of probes the current device is holded */
	struct rbprobe_trace *rp;     			/* Output buffer to keep track of last instance of the probe hit*/
} *rbprobe_devp;

struct rbt530_dev {
	struct cdev cdev;               		/* Character device structure */
	char name[20];                  		/* Name of the device */         
	struct rb_root rbt530;          		/* Holds the root of the rb_tree*/
    struct rb_node *current_node;   		/* Points to the current node which is being read*/
	int read_order;                 		/* A flag to check if the read_order must by ascending or descending.*/
};


static dev_t rbprobe_dev_number;            /* Allotted device number */
struct class *rbprobe_dev_class;            /* Tie with the device model */

static char *user_name = "";                /* the default user name, can be replaced if a new name is attached in insmod command */

module_param(user_name,charp,0000);	        /*to get parameter from load.sh script to greet the user*/

static struct kprobe **list_of_probes;      /*A pointer array to keep track of all the registered probes */

/* 
*	A pre-handler function which gets called when a breakpoint was hit
*/
static int pre_handler_rbprobe(struct kprobe *p, struct pt_regs *regs){
	struct task_struct *task = current;                           /*Get the current running task */
	rbprobe_devp->rp->pid = task->pid;                            /*Get the pid of the running task */
	rbprobe_devp->rp->time_stamp = (long long)native_read_tsc();  /* current time */
	rbprobe_devp->rp->address = (void *)p->addr;                  /*The Address of the probe that is being hit */
	return 0;
}

/* 
*	A post-handler function which that be called at the end afterpre_handler
*/
void post_handler_rbprobe(struct kprobe *p, struct pt_regs *regs, unsigned long flags){
	/*Reading the file pointer from the register */
	struct file *file = (struct file *)regs->ax;
	 /*Taking the file's private data */
	 struct rbt530_dev *rbt530_devp = file->private_data;
	 struct rb_node *node;
	 int count  = 0;
	/*Tracking the 10 nodes from the current pointer.
	*	I'm returning 5 node previous and after to the current node;
	*/
	 for (node = rbt530_devp->current_node; node; node = rb_next(node)){
	 	if(count >=5){
			break;
		}
		rbprobe_devp->rp->rb_object[count] = rb_entry(node, struct tree_node, node)->rb_object;
		count++;
	}

	for (node = rbt530_devp->current_node; node; node = rb_prev(node)){
		if(count >=10){
			break;
		}
		rbprobe_devp->rp->rb_object[count] = rb_entry(node, struct tree_node, node)->rb_object;
		count++;
	}
	rbprobe_devp->rp->node_count = count-1;
}

/*
* Open rbprobe driver
*/
int rbprobe_driver_open(struct inode *inode, struct file *file)
{
	struct rbprobe_dev *rbprobe_devp;

	/* Get the per-device structure that contains this cdev */
	rbprobe_devp = container_of(inode->i_cdev, struct rbprobe_dev, cdev);

	/* Easy access to cmos_devp from rest of the entry points */
	file->private_data = rbprobe_devp;
	printk("\n%s is opening \n", rbprobe_devp->name);
	return 0;
}

/*
 * Release rbprobe driver
 */
static int rbprobe_driver_release(struct inode *inode, struct file *file)
{
	struct rbprobe_dev *rbprobe_devp = file->private_data;

	printk("\n%s is closing\n", rbprobe_devp->name);
	
	return 0;
}

/*
 * Write to rbprobe driver. Register the probe in a given memory address.
 */
ssize_t rbprobe_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
	int ret;
	struct rbprobe_init input; /*Read the input parameters from the user*/
	struct kprobe *kp; /*A probe object holding the data that needs to be registered or unregistered*/
	int current_probe_index = 0; /* Index to iterate over the list of probes*/

	
	/*Reading the arguments inputted by the user*/
	copy_from_user(&input, (struct rbprobe_init *)buf, sizeof(struct rbprobe_init));
	kp = kmalloc(sizeof(struct kprobe), GFP_KERNEL);
	memset(kp, 0, sizeof(struct kprobe));

	/*Assigning the values to the kprobe structure either to register or remove the probe */
	kp->pre_handler = pre_handler_rbprobe;
	kp->post_handler = post_handler_rbprobe;
	kp->addr = (kprobe_opcode_t *) kallsyms_lookup_name(input.function_name) + input.offset;

	/*Check the user choice of whether to register ot unregister probe*/
	if(input.isRegisterProbe == 1){
		printk("Registering kprobe\n");
		if(!kp->addr){
			printk(KERN_ALERT "Couldn't find %s to plant kprobe \n", "rbt530_driver_write");
			return -EINVAL;
		}

		/*Increment and assign the probes, since the default value is -1 */
		rbprobe_devp->num_probes++;
		/*Allocate memory to register the give probe */
		list_of_probes[rbprobe_devp->num_probes] = kmalloc(sizeof(struct kprobe), GFP_KERNEL);
		list_of_probes[rbprobe_devp->num_probes] = kp;

		/*Check if the probe is registered successfully */
		if((ret = register_kprobe(kp) < 0)){
			printk(KERN_ALERT "Register kprobe failed, retunred %d\n", ret);
			return -EINVAL;
		}
		printk(KERN_ALERT "Kprobe registered \n");
	}else{
		/*If the given request is to unregister, search for the given address from the list of probes and unregister it.
		* Deallocation will happen at driver exit.
		*/
		while(current_probe_index <= rbprobe_devp->num_probes){
			if(list_of_probes[current_probe_index]->addr == kp->addr){
				unregister_kprobe(list_of_probes[current_probe_index]);
				break;
			}
			current_probe_index++;	
		}
	}
	return 0;
}

/*
 * Read to rbprobe driver. Post the collected debugger information and send it to the user.
 */
ssize_t rbprobe_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	/* If there is no probe information to retrive, return -1 */
	if(rbprobe_devp->rp == NULL){
		printk(KERN_ALERT "There is no debug information.\n");
		return -EINVAL;
	}else{
		/*Copy the debugger information from the kerner buffer to the user*/
		copy_to_user((struct rbprobe_trace *)buf, rbprobe_devp->rp, sizeof(struct rbprobe_trace));
	}

	/*Return the size of the object on success */
	return sizeof(struct rbprobe_trace);
}



/* File operations structure. Defined in linux/fs.h */
static struct file_operations rbprobe_fops = {
    .owner		= THIS_MODULE,                /* Owner */
    .open		= rbprobe_driver_open,        /* Open method */
    .release	= rbprobe_driver_release,     /* Release method */
    .write		= rbprobe_driver_write,       /* Write method */
    .read		= rbprobe_driver_read,        /* Read method */
};

/*
 * Driver Initialization
 */
int __init rbprobe_driver_init(void)
{
	int ret;
	/* Request dynamic allocation of a device major number */
	if (alloc_chrdev_region(&rbprobe_dev_number, 0, 1, DEVICE_NAME) < 0) {
			printk(KERN_DEBUG "Can't register device\n"); return -1;
	}

	/* Populate sysfs entries */
	rbprobe_dev_class = class_create(THIS_MODULE, DEVICE_NAME);

	/* Allocate memory for the per-device structure */
	rbprobe_devp = kmalloc(sizeof(struct rbprobe_dev), GFP_KERNEL);
		
	if (!rbprobe_devp) {
		printk(KERN_ALERT "Bad Kmalloc\n"); return -ENOMEM;
	}

	/* Request I/O region */
	sprintf(rbprobe_devp->name, DEVICE_NAME);

	/* Connect the file operations with the cdev */
	cdev_init(&rbprobe_devp->cdev, &rbprobe_fops);
	rbprobe_devp->cdev.owner = THIS_MODULE;

	/* Connect the major/minor number to the cdev */
	ret = cdev_add(&rbprobe_devp->cdev, (rbprobe_dev_number), 1);

	if (ret) {
		printk(KERN_ALERT "Bad cdev\n");
		return ret;
	}

	/* Send uevents to udev, so it'll create /dev nodes */
	device_create(rbprobe_dev_class, NULL, MKDEV(MAJOR(rbprobe_dev_number), 0), NULL, DEVICE_NAME);	

	/*Allocate memory for the rbprobe_trace to store the debug info */	
	rbprobe_devp->rp = kmalloc(sizeof(struct rbprobe_trace), GFP_KERNEL);

	if(!rbprobe_devp -> rp){
		printk(KERN_ALERT "Bad Kmalloc \n");
		return -ENOMEM;
	}
	
	/*Initialize the default number of probes to -1*/
	rbprobe_devp->num_probes = -1;

	/*Allocate the memory to store an array of probes dynamically */
	list_of_probes = kmalloc(sizeof(struct kprobe *), GFP_KERNEL);

	if(!list_of_probes){
		printk(KERN_ALERT "Bad Kmalloc \n");
		return -ENOMEM;
	}

	printk("rbprobe driver initialized.\n");
	return 0;
}

/* Driver Exit */
void __exit rbprobe_driver_exit(void)
{
	int i=0;
	struct kprobe *temp;
	/* Release the major number */
	unregister_chrdev_region((rbprobe_dev_number), 1);

	/* Destroy device */
	device_destroy (rbprobe_dev_class, MKDEV(MAJOR(rbprobe_dev_number), 0));
	cdev_del(&rbprobe_devp->cdev);
	kfree(rbprobe_devp);
	/*unregister all the probes */
	unregister_kprobe(*list_of_probes);
	/*Deallocate the saved memory */
	for(i = 0;i<rbprobe_devp->num_probes ;i++){
		temp = list_of_probes[i];
		kfree(temp);
		temp =NULL;
	}
	kfree(list_of_probes);
	/* Destroy driver_class */
	class_destroy(rbprobe_dev_class);

	printk("rbprobe driver removed.\n");
}

module_init(rbprobe_driver_init);
module_exit(rbprobe_driver_exit);
MODULE_LICENSE("GPL v2");
