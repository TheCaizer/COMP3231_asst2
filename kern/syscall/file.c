#include <types.h>
#include <kern/errno.h>
#include <kern/fcntl.h>
#include <kern/limits.h>
#include <kern/stat.h>
#include <kern/seek.h>
#include <lib.h>
#include <uio.h>
#include <proc.h>
#include <current.h>
#include <synch.h>
#include <vfs.h>
#include <vnode.h>
#include <file.h>
#include <syscall.h>
#include <copyinout.h>

/*
 * Add your file-related functions here ...
 */

int sys_close(int fd, int *retval){
    // Open the file in the FileDescriptorTable
    struct OpenFileTable *file = curproc->FileDescriptorTable[fd];
    // check if it is a valid fd or not
    if(fd >= OPEN_MAX || fd < 0){
        *retval = -1;
        return EBADF;
    }
    if(file == NULL){
        *retval = -1;
        return EBADF;
    }
    // 
    if(file->ReferenceCounter > 0){
        file->ReferenceCounter--;
    }
    else{
        *retval = -1;
        return ENOENT;
    }
    if(file->ReferenceCounter == 0){
        vfs_close(file->vnodeptr);
        kfree(file);
        curproc->FileDescriptorTable[fd] = NULL;
    }
    *retval = 0;
    return 0;
}

//Function to initialze the tables when the program runs
int initialize_tables()[
    // checks if global_oft exist
    if(global_oft == NULL){
        global_oft = kmalloc(OPEN_MAX * sizeof(struct* OpenFileTable));
        // no memeory for the table;
        if(global_oft == NULL){
            return ENOMEM;
        }
        for(int i = 0; i < OPEN_MAX; i++){
            global_oft[i] = NULL;
        }
    }
    for(int j = 0; j < OPEN_MAX;j++){
        curproc->FileDescriptorTable[j] = NULL;
    }
    //Only need to connect the stdout and stderr to console
    char con1[5] = "cons:"
    char con2[5] = "cons:"

    global_oft[1]->flags = O_WRONLY;
    global_oft[1]->ReferenceCounter = 1;
    global_oft[1]->Offset = 0;
    vfs_open(con1, O_WRONLY, 0, &global_oft[1]);

    global_oft[2]->flags = O_WRONLY;
    global_oft[2]->ReferenceCounter = 1;
    global_oft[2]->Offset = 0;
    vfs_open(con2, O_WRONLY, 0, &global_oft[2]);
    return 0;
]
