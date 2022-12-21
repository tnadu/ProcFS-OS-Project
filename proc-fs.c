#define FUSE_USE_VERSION 31

#include </usr/include/fuse3/fuse.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>


// struct with which the process
// tree structure will be stored
typedef struct proc {
    int PID, numberOfChildren;
    struct proc *children[201];
    char *status;
} process;

process *rootOfFS;
time_t timeOfMount;


// DESCRIPTION:
//      Recursive function which reconstructs the tree structure of
// running processes at mount-time in a DFS manner. 'currentProcess'
// holds the process which is currently being visited.
//
// CALLING METHOD:
//      Memory for a Proc struct must be allocated beforehand. In order
// to reconstruct a specific subtree, that subtree's root node's PID
// must be specified in the PID field of the newly allocated struct.
// In order to reconstruct the whole process tree, '1' will be stored
// in said field. When the function returns, the subtree can be accessed
// via the provided pointer.
//
// RETURN VALUES:
// - 0 -> success
// - n > 0 -> PID of the process at which the function encountered an error 
int constructTreeOfProcesses(process *currentProcess) {
    char convertedPID[10];  // necessary when appending to paths

    // verify memory allocation was successful
    if ((currentProcess->status = malloc(sizeof(char) * 8096)) == NULL) {
        return currentProcess->PID;
    }

    // construct the 'status' file's path for the current process
    char *statusPath;
    // verify memory allocation was successful
    if ((statusPath = malloc(sizeof (char) * 42)) == NULL)
        return currentProcess->PID;

    // verify no errors were encountered while converting integer
    if (sprintf(convertedPID, "%d", currentProcess->PID) < 0)
        return currentProcess->PID;

    // unless sprintf() and malloc fail, it is guaranteed that there is
    // enough memory in 'statusPath' for all the string operations bellow
    strcpy(statusPath, "/proc/");
    strcat(statusPath, convertedPID);
    strcat(statusPath, "/status");
    
    int statusFileDescriptor;
    // verify no errors were encountered when opening file
    if ((statusFileDescriptor = open(statusPath, O_RDONLY)) == -1)
        return currentProcess->PID;

    size_t offset = 0, bytesRead;
    // storing content of the 'status' file into the corresponding process's field;
    // 1KB will be read at a time
    while ((bytesRead = read(statusFileDescriptor, currentProcess->status + offset, 1024)) > 0 && offset < 8096)
        offset += bytesRead;

    // verify no errors were encountered while reading file
    if (bytesRead == -1)
        return currentProcess->PID;

    // verify no errors were encountered while closing file
    if (close(statusFileDescriptor) == -1)
        return currentProcess->PID;


    // construct the 'children' file's path for the current process
    char *childrenReadBuffer;
    // verify memory allocation was successful
    if ((childrenReadBuffer = malloc(sizeof(char) * 8096)) == NULL)
        return currentProcess->PID;
    
    char *childrenPath = statusPath;    // reusing memory allocated for the previous path
    // verify no errors were encountered
    if (sprintf(convertedPID, "%d", currentProcess->PID) < 0)
        return currentProcess->PID;

    // unless sprintf() and malloc fail, it is guaranteed that there is
    // enough memory in 'childrenPath' for all string operations bellow
    strcpy(childrenPath, "/proc/");
    strcat(childrenPath, convertedPID);
    strcat(childrenPath, "/task/");
    strcat(childrenPath, convertedPID);
    strcat(childrenPath, "/children");

    int childrenFileDescriptor;
    // verify no errors were encountered when opening file
    if ((childrenFileDescriptor = open(childrenPath, O_RDONLY)) == -1)
        return currentProcess->PID;

    offset = 0;
    // storing content of the 'children' file into the corresponding buffer;
    // 1KB will be read at a time
    while ((bytesRead = read(childrenFileDescriptor, childrenReadBuffer + offset, 1024)) > 0 && offset < 8096)
        offset += bytesRead;

    // verify no errors were encountered while reading file
    if (bytesRead == -1)
        return currentProcess->PID;

    // verify no errors were encountered while closing file
    if (close(childrenFileDescriptor) == -1)
        return currentProcess->PID;

    free(childrenPath);


    // allocating memory for every PID found in the 'children' file
    char *childProcess = strtok(childrenReadBuffer, " ");
    currentProcess->numberOfChildren = 0;
    while (childProcess != NULL) {
        // verify memory allocation was successful
        if ((currentProcess->children[currentProcess->numberOfChildren] = malloc(sizeof(process))) == NULL)
            return currentProcess->PID;
        currentProcess->children[currentProcess->numberOfChildren]->PID = atoi(childProcess);

        currentProcess->numberOfChildren++;
        childProcess = strtok(NULL, " ");
    }

    free(childrenReadBuffer);


    // building process subtrees by recursively calling function upon all the current process's children
    int subtreeConstructionStatus;
    for (int i = 0; i < currentProcess->numberOfChildren; i++) {
        subtreeConstructionStatus = constructTreeOfProcesses(currentProcess->children[i]);

        // verify no errors were encountered while building subtrees
        if (subtreeConstructionStatus != 0)
            return subtreeConstructionStatus;
    }

    return 0;
}



// DESCRIPTION
// 'requestedProcess' holds:
//  - the pointer to the specified process, if the path is valid
//  - NULL, otherwise
//
// RETURN VALUES:
//  - '-1' -> invalid path
//  - '0' -> a process's dir was requested
//  - '1' -> the 'stats' file of a process was requested
int getProcess(char *path, process **requestedProcess) {
    // all paths must begin from the root of the file system
    if (*path != '/') {
        *requestedProcess = NULL;
        return -1;
    }

    // used in traversing the items delimited by '/'
    char *currentItemInPath = strtok(path, "/");    // omitting the empty char

    // the provided path is the root of the FS
    if (currentItemInPath == NULL) {
        *requestedProcess = rootOfFS;
        return 0;
    }

    *requestedProcess = rootOfFS;    // points to the root of the FS (PID = -1)


    // according to the signature of the 'strtol' function, a char pointer is used in
    // the case where an invalid character for the provided numerical base is encountered
    char *firstNonDigit;

    // the root process's PID is always equal to 1 (systemd - linux);
    // even when 1 is returned as the result of the conversion of the first item to 
    // a numerical value, it might still be possible that invalid characters where
    // encountered, which is why the provided char pointer must be either NULL or the null char
    if (strtol(currentItemInPath, &firstNonDigit, 0) != 1 && firstNonDigit != NULL && *firstNonDigit != '\0') {
        // the 'README' file was requested
        if (strcmp(currentItemInPath, "README") == 0)
            return 1;   // corresponding return status

        *requestedProcess = NULL;
        return -1;
    }

    *requestedProcess = rootOfFS->children[0];    // points to the root process (PID = 1)


    int statsFound = 0;     // used to mark that the 'stats' file was encountered in the path
    long currentPID;        // the PID of the process being searched for at every level of the process tree

    // parsing the remaining path and the corresponding process tree path
    for (currentItemInPath = strtok(NULL, "/"); currentItemInPath != NULL; currentItemInPath = strtok(NULL, "/")) {
        // 'strtol' returns the result of the conversion to a numerical value of the current item in the path
        currentPID = strtol(currentItemInPath, &firstNonDigit, 0);
        // verifying that no invalid characters were encountered
        if (firstNonDigit != NULL && *firstNonDigit != '\0') {
            // the stats file was requested in the path
            if (strcmp(currentItemInPath, "stats") == 0) {
                statsFound = 1;
                break;
            }
            // invalid path
            else {
                *requestedProcess = NULL;
                return -1;
            }
        }

        // searching the children of the current process for the child with the specified PID 
        int i, numberOfChildrenOfTheCurrentProcess = (*requestedProcess)->numberOfChildren;

        for (i = 0; i < numberOfChildrenOfTheCurrentProcess; i++)
            // specified process was found
            if ((*requestedProcess)->children[i]->PID == currentPID) {
                *requestedProcess = (*requestedProcess)->children[i];
                break;
            }

        // no children with the specified PID could be found
        if (i == numberOfChildrenOfTheCurrentProcess) {
            *requestedProcess = NULL;
            return -1;
        }
    }


    // 'stats' file found in path
    if (statsFound == 1) {

        if (strtok(NULL, "/") != NULL) {
            *requestedProcess = NULL;
            return -1;
        }

        return 1;   // 'stats' file requested in the specified path
    }

    return 0;   // process directory requested in the specified path
}



static int p_getattr(const char *path, struct stat *status, struct fuse_file_info *fi) {
    (void) fi;
    memset(status, 0, sizeof(struct stat));

    // in order to avoid discarding the 'const' qualifier when passing arguments to string
    // functions used in getProcess(), a non const copy of the provided path will be made
    char *nonConstPath;
    // verify memory allocation was successful
    if ((nonConstPath = malloc(sizeof(char) * strlen(path))) == NULL)
        return ENOMEM;
    strcpy(nonConstPath, path);

    // searching the corresponding process struct of the provided path
    process *requestedProcess;
    int returnStatus = getProcess(nonConstPath, &requestedProcess);
    free(nonConstPath);


    // specified path is invalid
    if (requestedProcess == NULL)
        return EINVAL;

    // common properties of files and directories
    // the owner of the file/directory is the user who mounted the filesystem
    status->st_uid = getuid();
    // the group of the file/directory is that of the group of the user who mounted the filesystem
    status->st_gid = getgid();
    // the mount-time will be considered as both the access and the last-modified time
    status->st_atime = timeOfMount;
    status->st_mtime = timeOfMount;

    // provided path references a directory
    if (returnStatus == 0)
    {
        // specifying that the path is that of a directory and
        // setting the permissions to RX-RX-RX
        status->st_mode = S_IFDIR | 0555;
        status->st_nlink = 2;   // both the current directory and its parent point to itself
    }
    // provided path references a file
    else {
        // specifying that the path is that of a file and
        // setting the permission to R-R-R
        status->st_mode = S_IFREG | 0444;
        status->st_nlink = 1;   // only the parent directory points to the current file
        status->st_size = strlen(requestedProcess->status);
    }

    return 0;
}



static int p_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fileInfo;
    (void) flags;

    // in order to avoid discarding the 'const' qualifier when passing arguments to string
    // functions used in getProcess(), a non const copy of the provided path will be made
    char *nonConstPath;
    // verify memory allocation was successful
    if ((nonConstPath = malloc(sizeof(char) * strlen(path))) == NULL)
        return ENOMEM;
    strcpy(nonConstPath, path);

    // searching the corresponding process struct of the provided path
    process *requestedProcess;
    int returnStatus = getProcess(nonConstPath, &requestedProcess);
    free(nonConstPath);

    // operation is only permitted on directories
    if (returnStatus != 0)
        return EINVAL;

    char PID[10];       // needed when passing PIDs to filler() 

    filler(buffer, ".", NULL, 0, 0);
    filler(buffer, "..", NULL, 0, 0);

    // listing all the current process's children
    for (int i = 0; i < requestedProcess->numberOfChildren; i++) {
        sprintf(PID, "%d", requestedProcess->children[i]->PID);
        filler(buffer, PID, NULL, 0, 0);
    }

    // the root of the filesystem is not a process,
    // therefore it doesn't contain a 'stats' file
    if (requestedProcess->PID != -1)
        filler(buffer, "stats", NULL, 0, 0);
    // it does, however, contain a 'README' file
    else
        filler(buffer, "README", NULL, 0, 0);

    return 0;
}


static int p_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    (void) fileInfo;

    // in order to avoid discarding the 'const' qualifier when passing arguments to string
    // functions used in getProcess(), a non const copy of the provided path will be made
    char *nonConstPath;
    // verify memory allocation was successful
    if ((nonConstPath = malloc(sizeof(char) * strlen(path))) == NULL)
        return ENOMEM;
    strcpy(nonConstPath, path);

    // searching the corresponding process struct of the provided path
    process *requestedProcess;
    int returnStatus = getProcess(nonConstPath, &requestedProcess);
    free(nonConstPath);


    // operation is permitted only on files
    if (returnStatus != 1)
        return EINVAL;

    // when offset is at or after the end of file or when size is 0,
    // the specification of 'read' mentions that '0' shall be returned
    if (strlen(requestedProcess->status) <= offset || size == 0)
        return 0;

    // the specification of 'read' mentions that when the sizes
    // of the offset and the number-of-bytes-to-be-copied is larger
    // than SSIZE_MAX, the behaviour is implementation-defined;
    // therefore, I made sure the maximum amount of bytes which
    // can be copied is precisely that constant
    offset = (offset < SSIZE_MAX) ? offset : SSIZE_MAX;
    size = (size < SSIZE_MAX) ? size : SSIZE_MAX;

    // the number of bytes which can be copied cannot exceed the total size
    // of the source - the size of the offset;
    size_t numberOfBytesCopied = (size < strlen(requestedProcess->status) - offset) ? size :
                                 strlen(requestedProcess->status) - offset;

    memcpy(buffer, requestedProcess->status + offset, numberOfBytesCopied);

    // 'read' must return the total number of successfully copied bytes
    return numberOfBytesCopied;
}



// loading implemented functions
static struct fuse_operations implementedOperations = {
        .getattr = p_getattr,
        .readdir = p_readdir,
        .read = p_read
};


int main(int argc, char **argv) {
    timeOfMount = time(NULL);   // storing the mount-time for future reference in getattr()

    // initializing file system root
    // verifying that the memory allocation was successful
    if ((rootOfFS = malloc(sizeof(process))) == NULL)
        return ENOMEM;

    rootOfFS->PID = -1;     // convention to indicate that the current struct is the root of the file system
    rootOfFS->numberOfChildren = 1;     // its only child is the systemd process
    // the content of 'README'
    rootOfFS->status = "██████╗ ██████╗  ██████╗  ██████╗███████╗███████╗\n"
                       "██╔══██╗██╔══██╗██╔═══██╗██╔════╝██╔════╝██╔════╝\n"
                       "██████╔╝██████╔╝██║   ██║██║     █████╗  ███████╗\n"
                       "██╔═══╝ ██╔══██╗██║   ██║██║     ██╔══╝  ╚════██║\n"
                       "██║     ██║  ██║╚██████╔╝╚██████╗██║     ███████║\n"
                       "╚═╝     ╚═╝  ╚═╝ ╚═════╝  ╚═════╝╚═╝     ╚══════╝\n"
                       "                                                 \n"
                       "\tProcFS is a fuse-based file system which aims\n"
                       "to provide a snapshot of the running processes at\n"
                       "mount-time. Unlike /proc, the directory structure\n"
                       "of ProcFS maintains the process tree structure.\n"
                       "Each directory represents a process and contains\n"
                       "a 'stats' file with relevant data about the status\n"
                       "of the corresponding process.\n\n"
                       "\tProcFS was assigned as a lab project for the Operating\n"
                       "Systems course of the University of Bucharest and can be\n"
                       "used as desired. More information can be found here:\n"
                       "\t\tgithub.com/playback0022/OS-ProcFS";
    // verifying that the memory allocation was successful for the systemd process
    if ((rootOfFS->children[0] = (process *) malloc(sizeof(process))) == NULL)
        return ENOMEM;
    rootOfFS->children[0]->PID = 1;


    int operationStatus;
    // checking that no errors were encountered in the construction of the process tree
    if ((operationStatus = constructTreeOfProcesses(rootOfFS->children[0])) != 0) {
        fprintf(stderr, "Tree construction failed at process #%d\n", operationStatus);
        return 1;
    }

    return fuse_main(argc, argv, &implementedOperations, NULL);
}