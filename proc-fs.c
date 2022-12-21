#define FUSE_USE_VERSION 31

#include </usr/include/fuse3/fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <errno.h>


typedef struct proc {
    int PID, numberOfChildren;
    struct proc *children[201];
    char *status;
} process;

process *rootOfFS;
time_t timeOfMount;


// Function that will use a pipe to:
// cat /proc/PID/status -> write to the end of pipe
// Proc.status          -> read from end of pipe
void setStatus(process *process) {
    char catPathBuff[400], PIDtoChar[20];

    // Construct the correct path to the status file of the process
    sprintf(PIDtoChar, "%d", process->PID);
    strcpy(catPathBuff, "/proc/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/status");

    process->status = malloc(sizeof(char) * 6000);

    int statusFileDescriptor = open(catPathBuff, O_RDONLY);

    int StatusOffset = 0, nbytes;
    while ((nbytes = read(statusFileDescriptor, process->status + StatusOffset, 255)) > 0)
        StatusOffset += nbytes;

    close(statusFileDescriptor);
}


int constructTreeOfProcesses(process *currentProcess) {
    // First of all, set the status file for the current process
    setStatus(currentProcess);

    char catPathBuff[200], PIDtoChar[20];
    char *readBuffer = malloc(sizeof(char) * 4000);

    // Construct the correct path to the file that contains
    // the children processes
    sprintf(PIDtoChar, "%d", currentProcess->PID);
    strcpy(catPathBuff, "/proc/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/task/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/children");

    int childrenFileDescriptor = open(catPathBuff, O_RDONLY);

    int StatusOffset = 0, nbytes;
    while ((nbytes = read(childrenFileDescriptor, readBuffer + StatusOffset, 255)) > 0) {
        StatusOffset += nbytes;
    }

    close(childrenFileDescriptor);

    // After we get the children processes, we need to build
    // the Proc *children array for the current process,
    // and recursively call this function for the children
    int numOfChildren = 0;
    char *childProcess = strtok(readBuffer, " ");
    while (childProcess != NULL) {
        // First allocate memory for the children
        currentProcess->children[numOfChildren] = malloc(sizeof(process));
        // Set its PID
        currentProcess->children[numOfChildren]->PID = atoi(childProcess);

        numOfChildren++;
        childProcess = strtok(NULL, " ");
    }

    currentProcess->numberOfChildren = numOfChildren;

    for (int i = 0; i < currentProcess->numberOfChildren; i++) {
        constructTreeOfProcesses(currentProcess->children[i]);
    }

    return 0;
}


// DESCRIPTION
// 'returnedProcess' holds:
//  - the pointer to the specified process, if the path is valid
//  - NULL, otherwise
//
// return values:
//  - '-1' -> invalid path
//  - '0' -> a process's dir was requested
//  - '1' -> the 'stats' file of a process was requested
int getProcess(char *path, process **returnedProcess) {
    // all paths must begin from the root of the file system
    if (*path != '/') {
        *returnedProcess = NULL;
        return -1;
    }

    // used in traversing the items delimited by '/'
    char *currentItemInPath = strtok(path, "/");    // omitting the empty char

    // the provided path is the root of the FS
    if (currentItemInPath == NULL) {
        *returnedProcess = rootOfFS;
        return 0;
    }

    *returnedProcess = rootOfFS;    // points to the root of the FS (PID = -1)


    // according to the signature of the 'strtol' function, a char pointer is used in
    // the case where an invalid character for the provided numerical base is encountered
    char *firstNonDigit;

    // the root process's PID is always equal to 1 (systemd - linux);
    // even when 1 is returned as the result of the conversion of the first item to 
    // a numerical value, it might still be possible that invalid characters where
    // encountered, which is why the provided char pointer must be either NULL or the null char
    if (strtol(currentItemInPath, &firstNonDigit, 0) != 1 && firstNonDigit != NULL && *firstNonDigit != '\0') {
        *returnedProcess = NULL;
        return -1;
    }

    *returnedProcess = rootOfFS->children[0];    // points to the root process (PID = 1)


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
                *returnedProcess = NULL;
                return -1;
            }
        }

        // searching the children of the current process for the child with the specified PID 
        int i, numberOfChildrenOfTheCurrentProcess = (*returnedProcess)->numberOfChildren;

        for (i = 0; i < numberOfChildrenOfTheCurrentProcess; i++)
            // specified process was found
            if ((*returnedProcess)->children[i]->PID == currentPID) {
                *returnedProcess = (*returnedProcess)->children[i];
                break;
            }

        // no children with the specified PID could be found
        if (i == numberOfChildrenOfTheCurrentProcess) {
            *returnedProcess = NULL;
            return -1;
        }
    }


    // 'stats' file found in path
    if (statsFound == 1) {

        if (strtok(NULL, "/") != NULL) {
            *returnedProcess = NULL;
            return -1;
        }

        return 1;   // 'stats' file requested in the specified path
    }

    return 0;   // process directory requested in the specified path
}


//when the system asks for the attributes of a specific file
static int p_getattr(const char *path, struct stat *status, struct fuse_file_info *fi) {
    (void) fi;
    memset(status, 0, sizeof(struct stat));

    char *nonConstPath = (char *) malloc(sizeof(char) * strlen(path));
    strcpy(nonConstPath, path);

    process *target;

    int returnStatus = getProcess(nonConstPath, &target);


//    char message[100], PID[10];
//    sprintf(PID, "%d", target->PID);
//    strcpy(message, "GET_ATTR -- Process ");
//    strcat(message, PID);
//    strcat(message, ":");
//    perror(message);
//    fprintf(stdout, "GETATTR -- Process %d:\n", target->PID);
//    fflush(stdout);

    if (target != NULL) {
        status->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
        status->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
        status->st_atime = timeOfMount; // The last "a"ccess of the file/directory is right now
        status->st_mtime = timeOfMount; // The last "m"odification of the file/directory is right now

        if (returnStatus == 0) // path = directory
        {
            //st_mode specifies if file is a regular file, directory or other
            status->st_mode = S_IFDIR | 0775; //directory;only the owner of the file -> write, execute the directory, other users-> read and execute
            status->st_nlink = 2;
        } else {
            status->st_mode = S_IFREG | 0664; // regular files; owner->read,write; others -> read
            status->st_nlink = 1;
            status->st_size = strlen(target->status);
        }

        return 0;
    }
    else
        return -1;
}


static int p_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo, enum fuse_readdir_flags flags) {
    (void) offset;
    (void) fileInfo;
    (void) flags;

    char *nonConstPath = (char *) malloc(sizeof(char) * strlen(path));
    strcpy(nonConstPath, path);

    process *target;
    int returnStatus = getProcess(nonConstPath, &target);

//    char message[100];
    char PID[10];
//    sprintf(PID, "%d", target->PID);
//    strcpy(message, "READDIR -- Process ");
//    strcat(message, PID);
//    strcat(message, ":");
//    perror(message);


    if (returnStatus == 0) {
        filler(buffer, ".", NULL, 0, 0);
        filler(buffer, "..", NULL, 0, 0);

        for (int i = 0; i < target->numberOfChildren; i++) {
            sprintf(PID, "%d", target->children[i]->PID);
            filler(buffer, PID, NULL, 0, 0);
//            perror(PID);
        }

        if (target->PID != -1)
            filler(buffer, "stats", NULL, 0, 0);

        return 0;
    }
    else
        return -1;
}


static int p_open(const char *path, struct fuse_file_info *fileInfo)
{
    char *nonConstPath = malloc(sizeof(char) * strlen(path));
    strcpy(nonConstPath, path);

    process *requestedProcess;
    int returnNumber = getProcess(nonConstPath, &requestedProcess);

    // path is either invalid or requested item is a directory
    if (returnNumber == -1 || returnNumber == 0)
        return -ENOENT;

    if ((fileInfo->flags & O_ACCMODE) != O_RDONLY)
        return -AT_EACCESS;

    return 0;
}


static int p_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fileInfo) {
    (void) fileInfo;

    char *nonConstPath = malloc(sizeof(char) * strlen(path));
    strcpy(nonConstPath, path);

    process *requestedProcess;
    int returnNumber = getProcess(nonConstPath, &requestedProcess);

//    char message[6000];
//    strcpy(message, requestedProcess->status);
//    perror(message);
//    fflush(stderr);

    // path is either invalid or requested item is a directory
    if (returnNumber == -1 || returnNumber == 0)
        return -1;

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

//    size_t numberOfBytesCopied;
//    if (size < strlen(requestedProcess->status) - offset)
//        numberOfBytesCopied = size;
//    else
//        numberOfBytesCopied = strlen(requestedProcess->status) - offset;
//
    memcpy(buffer, requestedProcess->status + offset, numberOfBytesCopied);

//    char message1[] = "Hello world!\0";
//    memcpy(buffer, "Hello world!", 12);

    // 'read' must return the total number of successfully copied bytes
//    return numberOfBytesCopied;
    return numberOfBytesCopied;
}


static struct fuse_operations implementedOperations = {
        .getattr = p_getattr,
        .readdir = p_readdir,
        .open = &p_open,
        .read = p_read
};


void testTree(process *process){
    char PID[20]; // childPID[20],
    char message[6000];
    sprintf(PID, "%d", process->PID);

    strcpy(message, "Process ");
    strcat(message, PID);
    strcat(message, ":");
    perror(message);

    strcpy(message, process->status);
    perror(message);

//    for(int i = 0; i < process->numberOfChildren; i++) {
//        sprintf(childPID, "%d", process->children[i]->PID);
//        strcpy(message, "----> Child ");
//        strcat(message, childPID);
//        strcat(message, " of process ");
//        strcat(message, PID);
//        strcat(message, "");
//
//        perror(message);
//    }
    perror("\n");

    for(int i = 0; i < process->numberOfChildren; i++){
        testTree(process->children[i]);
    }
}


int main(int argc, char **argv) {
    timeOfMount = time(NULL);

    // initializing file system root
    rootOfFS = (process *) malloc(sizeof(process));
    rootOfFS->PID = -1;
    rootOfFS->numberOfChildren = 1;
    rootOfFS->status = NULL;
    rootOfFS->children[0] = (process *) malloc(sizeof(process));
    rootOfFS->children[0]->PID = 1;


    int operationStatus;
    if ((operationStatus = constructTreeOfProcesses(rootOfFS->children[0])) != 0) {
        fprintf(stdout, "Tree construction failed at process #%d", operationStatus);
        fprintf(stderr, "Tree construction failed at process #%d", operationStatus);
        return 1;
    }

//    testTree(rootOfFS->children[0]);

    return fuse_main(argc, argv, &implementedOperations, NULL);
}