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
    struct vnode * retVn;

    bool spaceFound = false;
    char *filenameStr = kmalloc(sizeof(char) * NAME_MAX);

    if(filenameStr == NULL){
        *retval = -1;
        return ENOMEM;
    }

    int err = copyinstr(filename,filenameStr,NAME_MAX, NULL);

    if(err){
        kfree(filenameStr);
        *retval = -1;
        return err;
    }
    // Looks ok vfs_open
    int res = vfs_open(filenameStr, flags, mode,&retVn);

    if(res){
        *retval = -1;
        return res;
    }
    
    int oftPos; 

    struct OpenFileTable **fd;
    // Check for a empty gloabl_oft slot
    for(int i = 0; i <OPEN_MAX; i++){
        if(global_oft[i]->vnodeptr == NULL){
            oftPos = i;
            spaceFound = true;
            global_oft[oftPos] = kmalloc(sizeof(struct OpenFileTable));
            global_oft[oftPos]->Flag = flags;
            global_oft[oftPos]->vnodeptr = retVn;
            global_oft[oftPos]->Offset = 0;
            global_oft[oftPos]->ReferenceCounter = 1;
            fd = &global_oft[oftPos];
            break;
        }
    }
    // No sapce
    if(spaceFound == false){
        vfs_close(retVn);
        *retval = -1;
        return ENFILE;
    }

    spaceFound = false;
    //Check for empty FileDescriptor Slot
    for(int j = 0; j <OPEN_MAX; j++){
        if(curproc->FileDescriptorTable[j]== NULL){
            curproc->FileDescriptorTable[j] = fd;
            *retval = j;
            spaceFound = true;
            break;
        }
    }
    if(!spaceFound){
        *retval = -1;
        vfs_close(retVn);
        kfree(*fd);
        // Close from the of table
        global_oft[oftPos]->Flag = -1;
        global_oft[oftPos]->ReferenceCounter = 0;
        global_oft[oftPos]->Offset = 0;
        global_oft[oftPos]->vnodeptr = NULL;
        kfree(global_oft[oftPos]);
        return EMFILE;
    }

    kfree(filenameStr);
    return 0;
}

int sys_close(int fd, int *retval){
    //Check valid fd
    if(fd >= OPEN_MAX || fd < 0){
        *retval = -1;
        return EBADF;
    }
    // Find the index of the current oft
    int index =  curproc->FileDescriptorTable[fd];
    // Check it is valid
    if(index >= OPEN_MAX || index < 0){
        *retval = -1;
        return EBADF;
    }
    // check if there are reference counters if so decrement
    if(global_oft[index]->ReferenceCounter > 0){
        global_oft[index]->ReferenceCounter--;
    }
    // If not then error
    else{
        *retval = -1;
        return ENOENT;
    }
    // Unassigned the fd
    curproc->FileDescriptorTable[fd] = -1;
    // There are no reference counter threfore we close the file
    if(global_oft[index]->ReferenceCounter == 0){
        vfs_close(global_oft[index]->vnodeptr);
        global_oft[index]->Offset = 0;
        global_oft[index]->vnodeptr = NULL;
        global_oft[index]->Flag = -1;
    }
    *retval = 0;
    return 0;
}

ssize_t sys_write(int fd, const void *buf, size_t nbytes, int *retval){
    //Check valid fd
    if(fd >= OPEN_MAX || fd < 0){
        *retval = -1;
        return EBADF;
    }
    // Get index
    int index =  curproc->FileDescriptorTable[fd];
    // Check it is valid
    if(index < 0 || index >= OPEN_MAX){
        *retval = -1;
        return EBADF;
    }
    // Check if you can write to it
    if(file->Flag == O_RDONLY){
        *retval = -1;
        return EBADF;
    }
    // UIO structs
    struct iovec iov;
    struct uio u_io;
    // Get the UIO
    uio_uinit(&iov, &u_io, (userptr_t) buf, nbytes, global_oft[index]->Offset, UIO_WRITE);
    
    int res = VOP_WRITE(global_oft[index]->vnodeptr, &u_io);
    if(res){
        *retval = -1;
        return res;
    }
    //return the retcal and new offset
    *retval = u_io.uio_offset - file->Offset;
    global_oft[index]->Offset = u_io.uio_offset;

    return 0;
}

//Function to initialze the tables when the program runs
int initialize_tables(void){
    *global_oft = kmalloc(OPEN_MAX * sizeof(struct OpenFileTable));
    // no memeory for the table;
    if(global_oft == NULL){
        return ENOMEM;
    }
    //initialize the tables
    for(int i = 0; i < OPEN_MAX; i++){
        global_oft[i]->Flag = -1;
        global_oft[i]->ReferenceCounter = 0;
        global_oft[i]->Offset = 0;
        global_oft[i]->vnodeptr = NULL;
    }
    //connect the stdout and stderr to console
    char con1[5] = "cons:";
    char con2[5] = "cons:";

    global_oft[1]->Flag = O_WRONLY;
    global_oft[1]->ReferenceCounter = 1;
    global_oft[1]->Offset = 0;
    vfs_open(con1, O_WRONLY, 0, &global_oft[1]->vnodeptr);

    global_oft[2]->Flag = O_WRONLY;
    global_oft[2]->ReferenceCounter = 1;
    global_oft[2]->Offset = 0;
    vfs_open(con2, O_WRONLY, 0, &global_oft[2]->vnodeptr);
    return 0;
}
