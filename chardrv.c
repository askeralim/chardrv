/**
 * @file   chardev.c
 * @author Askerali M
 * @date   2 january 2017
 * @version 0.1
 * @brief   An introductory character driver to support only the read operation, 
 * which reads the entire process in the system and list it while calling cat /dev/chardev CATthe second article of my series on
 * Linux loadable kernel module (LKM) development. This module maps to /dev/chardev
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <asm/uaccess.h>          // Required for the copy to user function
#include <linux/miscdevice.h>
#include <linux/kmod.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/string.h>

#define  DEVICE_NAME "chardrv"    ///< The device will appear at /dev/chardev using this value
#define  CLASS_NAME  "chardrv"        ///< The device class -- this is a character device driver
#define BUF_LEN 8000            /* Max length of the message from the device */

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Askerali M");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A simple read only Linux char driver System processes");  ///< The description -- see modinfo
MODULE_VERSION("0.1");            ///< A version number to inform users

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static short  size_of_message;              ///< Used to remember the size of the string stored
static struct class*  chardevClass  = NULL; ///< The device-driver class struct pointer
static struct device* chardevDevice = NULL; ///< The device-driver device struct pointer
static char msg[BUF_LEN];    /* The msg the device will give when asked    */

// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init chardev_init(void){
   // Try to dynamically allocate a major number for the device -- more difficult but worth it
   majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
   if (majorNumber<0){
      printk(KERN_ALERT "CharDrv failed to register a major number\n");
      return majorNumber;
   }
   printk(KERN_INFO "CharDrv: registered correctly with major number %d\n", majorNumber);

   // Register the device class
   chardevClass = class_create(THIS_MODULE, CLASS_NAME);
   if (IS_ERR(chardevClass)){                // Check for error and clean up if there is
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to register device class\n");
      return PTR_ERR(chardevClass);          // Correct way to return an error on a pointer
   }
   printk(KERN_INFO "CharDrv: device class registered correctly\n");

   // Register the device driver
   chardevDevice = device_create(chardevClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
   if (IS_ERR(chardevDevice)){               // Clean up if there is an error
      class_destroy(chardevClass);           // Repeated code but the alternative is goto statements
      unregister_chrdev(majorNumber, DEVICE_NAME);
      printk(KERN_ALERT "Failed to create the device\n");
      return PTR_ERR(chardevDevice);
   }
   printk(KERN_INFO "CharDrv: device class created correctly\n"); // Made it! device was initialized
   return 0;
}

/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit chardev_exit(void){
   device_destroy(chardevClass, MKDEV(majorNumber, 0));     // remove the device
   class_unregister(chardevClass);                          // unregister the device class
   class_destroy(chardevClass);                             // remove the device class
   unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
   printk(KERN_INFO "CharDrv: Goodbye from the LKM!\n");
}

/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
	struct task_struct *task;   
	for_each_process(task) {
		sprintf(msg + strlen(msg), "%s %d \n", task->comm, task->pid);
	}
   return 0;
}

/** @brief This function is called whenever device is being read from user space i.e. data is
 *  being sent from the device to the user. In this case is uses the copy_to_user() function to
 *  send the buffer string to the user and captures any errors.
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 *  @param buffer The pointer to the buffer to which this function writes the data
 *  @param len The length of the b
 *  @param offset The offset if required
 */
static ssize_t dev_read(struct file *filep, char *buffer, size_t length, loff_t *offset){

ssize_t bytes = length < (BUF_LEN-(*offset)) ? length : (BUF_LEN-(*offset));
    printk(KERN_ALERT "in dev_read\n");
    if(copy_to_user(buffer, msg, bytes)){
        return -EFAULT;
    }
    (*offset) += bytes;
    return bytes;
}

/** @brief This function is called whenever the device is being written to from user space i.e.
 *  data is sent to the device from the user. The data is copied to the message[] array in this
 *  LKM using the sprintf() function along with the length of the string.
 *  @param filep A pointer to a file object
 *  @param buffer The buffer to that contains the string to write to the device
 *  @param len The length of the array of data that is being passed in the const char buffer
 *  @param offset The offset if required
 */
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset){
     printk(KERN_INFO "CharDrv: The functionality is not supported, It is a Read only Device");
   return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
   printk(KERN_INFO "CharDrv: Device successfully closed\n");
   return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(chardev_init);
module_exit(chardev_exit);
