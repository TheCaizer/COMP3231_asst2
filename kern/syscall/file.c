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
int sys_open(userptr_t fileNamePtr, int flags, mode_t mode, int *retval){
    struct OpenFileTable *oft;
    struct vnode *retVn;
    int err;
    bool spaceFound = false;
    char *fileNameStr = NULL; 
    size_t fileNameLen;

    copyinstr(fileNamePtr,fileNameStr, NAME_MAX,&fileNameLen);

    err = vfs_open(fileNameStr, flags, mode,&retVn);
    if(err){
        return err;
    }

    oft = kmalloc(sizeof(struct OpenFileTable));

    oft->Flag = flags;
    oft->vnodeptr = retVn;
    oft->Offset = 0;
    oft->ReferenceCounter = retVn->vn_refcount;
    
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
        return EMFILE;
    }
    
    return 0;

}
