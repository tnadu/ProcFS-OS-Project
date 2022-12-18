#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <time.h>
#include <string.h>


typedef struct proc {
    int PID, numberOfChildren;
    struct proc *children[201];
    char *status;
} process;

process *rootOfFS;


int constructTreeOfProcesses(process *root, int PID) {
////    MOCK-UP IMPLEMENTATION
////    when any syscall fails -> return PID
//    root = malloc(sizeof(process));
//    root->PID = PID;
//
//    int fd = open("/proc/PID/task/PID/children", O_RDONLY);
//    char childrenList[12031];
//    read(fd, childrenList, 12031);
//    root->numberOfChildren = 0;
//
//    fd = open("/proc/PID/status", O_RDONLY);
//    root->status = malloc(sizeof(char) * 82738);
//    read(fd, root->status, 82738);
//
//    int returnStatus;
//    for(int i=0; i<strlen(childrenList); i++) {
//        returnStatus = constructTreeOfProcesses(root->children[i], childrenList[i]);
//        root->numberOfChildren++;
//
//        if (!returnStatus)
//            return returnStatus;
//    }
//
//    return 0;
}


int getProcess(char *path, process **returnedProcess) {
    // all paths must begin from the root of the file system
    if (*path != '/') {
        *returnedProcess = NULL;
        return -1;
    }

    // used in traversing the items delimited by '/'
    char *currentItemInPath = strtok(path, "/");    // omitting the empty char
    currentItemInPath = strtok(NULL, "/");

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
        if (firstNonDigit!=NULL && *firstNonDigit!='\0') {
            // the stats file was requested in the path
            if (strcmp(currentItemInPath, "stats") != 0) {
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
        
        for(i = 0; i < numberOfChildrenOfTheCurrentProcess; i++)
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


static int _getattr(const char *path, struct stat *status);


static int _readdir(const char *path, void *buffer, fuse_fill_dir_t, off_t offset, struct fuse_file_info *fileInfo);


static int _read(const char *path, char *buffer, size_t, off_t, struct  fuse_file_info *fileInfo);


static struct fuse_operations implementedOperations = {
    .getattr = &_getattr,
    .readdir = &_readdir,
    .read = &_read
};


int main(int argc, char **argv) {
    int operationStatus;
    if ((operationStatus = constructTreeOfProcesses(rootOfFS, 1)) != 0) {
        fprintf(stdout, "Tree construction failed at process #%d", operationStatus);
        fprintf(stderr, "Tree construction failed at process #%d", operationStatus);
        return 1;
    }

    return fuse_main(argc, argv, &implementedOperations, NULL);
}