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
#include <linux/init.h>
#include <linux/moduleparam.h>
#include "rbtree_rbt530.h"

#define DEVICE_NAME    "rbt530_dev"  // device name to be created and registered
#define NUM_DEVICES    2 //Number of device files to be created

/* per device structure */
struct rbt530_dev {
	struct cdev cdev; /* Character device structure */
	char name[20];    /* Name of the device */         
	struct rb_root rbt530; /*Holds the root of the rb_tree*/
    struct rb_node *current_node; /*Points to the current node which is being read*/
	int read_order; /*A flag to check if the read_order must by ascending or descending.*/
} *rbt530_devp[NUM_DEVICES];

static dev_t rbt530_dev_number; /*A commom device number for all the devices*/
struct class *rbt530_dev_class; /*A commom device class for all the devices*/

static char *user_name = "";  /*Track the user who installed the given driver*/

DEFINE_MUTEX(lock);

module_param(user_name,charp,0000);	/*Read the user_name from command line argument */

/*
* Function to open the given device file.
*/
int rbt530_driver_open(struct inode *inode, struct file *file)
{
    struct rbt530_dev *rbt530_devp;
    rbt530_devp = container_of(inode->i_cdev, struct rbt530_dev, cdev);

    file->private_data  = rbt530_devp;
    printk("\n%s is opening \n", rbt530_devp->name);
    return 0;
}

/*
* Function to release the given device file once all the operations are done.
*/
int rbt530_driver_release(struct inode *inode, struct file *file)
{
    struct rbt530_dev *rbt530_devp = file->private_data;

    printk("\n%s is closing \n", rbt530_devp->name);
    return 0;
}

/*
*Fucntion to perfom write operation in the rb_tree of the device
*/
ssize_t rbt530_driver_write(struct file *file, const char *buf, size_t count, loff_t *ppos)
{
    struct rb_node *node; 
    struct tree_node *rb = kmalloc(sizeof(struct tree_node), GFP_KERNEL);
    rb_object_t rb_object;
    struct rbt530_dev *rbt530_devp = file->private_data;

    mutex_lock(&lock);

    copy_from_user(&rb_object, (rb_object_t *)buf, sizeof(rb_object_t)); /*Copy the input arguments from the user space*/
    
    /*If the data is 0, erase the node. If not insert the node into the tree*/
    if(rb_object.data == 0){
        node = search_tree(&rbt530_devp->rbt530, rb->rb_object.key);
        /*Check if the node of given is present. If not preset, do nothing and return 0*/
        if(node){
            if(rb_entry(rbt530_devp->current_node, struct tree_node, node)->rb_object.key ==
                rb_entry(node, struct tree_node, node)->rb_object.key){
                    if(rbt530_devp->read_order == 0){
                        rbt530_devp->current_node = rb_next(rbt530_devp->current_node);
                    }else{
                        rbt530_devp->current_node = rb_prev(rbt530_devp->current_node);
                    }
                }
            rb_erase(node, &rbt530_devp->rbt530); //Erase the node of given key from the tree
        }
    }else{
        rb->rb_object.data = rb_object.data; 
        rb->rb_object.key  = rb_object.key;

        insert_node(&rbt530_devp->rbt530, rb);

        /* base case, where current_node will be null. Initialize it to the first node inserted */
        if(rbt530_devp->current_node == NULL){
            rbt530_devp->current_node = rbt530_devp->rbt530.rb_node;
        }
    }
    mutex_unlock(&lock);
    return 0;
}

/*
* Function the send the data from the device file to user space.
*/
ssize_t rbt530_driver_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    struct rb_node *node; 
    struct rbt530_dev *rbt530_devp = file -> private_data;
    rb_object_t object;
    mutex_lock(&lock);
    if(!rbt530_devp->rbt530.rb_node)
    {
        mutex_unlock(&lock);
        return -EINVAL;
    }

    if(rbt530_devp->current_node == NULL){
        if(rbt530_devp->read_order == 0){
            node = rb_first(&rbt530_devp->rbt530);
        }else{
            node = rb_last(&rbt530_devp->rbt530);
        }
    }else{
        if(rbt530_devp->read_order == 0){
            node = rb_next(rbt530_devp->current_node);
        }else {
            node = rb_prev(rbt530_devp->current_node);
        }
    }

    if(node){
        rbt530_devp->current_node = node;
        object.key = rb_entry(node, struct tree_node, node)->rb_object.key;
        object.data = rb_entry(node, struct tree_node, node)->rb_object.data;
        copy_to_user((rb_object_t *)buf, &object, sizeof(rb_object_t));
        
        mutex_unlock(&lock); //Unlock once the operation is done before returning the function.

        return sizeof(rb_object_t);
    }

    printk(KERN_ALERT "Node you are trying to read is NULL\n");
    mutex_unlock(&lock);

    return -EINVAL;
}

static long rbt530_driver_ioctl(struct file *file, unsigned int cmd, unsigned long arg){
    struct rbt530_dev *rbt530_devp = file -> private_data;
    dump_object_t *dump;
    rb_object_t object;
    struct rb_node *node;
    int *order = (int *)arg;
    int i=0;

    mutex_lock(&lock);

    switch(cmd){
        case READ_ORDER_IOCTL:
            if(*order == 0){
                rbt530_devp->current_node = rb_first(&rbt530_devp->rbt530);
                copy_to_user(&arg, &order, sizeof(int));
            }else if(*order == 1){
                rbt530_devp->current_node = rb_last(&rbt530_devp->rbt530);
                copy_to_user(&arg, &order, sizeof(int));
            }else{
                printk(KERN_ALERT "Invalid params. Exiting...\n");
                mutex_unlock(&lock);
                return -EINVAL;
            }
            break;
        case DUMP_OBJ_IOCTL:
            dump = (dump_object_t *)arg;
            for (node = rb_first(&rbt530_devp->rbt530); node; node = rb_next(node)){
                object = rb_entry(node, struct tree_node, node)->rb_object;
                copy_to_user(&(dump)->object_list[i], &object, sizeof(rb_object_t));           
                i++;
            }
            copy_to_user(&(dump)->n, &i, sizeof(int)); 
            break;
        default:
            break;
    }
    mutex_unlock(&lock);
    return 0; 
}

static struct file_operations rbt530_fops = {
    .owner          = THIS_MODULE,
    .open           = rbt530_driver_open,
    .release        = rbt530_driver_release,
    .write          = rbt530_driver_write,
    .read           = rbt530_driver_read,
    .unlocked_ioctl = rbt530_driver_ioctl
};

int __init rbt530_driver_init(void)
{
    int ret;
    int i;

    if(alloc_chrdev_region(&rbt530_dev_number, 0, NUM_DEVICES, DEVICE_NAME) < 0){
        printk(KERN_DEBUG "Can't register device\n");
        return -1;
    }

    rbt530_dev_class = class_create(THIS_MODULE, DEVICE_NAME);
    for(i=0;i<NUM_DEVICES;i++){
        
        rbt530_devp[i] = kmalloc(sizeof(struct rbt530_dev), GFP_KERNEL);
        if(!rbt530_devp[i]) {
            printk("Bad Kmalloc \n");
            return -ENOMEM;
        }

        sprintf(rbt530_devp[i]->name, "%s%d",DEVICE_NAME,i+1);

        cdev_init(&rbt530_devp[i]->cdev, &rbt530_fops);
        rbt530_devp[i]->cdev.owner = THIS_MODULE;

        ret = cdev_add(&rbt530_devp[i]->cdev, (MKDEV(MAJOR(rbt530_dev_number), i)), 1);

        if(ret < 0) {
            printk("Bad cdev \n");
            return ret;
        }

        device_create(rbt530_dev_class, NULL, MKDEV(MAJOR(rbt530_dev_number),i), NULL, "%s%d",DEVICE_NAME,i+1);

        (rbt530_devp[i])->rbt530 = RB_ROOT; 
        (rbt530_devp[i])->current_node = NULL;
        (rbt530_devp[i])->read_order = 1;
    }
    printk("rbt530 drive initialized. \n");
    
    return 0;
}

void __exit rbt530_driver_exit(void)
{
    struct rb_node *node, *temp;
    int i=0;
    
    /*Free the memory for each device created */
    for(i=0;i<NUM_DEVICES;i++){
        /*Loop over the tree and deallocate memory for each node */
        for (node = rb_first(&rbt530_devp[i]->rbt530); node; node = rb_next(node))
	    {
            temp  =  node;
            rb_erase(temp,&rbt530_devp[i]->rbt530);
        }
        
        /*Delete the character device */
        cdev_del(&rbt530_devp[i]->cdev);

        /*Destroy the created devices */
        device_destroy(
, MKDEV(MAJOR(rbt530_dev_number),i));
        
        /*Free the allocated memory for each device */
        kfree(rbt530_devp[i]);
    }

    /*Unregister device from the kernel */
    unregister_chrdev_region((rbt530_dev_number), NUM_DEVICES);

    /*Destroy the created device class */
    class_destroy(rbt530_dev_class);

    printk(KERN_ALERT "rbt_530 driver removed \n");
}

EXPORT_SYMBOL(rbt530_driver_write);
EXPORT_SYMBOL(rbt530_driver_read);

module_init(rbt530_driver_init);
module_exit(rbt530_driver_exit);
MODULE_LICENSE("GPL v2");
