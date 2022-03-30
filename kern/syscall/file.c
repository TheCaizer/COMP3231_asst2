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
ssize_t sys_read(int fd, void *buf, size_t buflen, int *retval){
    if (curproc->FileDescriptorTable[fd] == -1){
        *retval = -1;
        return EBADF;
    }

    int index = curproc->FileDescriptorTable[fd];

    struct iovec iovecRead;
    struct uio uioRead;
    
    uio_uinit(&iovecRead,&uioRead,buf,buflen,global_oft[index].Offset,UIO_READ);

    int err = VOP_READ(global_oft[index].vnodeptr, &uioRead);

    if(err){
        *retval = -1;
        return err;
    }

    global_oft[index].Offset = uioRead.uio_offset;

    *retval = buflen - uioRead.uio_resid;
    return 0;

}
int sys_open(userptr_t filename, int flags, mode_t mode, int *retval){

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
    
    int oftPos; 

  
    // Check for a empty gloabl_oft slot
    for(int i = 3; i <OPEN_MAX; i++){
        if(global_oft[i].vnodeptr == NULL){
            oftPos = i;
            spaceFound = true;
            global_oft[oftPos].Flag = flags;
            global_oft[oftPos].Offset = 0;
            global_oft[oftPos].ReferenceCounter = 1;
            i = OPEN_MAX;
        }
    }
    // No space
    if(spaceFound == false){
        *retval = -1;
        return ENFILE;
    }

    spaceFound = false;
    //Check for empty FileDescriptor Slot
    for(int j = 0; j <OPEN_MAX; j++){
        if(curproc->FileDescriptorTable[j]== -1){
            curproc->FileDescriptorTable[j] = oftPos;
            *retval = j;
            spaceFound = true;
            j = OPEN_MAX;
        }
    }
    if(spaceFound == false){
        *retval = -1;
        // Close from the of table
        global_oft[oftPos].Flag = -1;
        global_oft[oftPos].ReferenceCounter = 0;
        global_oft[oftPos].Offset = 0;
        global_oft[oftPos].vnodeptr = NULL;
        return EMFILE;
    }
    
    // Looks ok vfs_open
    int res = vfs_open(filenameStr, flags, mode, &global_oft[oftPos].vnodeptr);

    if(res){
        *retval = -1;
        return res;
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
    if(global_oft[index].ReferenceCounter > 0){
        global_oft[index].ReferenceCounter--;
    }
    // If not then error
    else{
        *retval = -1;
        return ENOENT;
    }
    // Unassigned the fd
    curproc->FileDescriptorTable[fd] = -1;
    // There are no reference counter threfore we close the file
    if(global_oft[index].ReferenceCounter == 0){
        vfs_close(global_oft[index].vnodeptr);
        global_oft[index].Offset = 0;
        global_oft[index].vnodeptr = NULL;
        global_oft[index].Flag = -1;
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
    if(global_oft[index].Flag == O_RDONLY){
        *retval = -1;
        return EBADF;
    }
    // UIO structs
    struct iovec iov;
    struct uio u_io;
    // Get the UIO
    uio_uinit(&iov, &u_io, (userptr_t) buf, nbytes, global_oft[index].Offset, UIO_WRITE);
    
    int res = VOP_WRITE(global_oft[index].vnodeptr, &u_io);
    if(res){
        *retval = -1;
        return res;
    }
    //return the retcal and new offset
    *retval = u_io.uio_offset - global_oft[index].Offset;
    global_oft[index].Offset = u_io.uio_offset;

    return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval){
    // check all the fd is valid
    if(oldfd >= OPEN_MAX || oldfd < 0){
        *retval = -1;
        return EBADF;
    }
    if(newfd >= OPEN_MAX || newfd < 0){
        *retval = -1;
        return EBADF;
    }
    // grab the index for global_oft and check if its valid
    int oldIndex = curproc->FileDescriptorTable[oldfd];
    int newIndex = curproc->FileDescriptorTable[newfd];
    if(oldIndex >= OPEN_MAX || oldIndex < 0){
        *retval = -1;
        return EBADF;
    }
    if(newIndex >= OPEN_MAX || newIndex < 0){
        *retval = -1;
        return EBADF;
    }
    // If they are are same file descriptor do nothing
    if(oldfd == newfd){
        *retval = newfd;
        return 0;
    }
    // Nothing in the old fd
    if(global_oft[oldIndex].vnodeptr == NULL){
        *retval = -1;
        return EBADF;
    }
    // Check if the new File descriptor is an opened one
    if(global_oft[newIndex].vnodeptr != NULL){
        int err = sys_close(newfd, retval);
        if(err){
            return EBADF;
        }
    }
    // assigned the new fd as the old index and then increment the reference counter
    // of the old index global_oft to show a new reference for it
    curproc->FileDescriptorTable[newfd] = oldIndex;
    global_oft[oldIndex].ReferenceCounter ++;
    *retval = newfd;
    return 0;
}

//Function to initialze the tables when the program runs
int initialize_tables(void){
    global_oft = kmalloc(OPEN_MAX * sizeof(struct OpenFileTable));
    // no memeory for the table;
    if(global_oft == NULL){
        return ENOMEM;
    }
    //initialize the tables
    for(int i = 0; i < OPEN_MAX; i++){
        global_oft[i].Flag = -1;
        global_oft[i].ReferenceCounter = 0;
        global_oft[i].Offset = 0;
        global_oft[i].vnodeptr = NULL;
    }
    //connect the stdout and stderr to console
    char conname[5];

    strcpy(conname, "cons:");
    global_oft[1].Flag = O_WRONLY;
    global_oft[1].ReferenceCounter = 1;
    global_oft[1].Offset = 0;
    vfs_open(conname, O_WRONLY, 0, &global_oft[1].vnodeptr);

    strcpy(conname, "cons:");
    global_oft[2].Flag = O_WRONLY;
    global_oft[2].ReferenceCounter = 1;
    global_oft[2].Offset = 0;
    vfs_open(conname, O_WRONLY, 0, &global_oft[2].vnodeptr);
    return 0;
}
