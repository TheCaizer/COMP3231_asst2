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
    // Open the file in the FileDescriptorTable
    struct OpenFileTable *file = *(curproc->FileDescriptorTable[fd]);
    // Check if it is a null value then return error
    if(file == NULL){
        *retval = -1;
        return EBADF;
    }
    // check if there are reference counters if so decrement
    if(file->ReferenceCounter > 0){
        file->ReferenceCounter--;
    }
    // If not then error
    else{
        *retval = -1;
        return ENOENT;
    }
    // There are no reference counter threfore we close the file
    if(file->ReferenceCounter == 0){
        vfs_close(file->vnodeptr);
        kfree(file);
        *curproc->FileDescriptorTable[fd] = NULL;
    }
    *retval = 0;
    curproc->FileDescriptorTable[fd] = NULL;
    return 0;
}

ssize_t sys_write(int fd, const void *buf, size_t nbytes, int *retval){
    //Check valid fd
    if(fd >= OPEN_MAX || fd < 0){
        *retval = -1;
        return EBADF;
    }
    // Open the file in the FileDescriptorTable
    struct OpenFileTable *file = *curproc->FileDescriptorTable[fd];
    // Check if it is a null value then return error
    if(file == NULL){
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
    uio_uinit(&iov, &u_io, buf, nbytes, file->Offset, UIO_WRITE);
    
    int res = VOP_WRITE(file->vnodeptr, &u_io);
    if(res){
        *retval = -1;
        return res;
    }
    *retval = u_io.uio_offset - file->Offset;
    file->Offset = u_io.uio_offset;

    return 0;
}

//Function to initialze the tables when the program runs
int initialize_tables(){
    *global_oft = kmalloc(OPEN_MAX * sizeof(struct OpenFileTable));
    // no memeory for the table;
    if(global_oft == NULL){
        return ENOMEM;
    }
    for(int i = 0; i < OPEN_MAX; i++){
        struct OpenFileTable *newOft;
        newOft = kmalloc(sizeof(struct OpenFileTable));
        newOft->Flag = -1;
        newOft->ReferenceCounter = 0;
        newOft->Offset = 0;
        newOft->vnodeptr = NULL;
        global_oft[i] = newOft;
    }
    
    for(int j = 0; j < OPEN_MAX;j++){
        curproc->FileDescriptorTable[j] = NULL;
    }
    //Only need to connect the stdout and stderr to console
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
