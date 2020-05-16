#include<fcntl.h>
#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<string.h>
#include <sys/ioctl.h>
#include<time.h>
#include<pthread.h>
#include <sched.h>
#include "rb_structures.h"

#define NUMTHREADS_EACH 2
#define WRITE_FUNCTION_NAME "rbt530_driver_write" 
#define READ_FUNCTION_NAME "rbt530_driver_read"
#define READ_OFFSET 3
#define WRITE_OFFSET 5

/*
* An util function to generate a random node to be inserted in the rb_tree
*/
rb_object_t* generateRandom(rb_object_t *node)
{
    node->key = rand()%2000;
    node->data = rand();

    return node;
}


/*
*Function to insert probes
*/
void *insertProbe(void *arg){
    struct rbprobe_init obj;
    struct rbprobe_trace rbprobe_trace;
    int rbt_file  = (int)arg;
    int count = 0,i=0;

    /*Instead of hardcoding it can be inputted from user level as well. But just to keep hold of the
     accuracy of the offset that is being fed */

    /*Inserting two probes*/

    /*Write function probe*/
    obj.function_name = WRITE_FUNCTION_NAME;
    obj.isRegisterProbe = 1;
    obj.offset = WRITE_OFFSET;
    if(write(rbt_file, &obj, sizeof(struct rbprobe_init))<0){
        printf("Probe failed to register \n");
    }

    /*read function probe*/
    obj.function_name = READ_FUNCTION_NAME;
    obj.offset = READ_OFFSET;
    obj.isRegisterProbe = 1;

    if(write(rbt_file, &obj, sizeof(struct rbprobe_init))<0){
         printf("Probe failed to register \n");
    }

    /*read function probe at same location (Multiple probes at same location scenario)*/
    obj.function_name = READ_FUNCTION_NAME;
    obj.offset = READ_OFFSET;
    obj.isRegisterProbe = 1;

    if(write(rbt_file, &obj, sizeof(struct rbprobe_init))<0){
         printf("Probe failed to register \n");
    }


    /*Infinite loop to keep check of the data that is being fed to the debug buffer */
    /*loop will terminate when the rest of the four threads completes its function and joins*/
    while(1){
        if(read(rbt_file, &rbprobe_trace, sizeof(struct rbprobe_trace)) < 0){
            printf("Error in reading the data. Value is not present.\n");
        }else{
            printf("\t\t\t\tProbe address: %p \n", rbprobe_trace.address);
            printf("\t\t\t\tProbe pid: %d \n", rbprobe_trace.pid);
            printf("\t\t\t\tProbe timestamp: %lld \n", rbprobe_trace.time_stamp);
            printf("\t\t\t\t*********Parsed Objects************\n");
            for(i=0;i<rbprobe_trace.node_count;i++){
                printf("\t\t\t\tObject Key = %d  Object Data = %d \n", rbprobe_trace.rb_object[i].key, rbprobe_trace.rb_object[i].key);
            }
        }
        sleep(2);
        count++;
    }
}
/* 
* A common function to be used by the threads, perform diffent operations in it's respective rb_tree.
*/
void *performDeviceOperations(void *arg){
    int i = 0;
    int fd = (int)arg;
    int read_val;
    rb_object_t return_object;
    int num_read_write_ops = 0;
    rb_object_t *node = (rb_object_t *)malloc(sizeof(rb_object_t));
    struct timespec tp;
    /* Seed for random number generation */
    srand(time(NULL));

    printf("Thread Created for the file %d\n", fd);
    
    /* Perform 50 WRITE operations on the tree (25 Write operations by each thread)*/
    while(i < 25){
        write(fd, generateRandom(node), sizeof(rb_object_t));
        i++;
    }
    
    /* Perform 100 Randon operations on the tree (50 Random operations by each thread)*/
    while(num_read_write_ops < 50){
        tp.tv_nsec = rand();
        tp.tv_sec = 0;
        switch(rand()%3){
            case 0:
                if(read(fd, &return_object, sizeof(rb_object_t)) < 0){
                    printf("There is no next/previous data \n");
                }else{
                    printf("Reading data from file %d : Key - %d   Data - %d\n", fd, return_object.key, return_object.data);
                }
                
                num_read_write_ops++;
                break;
            case 1:
                write(fd, generateRandom(node), sizeof(rb_object_t));
                printf("Writing data to the file %d\n", fd);
                num_read_write_ops++;
                break;
            case 2:
                printf("Performing IOCTL on file %d\n", fd);
                read_val = rand()%2;
                ioctl(fd, READ_ORDER_IOCTL, &read_val);
                if(read_val == -1){
                    printf("Invalid IOCTL argument \n");
                }
                break;
            default:
                break;
        }
        nanosleep(&tp,&tp);
    }
    printf("Thread Exiting after %d operations\n", num_read_write_ops);
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
    /*Thread array for each device. Made the number of threads customizable by declaring a macro */
    pthread_t thread_dev1[NUMTHREADS_EACH], thread_dev2[NUMTHREADS_EACH], probe_thread;
    pthread_attr_t attributes; /*Thread attribute to include schedule policy*/
    struct sched_param schedulingParams; /* scheduling paramter to impose priority on the threads*/
    int i=0, return_value; 
    int join_return_value1, join_return_value2;
    int fd1, fd2, probe_file; /* File Descriptor*/
    dump_object_t *dump_object_1 = (dump_object_t *) malloc(sizeof(dump_object_t)); /*Dump Object type for each device file */
    dump_object_t *dump_object_2 = (dump_object_t *) malloc(sizeof(dump_object_t));
    srand(time(NULL)); /*seeding */

    /* Opening the device file 1 and 2 and getting its respective firle descriptor*/
    fd1 = open("/dev/rbt530_dev1", O_RDWR);
    fd2 = open("/dev/rbt530_dev2", O_RDWR);

    if(fd1 < 0 ){
        printf("Cannot Open Device. Exiting....\n");
        return 0;
    }

    if(pthread_attr_init(&attributes)){
        printf("Error in Initializing pthread attribute \n");
        return 0;
    }

    /*Since we will be joining the thread after it's completion,  I've made the threads to be joinable.(Safe Implementation)*/
    if(pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_JOINABLE)){
        printf("Error in initializing attributes ------. \n");
        return 0;
    }
    
    /*Setting the scheduling type*/
    pthread_attr_setschedpolicy(&attributes, SCHED_RR);

   

    probe_file = open("/dev/RBprobe", O_RDWR);

    if(probe_file < 0){
        printf("Cannot Open  probe Device. Exiting....\n");
        return 0;
    }


    if(pthread_create(&probe_thread, NULL, &insertProbe, (void *)probe_file)){
        printf("Error creating thread to insert probe \n");
    }
    
    
    /*Spawning threads based on the number of threads. For Device file 1*/
    for(i=0;i<NUMTHREADS_EACH;i++){
        schedulingParams.sched_priority = rand()%99+1; /* Randomly setting a priority before creating the thread*/

        if(pthread_attr_setschedparam(&attributes, &schedulingParams)){
            printf("Error in initializing attributes. ****\n");
            return 0;
        }

        /*Creating thread with the given attirbute and scheduling policy to perform the device operations*/
        return_value = pthread_create(&thread_dev1[i], &attributes, performDeviceOperations, (void *)fd1);
        
        if(return_value){
            printf("Error in creating thread for device 1\n");
           break;
        }
        printf("Thread Created successfully for device 1\n");
    }

    if(fd2 < 0){
        printf("Cannot Open Device. Exiting....\n");
        return 0;
    }
    
    /*Spawning threads based on the number of threads. For Device file 2*/
    for(i=0;i<NUMTHREADS_EACH;i++){
        schedulingParams.sched_priority = rand()%99+1; /* Randomly setting a priority before creating the thread*/

        if(pthread_attr_setschedparam(&attributes, &schedulingParams)){
            printf("Error in initializing attributes. \n");
            return 0;
        }

        /*Creating thread with the given attirbute and scheduling policy to perform the device operations*/
        return_value = pthread_create(&thread_dev2[i], &attributes, performDeviceOperations, (void *)fd2);
        if(return_value){
            printf("Error in creating thread for device 2\n");
            break;
        }
        printf("Thread Created successfully for device 2\n");
    }

    /*Destroying the attributes once we are done utilizing it*/
    pthread_attr_destroy(&attributes);

    /*Waiting for all threads to Join*/
    for(i =0 ;i <NUMTHREADS_EACH; i++){
       join_return_value1 =  pthread_join(thread_dev1[i], NULL);
       if(join_return_value1){
           printf("Error joining the thread. Return code %d \n", join_return_value1);
           return 0;
       }
       join_return_value2 =  pthread_join(thread_dev2[i], NULL);
       if(join_return_value2){
           printf("Error joining the thread. Return code %d \n", join_return_value2);
           return 0;
       }
       printf("Joining the thread\n");
    }
   
    /* dump n number of objects. Number of objects to dump can be changed by modifying the defined values*/
    dump_object_1->n = NUM_DUMP_OBJECTS;
    dump_object_2->n = NUM_DUMP_OBJECTS;

    /*Performing IOCTL operation to dump all the objects in the tree from device 1 and displaying it*/
    ioctl(fd1, DUMP_OBJ_IOCTL, dump_object_1);

    printf("\nPrinting Dump for device 1:\n\n");
    for(i=0; i<dump_object_1->n; i++){
        printf("Key:%d  Data:%d\n", dump_object_1->object_list[i].key, dump_object_1->object_list[i].data);
    }

    /*Performing IOCTL operation to dump all the objects in the tree from device 1 and displaying it*/
    ioctl(fd2, DUMP_OBJ_IOCTL, dump_object_2); 

    printf("\nPrinting Dump for device 2:\n\n");
    for(i=0; i<dump_object_2->n; i++){
        printf("Key:%d  Data:%d\n", dump_object_2->object_list[i].key, dump_object_2->object_list[i].data);
    }

    /*Unregistering the kprobes*/


    /*Closing the device files once all the operations are performed*/
    close(fd1);
    close(fd2);
    close(probe_file);

    printf("Test program Exiting...\n");
    
    return 0;
}