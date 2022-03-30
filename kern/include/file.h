/*
 * Declarations for file handle and file table management.
 */

#ifndef _FILE_H_
#define _FILE_H_

/*
 * Contains some file-related maximum length constants
 */
#include <limits.h>


/*
 * Put your function declarations and data types here ...
 */
struct OpenFileTable{
    int Flag; // The flag for the opened file
    struct vnode* vnodeptr; // Pointer to the vnode
    off_t Offset; // Number of bytes offset for the opened file 
    int ReferenceCounter; // Number of reference for this file
};
// global open file table
struct OpenFileTable *global_oft;

// The functions implementation
int sys_open(userptr_t filename, int flags, mode_t mode, int *retval);
int sys_close(int fd, int *retval);
int sys_dup2(int oldfd, int newfd, int *retval);
ssize_t sys_write(int fd, const void *buf, size_t nbytes, int *retval);
ssize_t sys_read(int fd, void *buf, size_t buflen, int *retval);



//HELPER
int initialize_tables(void);
#endif /* _FILE_H_ */
