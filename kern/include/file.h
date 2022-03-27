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


#endif /* _FILE_H_ */
