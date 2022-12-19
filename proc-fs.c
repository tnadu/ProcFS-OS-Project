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


process* getProcess(char *path);


//when the system asks for the attributes of a specific file
static int _getattr(const char *path, struct stat *status){
    
    process* target = getProcess(path);
    
    if(target != NULL){
        
        status->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
        status->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
        //status->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
        //status->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now
        
        if ( strcmp( path, "/" ) == 0 ) // path = root directory
        {
            //st_mode specifies if file is a regular file, directory or other
            status->st_mode = S_IFDIR | 0755; //directory;only the owner of the file -> write, execute the directory, other users-> read and execute 
            status->st_nlink = 2; 
        }
        else
        {
            char * ptr;
            int ch = '/';
            ptr = strrchr( path, ch );
            if(ptr != NULL && strcmp("/stat", ptr) == 0){ // suntem in /../stat
                status->st_mode = S_IFREG | 0644; // regular files; owner->read,write; others -> read
                status->st_nlink = 1;
                status->st_size = 1024;
            }
        }
    }
    else{ // directory / file does not exist 
        perror("operation failed: ");
        return errno;
    }
    
    return 0;
}


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