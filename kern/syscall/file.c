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
int sys_open(const char *filename, int flags, mode_t mode, **retval){
    struct OpenFileTable *oft;
    struct vnode * retVn;
    int err;
    bool spaceFound = false;

    err = vfs_open(filename, flags, mode,retVn);
    if(err){]
        return err;
    }

    oft = kmalloc(sizeof(stuct OpenFileTable));

    oft->Flag = flags;
    oft->vnodeprt = retVn;
    oft->Offset = 0;
    oft->ReferenceCounter = retVn->vn_refcount;
    
    for(i = 0; i <OPEN_MAX); i++){
        if(curproc->FileDescriptorTable[i]== NULL){
            curproc->FileDescriptorTable[i] = oft;
            spaceFound = true;
            break;
        }
    }

    if(spaceFound == false){
        vfs_close(retVn);
        kfree(oft);
        return EMFILE;
    }
    
    return 0;

}
