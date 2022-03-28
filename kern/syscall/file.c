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
int sys_open(const userptr_t filename, int flags, mode_t mode, int *retval){
    struct OpenFileTable *oft;
    struct vnode * retVn;

    bool spaceFound = false;
    char *filenameStr;

    int err = copyinstr(filename,filenameStr,NAME_MAX, NULL);

    if(err){
        return err;
        *retval = -1;
    }

    err = vfs_open(filenameStr, flags, mode,&retVn);
    
    oft = kmalloc(sizeof(struct OpenFileTable));

    oft->Flag = flags;
    oft->vnodeptr = retVn;
    oft->Offset = 0;
    oft->ReferenceCounter = retVn->vn_refcount;
    
    int oftPos; 

    for(int i = 0; i <OPEN_MAX; i++){
        if(global_oft[i]== NULL){
            global_oft[i] = oft;
            spaceFound = true;
            oftPos = i;
            i = OPEN_MAX;
        }
    }

    for(int i = 0; i <OPEN_MAX; i++){
        if(curproc->FileDescriptorTable[i]== NULL){
            curproc->FileDescriptorTable[i] = oft;
            *retval = i;
            spaceFound = true;
            i = OPEN_MAX;
        }
    }

    if(spaceFound == false){
        vfs_close(retVn);
        kfree(oft);
        *retval = -1;
        return EMFILE;
    }
    

    for(int i = 0; i <OPEN_MAX; i++){
        if(curproc->FileDescriptorTable[i]== NULL){
            curproc->FileDescriptorTable[i] = &global_oft[oftPos];
            spaceFound = true;
            break;
        }
    }

    if(spaceFound == false){
        vfs_close(retVn);
        kfree(oft);
        *retval = -1;
        return EMFILE;
    }

    *retval = oftPos;
    return 0;
}



int sys_close(int fd, int *retval){
    struct OpenFileTable *file = curproc->FileDescriptorTable[fd];
    if(file == NULL){
        *retval = -1;
        return EBADF;
    }
    // check if it is a valid fd or not
    if(fd >= OPEN_MAX || fd < 0){
        *retval = -1;
        return EBADF;
    }
    if(file->ReferenceCounter > 0){
        file->ReferenceCounter--;
    }
    if(file->ReferenceCounter == 0){
        vfs_close(file->vnodeptr);
        kfree(file);
        curproc->FileDescriptorTable[fd] == NULL;
    }
    *retval = 0;
    return 0;
}
