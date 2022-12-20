#define FUSE_USE_VERSION 30

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <limits.h>


typedef struct proc {
    int PID, numberOfChildren;
    struct proc *children[201];
    char *status;
} process;

process *rootOfFS;


// Function that will use a pipe to:
// cat /proc/PID/status -> write to the end of pipe
// Proc.status          -> read from end of pipe

int setStatus(process *process) {
    char catPathBuff[25], PIDtoChar[8];

    // Construct the correct path to the status file of the process
    sprintf(PIDtoChar, "%d", process->PID);
    strcpy(catPathBuff, "/proc/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/status");

    // pipeFD[0] -> will refer to the read end of the pipe
    // pipeFD[1] -> will refer to the write end of the pipe
    // nbytes -> will be the number of bytes written at a time
    int pipeFD[2], nbytes;

    if( pipe(pipeFD) == -1){
        perror("Piping failed! Please try again.");
        return process->PID;
    } else{
        pid_t pid = fork ();
        if ( pid < 0) {
            perror("Forking failed! Please try again.");
            return process->PID;
        }
        else if ( pid == 0){
            // Children process:
            // -> will write to the pipe
            // -> won't need the read end of the pipe
            dup2(pipeFD[1], 1);         // STDOUT -> will go to the pipeFD[1], write end of the pipe
            close(pipeFD[0]);           // Child won't read from the pipe

            char *cmd[] = {"cat", catPathBuff, NULL };

            // first arg -> command
            // second arg -> arguments
            execvp(cmd[0], cmd);

        } else{
            // Parent process:
            // -> will wait for the child to finish
            // -> will read from the pipe, won't write on the pipe
            wait(NULL);

            close(pipeFD[1]);          // Parent won't write on the pipe

            process->status = (char *) malloc(sizeof(char) * 3000);

            int StatusOffset = 0;
            // use pipeFD[0] to read from the pipe
            while( (nbytes = read(pipeFD[0], process->status + StatusOffset, 255)) > 0){
                StatusOffset += nbytes;
            }
        }
    }

    return 0;
}

int constructTreeOfProcesses(process *process) {
    // First of all, set the status file for the current process
    int operationStatus = setStatus(process);

    if (operationStatus != 0)
        return operationStatus;

    char catPathBuff[200], PIDtoChar[40];
    char *readBuffer = (char *) malloc(sizeof(char) * 255);

    // Construct the correct path to the file that contains
    // the children processes
    sprintf(PIDtoChar, "%d", process->PID);
    strcpy(catPathBuff, "/proc/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/task/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/children");

    int pipeFD[2], nbytes;
    if(pipe(pipeFD) == -1){
        perror("Piping failed! Please try again.");
        return process->PID;
    } else{
        pid_t pid = fork();

        if ( pid < 0) {
            perror("Forking failed! Please try again.");
            return process->PID;
        }
        else if ( pid == 0){
            // Children process:
            // -> will write to the pipe
            // -> won't need the read end of the pipe
            dup2(pipeFD[1], 1);
            close(pipeFD[0]);

            char *cmd[] = {"cat", catPathBuff, NULL };
            execvp(cmd[0], cmd);

        } else{
            // Parent process:
            // -> will wait for the child to finish
            // -> will read from the pipe, won't write on the pipe
            wait(NULL);

            close(pipeFD[1]);

            // use pipeFD[0] to read from the pipe
            while( (nbytes = read(pipeFD[0], readBuffer, 255)) > 0);

            // After we get the children processes, we need to build
            // the Proc *children array for the current process,
            // and recursively call this function for the children
            int numOfChildren = 0;
            char *childProcess = strtok(readBuffer, " ");
            while(childProcess != NULL){
                // First allocate memory for the children
                process->children[numOfChildren] = malloc(sizeof(process));
                // Set its PID
                process->children[numOfChildren]->PID = atoi(childProcess);

                numOfChildren += 1;
                childProcess = strtok(NULL, " ");
            }
            process->numberOfChildren = numOfChildren;

            for(int i = 0; i < process->numberOfChildren; i++){
                constructTreeOfProcesses(process->children[i]);
            }
        }
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


//when the system asks for the attributes of a specific file
static int _getattr(const char *path, struct stat *status){
    
    process* target;
    char *nonConstPath = (char*) malloc(sizeof(char) * strlen(path));
    int returnStatus = getProcess(nonConstPath, &target);
    
    if(target != NULL){
        
        status->st_uid = getuid(); // The owner of the file/directory is the user who mounted the filesystem
        status->st_gid = getgid(); // The group of the file/directory is the same as the group of the user who mounted the filesystem
        status->st_atime = time( NULL ); // The last "a"ccess of the file/directory is right now
        status->st_mtime = time( NULL ); // The last "m"odification of the file/directory is right now
        status->st_size = 0;

        if ( returnStatus == 0 ) // path = root directory
        {
            //st_mode specifies if file is a regular file, directory or other
            status->st_mode = S_IFDIR | 0755; //directory;only the owner of the file -> write, execute the directory, other users-> read and execute 
            status->st_nlink = 2;
        }
        else
        {
            status->st_mode = S_IFREG | 0644; // regular files; owner->read,write; others -> read
            status->st_nlink = 1;
        }
    }
    else{ // directory / file does not exist 
        perror("operation failed: ");
        return -1;
    }
    
    return 0;
}


static int _readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fileInfo) {
    process* target;
    char *nonConstPath = (char*) malloc(sizeof(char) * strlen(path));
    int returnStatus = getProcess(nonConstPath, &target);

    if(target != NULL){
        char buf[200];
        int index = 0;

        while (buffer && *(char *) buffer != '\0')  // buf = buffer content
        {
            buf[index++] = *(char *) buffer;
            (buffer)++;
        }


        for (int i = 0; i < target->numberOfChildren; i++)
        {
            int pid = target->children[i]->PID;
            char pidChar[100];
            int lenPid = 0;

            while (pid)
            {
                pidChar[lenPid++] = (pid % 10) + '0';
                pid /= 10;
            }

            while (lenPid)  // buf concatenare cu pid
            {
                buf[index++] = pidChar[lenPid - 1];
                lenPid--;
            }

            buf[index++] = '/';
        }

        buf[index++] = '\0';

        buffer = buf; // buffer points to the new location
    }
    else{
        perror("operation failed: ");
        return -1;
    }

    return 0;
}


static int _read(const char *path, char *buffer, size_t size, off_t offset, struct  fuse_file_info *fileInfo) {
    process *requestedProcess;
    char *nonConstPath = (char*) malloc(sizeof(char) * strlen(path));
    int returnNumber = getProcess(nonConstPath, &requestedProcess);

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
    size_t numberOfBytesCopied = (size < strlen(requestedProcess->status) - offset) ? size : strlen(requestedProcess->status) - offset;

    memcpy(buffer, requestedProcess->status + offset, numberOfBytesCopied);

    // 'read' must return the total number of successfully copied bytes
    return (int) numberOfBytesCopied;
}


static struct fuse_operations implementedOperations = {
    .getattr = &_getattr,
    .readdir = &_readdir,
    .read = &_read
};


int main(int argc, char **argv) {
//    printf("%s\n",);

    // initializing file system root
    rootOfFS = (process *) malloc(sizeof (process));
    rootOfFS->PID = -1;
    rootOfFS->numberOfChildren = 1;
    rootOfFS->status = NULL;
    rootOfFS->children[0] = (process *) malloc(sizeof (process));
    rootOfFS->children[0]->PID = 1;

    int operationStatus;
    if ((operationStatus = constructTreeOfProcesses(rootOfFS->children[0])) != 0) {
        fprintf(stdout, "Tree construction failed at process #%d", operationStatus);
        fprintf(stderr, "Tree construction failed at process #%d", operationStatus);
        return 1;
    }

    return fuse_main(argc, argv, &implementedOperations, NULL);
}