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

int sys_open(userptr_t filename, int flags, mode_t mode, int *retval);
// global open file table
struct OpenFileTable *global_oft[OPEN_MAX];

// The functions implementation
sys_close(int fd, int *retval);


#endif /* _FILE_H_ */
