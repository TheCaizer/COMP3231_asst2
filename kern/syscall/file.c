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
off_t sys_lseek(int fd, off_t pos, int whence, off_t *retval){
    //Check valid fd
    if(fd >= OPEN_MAX || fd < 0){
        *retval = -1;
        return EBADF;
    }
    //Check valid value
    if(curproc->FileDescriptorTable[fd] >= OPEN_MAX || curproc->FileDescriptorTable[fd] < 0){
        *retval = -1;
        return EBADF; 
    }

    // Looks ok get the index 
    int index = curproc->FileDescriptorTable[fd];
    int err;
    struct stat vnodeStat;

    // Check there is an reference counter for the file
    if(!global_oft[index].ReferenceCounter >= 1){
        *retval = -1;
        return EPERM; 
    }
    // Check seekable
    if(!VOP_ISSEEKABLE(global_oft[index].vnodeptr)){
        *retval = -1;
        return ESPIPE;
    }

    switch (whence){
    case SEEK_SET:
        if(pos < 0 || pos > vnodeStat.st_size){
            *retval = -1;
            return EINVAL;
        }
        global_oft[index].Offset = pos;
        *retval = pos;
        return 0;

    case SEEK_CUR:
        if((global_oft[index].Offset + pos) < 0 || (global_oft[index].Offset + pos) > vnodeStat.st_size){
            *retval = -1;
            return EINVAL;
        }
        global_oft[index].Offset = global_oft[index].Offset + pos;
        *retval = global_oft[index].Offset;
        return  0;

    case SEEK_END: 
        if((vnodeStat.st_size + pos) < 0 || (vnodeStat.st_size + pos) > vnodeStat.st_size){
            *retval = -1;
            return EINVAL;
        }
        err = VOP_STAT(global_oft[index].vnodeptr, &vnodeStat);

        if(err){
            *retval = -1;
            return err;
        }
        global_oft[index].Offset = vnodeStat.st_size + pos;
        *retval = vnodeStat.st_size + pos;
        return 0;

    default:
        *retval = -1;
        return EINVAL;
    }
}

ssize_t sys_read(int fd, void *buf, size_t buflen, int *retval){
    //Check valid fd
    if(fd >= OPEN_MAX || fd < 0){
        *retval = -1;
        return EBADF;
    }
    //Check valid value
    if(curproc->FileDescriptorTable[fd] >= OPEN_MAX || curproc->FileDescriptorTable[fd] < 0){
        *retval = -1;
        return EBADF; 
    }
    
    //Get Index
    int index = curproc->FileDescriptorTable[fd];

    // Check there is an reference counter for the file
    if(!global_oft[index].ReferenceCounter >= 1){
        *retval = -1;
        return EPERM; 
    }

    // Check if you can read it
    if(global_oft[index].Flag == O_WRONLY){
        *retval = -1;
        return EBADF;
    }

    struct iovec iovecRead;
    struct uio uioRead;
    
    uio_uinit(&iovecRead,&uioRead, (userptr_t) buf,buflen,global_oft[index].Offset,UIO_READ);

    int err = VOP_READ(global_oft[index].vnodeptr, &uioRead);

    if(err){
        *retval = -1;
        return err;
    }
    // return the number of bytes read
    // set new offset
    *retval = uioRead.uio_offset - global_oft[index].Offset;
    global_oft[index].Offset = uioRead.uio_offset;
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
    for(int i = 0; i <OPEN_MAX; i++){
        if(global_oft[i].vnodeptr == NULL){
            oftPos = i;
            spaceFound = true;
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
        if(curproc->FileDescriptorTable[j] == -1){
            curproc->FileDescriptorTable[j] = oftPos;
            *retval = j;
            spaceFound = true;
            j = OPEN_MAX;
        }
    }
    if(spaceFound == false){
        *retval = -1;
        return EMFILE;
    }
    
    //Found space in global_oft and FileDescriptor table so set values
    global_oft[oftPos].Flag = flags;
    global_oft[oftPos].Offset = 0;
    global_oft[oftPos].ReferenceCounter = 1;

    // Looks ok vfs_open to set vnodeptr
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
    //Check valid value
    if(curproc->FileDescriptorTable[fd] >= OPEN_MAX || curproc->FileDescriptorTable[fd] < 0){
        *retval = -1;
        return EBADF; 
    }

    // Looks ok get the index
    int index =  curproc->FileDescriptorTable[fd];

    // check if there are reference counters if so decrement
    if(global_oft[index].ReferenceCounter > 0){
        global_oft[index].ReferenceCounter--;
    }
    // If not then error
    else{
        *retval = -1;
        return EPERM;
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
    //Check valid value
    if(curproc->FileDescriptorTable[fd] >= OPEN_MAX || curproc->FileDescriptorTable[fd] < 0){
        *retval = -1;
        return EBADF; 
    }
    // OK fd then Get index
    int index =  curproc->FileDescriptorTable[fd];

    // Check there is an reference counter for the file
    if(!global_oft[index].ReferenceCounter >= 1){
        *retval = -1;
        return EPERM; 
    }

    // Check if you can write to it
    if(global_oft[index].Flag == O_RDONLY){
        *retval = -1;
        return EBADF;
    }

    // UIO structs
    struct iovec iovecWrite;
    struct uio uioWrite;
    
    // Get the UIO
    uio_uinit(&iovecWrite, &uioWrite, (userptr_t) buf, nbytes, global_oft[index].Offset, UIO_WRITE);
    
    int res = VOP_WRITE(global_oft[index].vnodeptr, &uioWrite);

    if(res){
        *retval = -1;
        return res;
    }

    //return the retval of the number of bytes it written and set new offset
    *retval = uioWrite.uio_offset - global_oft[index].Offset;
    global_oft[index].Offset = uioWrite.uio_offset;

    return 0;
}

int sys_dup2(int oldfd, int newfd, int *retval){
    // check all the fd is valid
    if(oldfd >= OPEN_MAX || oldfd < 0 || newfd >= OPEN_MAX || newfd < 0){
        *retval = -1;
        return EBADF;
    }

    // If they are are same file descriptor do nothing
    if(oldfd == newfd){
        *retval = newfd;
        return 0;
    }

    // get the index for global_oft and check if its valid
    int newIndex = curproc->FileDescriptorTable[newfd];
    int oldIndex = curproc->FileDescriptorTable[oldfd];

    if(oldIndex >= OPEN_MAX || oldIndex < 0 || newIndex >= OPEN_MAX || newIndex < 0){
        *retval = -1;
        return EBADF;
    }

    // Nothing in the old fd
    if(global_oft[oldIndex].vnodeptr == NULL){
        *retval = -1;
        return EBADF;
    }

    // Check if the new File descriptor is an opened one
    if(global_oft[newIndex].vnodeptr != NULL){
        sys_close(newfd, retval);
        //Check the sysclose succeded
        if(*retval != 0){
            return EBADF;
        }
    }

    // assigned the new fd as the old index and increment the reference counter
    // of the old index global_oft to show a new reference for it
    *retval = newfd;
    global_oft[oldIndex].ReferenceCounter++;
    curproc->FileDescriptorTable[newfd] = oldIndex;

    return 0;
}

//Function to initialze the tables when the program runs
int initialize_tables(void){
    // Check if the global_oft exist
    if(global_oft == NULL){
        global_oft = kmalloc(OPEN_MAX * sizeof(struct OpenFileTable));
        // no memeory for the table;
        if(global_oft == NULL){
            return ENOMEM;
        }
        //initialize the global_oft tables
        for(int i = 0; i < OPEN_MAX; i++){
            global_oft[i].Flag = -1;
            global_oft[i].ReferenceCounter = 0;
            global_oft[i].Offset = 0;
            global_oft[i].vnodeptr = NULL;
        }
    }

    //connect the stdout and stderr to console
    char conname[5];

    strcpy(conname, "con:");
    global_oft[0].Flag = O_RDONLY;
    global_oft[0].ReferenceCounter = 1;
    global_oft[0].Offset = 0;
    vfs_open(conname, O_RDONLY, 0, &global_oft[0].vnodeptr);

    strcpy(conname, "con:");
    global_oft[1].Flag = O_WRONLY;
    global_oft[1].ReferenceCounter = 1;
    global_oft[1].Offset = 0;
    vfs_open(conname, O_WRONLY, 0, &global_oft[1].vnodeptr);

    strcpy(conname, "con:");
    global_oft[2].Flag = O_WRONLY;
    global_oft[2].ReferenceCounter = 1;
    global_oft[2].Offset = 0;
    vfs_open(conname, O_WRONLY, 0, &global_oft[2].vnodeptr);
    
    return 0;
}
