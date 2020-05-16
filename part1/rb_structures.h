#include <linux/ioctl.h>

#define NUM_DUMP_OBJECTS 1000
#define READ_ORDER_IOCTL _IOWR('r',0,int)
#define DUMP_OBJ_IOCTL _IOWR('d',0, dump_object_t)

/*Struct of the rb_object */
typedef struct rb_object {
    int key;
    int data ;
} rb_object_t;


/*struct of the dump object*/

typedef struct dump_object {
    int n;
    rb_object_t object_list[NUM_DUMP_OBJECTS];
}dump_object_t;

/*
* Input structure to the program to register a probe.
*/
struct rbprobe_init{
    char *function_name; /* Function name */
    long long offset; /*Offset from the starting point of the give function */
    int isRegisterProbe; /*Flag to identify whether to register or unregister a probe at a given location */
};

/*
* Output structure that will be retured from the kernel to the user.
*/
struct rbprobe_trace{
    unsigned long long time_stamp; /*time stamp*/
    int pid; /*PID of the process*/
    void * address; /*Address where the probe hit*/
};