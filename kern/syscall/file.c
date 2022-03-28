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
    // Check if it is a null value then return error
    if(file == NULL){
        *retval = -1;
        return EBADF;
    }
    // check if it is a valid fd or not
    if(fd >= OPEN_MAX || fd < 0){
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
