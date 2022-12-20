#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/wait.h>
#include <time.h>
#include <string.h>
#include <limits.h>

struct Proc {
    int PID;
    int numberOfChildren;
    struct Proc *children[201];
    char *status;
};

struct Proc *rootProc;

// Function that will use a pipe to:
// cat /proc/PID/status -> write to the end of pipe
// Proc.status          -> read from end of pipe

void setStatus(struct Proc *process){
    char catPathBuff[25], PIDtoChar[8];

    // Construct the correct path to the status file of the process
    sprintf(PIDtoChar, "%d", process->PID);
    strcpy(catPathBuff, "/proc/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/status");


    int StatusOffset = 0, nbytes, fd = open(catPathBuff, O_RDONLY);
    process->status = malloc(sizeof(char) * 3000);

    while( (nbytes = read(fd, process->status + StatusOffset, 255)) > 0){
        StatusOffset += nbytes;
    }

    close(fd);
}

void constructTreeOfProcesses(struct Proc *process) {
    // First of all, set the status file for the current process
    setStatus(process);

    char catPathBuff[40], PIDtoChar[8];
    char *readBuffer = malloc(sizeof(char) * 4000);

    // Construct the correct path to the file that contains
    // the children processes
    sprintf(PIDtoChar, "%d", process->PID);
    strcpy(catPathBuff, "/proc/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/task/");
    strcat(catPathBuff, PIDtoChar);
    strcat(catPathBuff, "/children");

    int StatusOffset = 0, nbytes, fd = open(catPathBuff, O_RDONLY);

    while ((nbytes = read(fd, readBuffer + StatusOffset, 255)) > 0) {
        StatusOffset += nbytes;
    }

    close(fd);

    // After we get the children processes, we need to build
    // the Proc *children array for the current process,
    // and recursively call this function for the children
    int numOfChildren = 0;
    char *childProcess = strtok(readBuffer, " ");
    while (childProcess != NULL) {
        // First allocate memory for the children
        process->children[numOfChildren] = malloc(sizeof(struct Proc));
        // Set its PID
        process->children[numOfChildren]->PID = atoi(childProcess);

        numOfChildren += 1;
        childProcess = strtok(NULL, " ");
    }

    process->numberOfChildren = numOfChildren;

    for (int i = 0; i < process->numberOfChildren; i++) {
        constructTreeOfProcesses(process->children[i]);
    }
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
int getProcess(char *path, struct Proc **returnedProcess) {
//    printf("%s", path);
    // all paths must begin from the root of the file system
    if (*path != '/') {
        *returnedProcess = NULL;
        return -1;
    }

    // used in traversing the items delimited by '/'
    char *currentItemInPath = strtok(path, "/");    // omitting the empty char

    // the provided path is the root of the FS
    if (currentItemInPath == NULL) {
        *returnedProcess = rootProc;
        return 0;
    }

    *returnedProcess = rootProc;    // points to the root of the FS (PID = -1)


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

    *returnedProcess = rootProc->children[0];    // points to the root process (PID = 1)


    int statsFound = 0;     // used to mark that the 'stats' file was encountered in the path
    long currentPID;        // the PID of the process being searched for at every level of the process tree

    // parsing the remaining path and the corresponding process tree path
    for (currentItemInPath = strtok(NULL, "/"); currentItemInPath != NULL; currentItemInPath = strtok(NULL, "/")) {
        // 'strtol' returns the result of the conversion to a numerical value of the current item in the path
        currentPID = strtol(currentItemInPath, &firstNonDigit, 0);
        // verifying that no invalid characters were encountered
        if (firstNonDigit!=NULL && *firstNonDigit!='\0') {
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


void testTree(struct Proc *process){
    printf("Process %d:\n", process->PID);
    for(int i = 0; i < process->numberOfChildren; i++){
        printf("----> Child %d of process %d\n", process->children[i]->PID, process->PID);
    }
    printf("\n");
    for(int i = 0; i < process->numberOfChildren; i++){
        testTree(process->children[i]);
    }
}

void testGetProc(char *path) {
    char * auxPath = (char *) malloc(sizeof (char) * 100);
    strcpy(auxPath, path);

    struct Proc *process;
    int result = getProcess(auxPath, &process);

    if (result == -1)
        printf("Invalid path.\n\n");
    else if (result == 1) {
        printf("%s -- file\n", path);
        printf("%d\n\n", process->PID);
    }
    else {
        printf("%s -- dir\n", path);
        printf("%d\n\n", process->PID);
    }
}

// Command to get all the PIDs of processes: ls /proc | grep -Eo '[0-9]{1,4}'
int main(int argc, char *argv[]){
    // Father of fathers: process 1
    rootProc = (struct Proc *) malloc(sizeof(struct Proc *));

    rootProc->PID = -1;
    rootProc->numberOfChildren = 1;
    rootProc->status = NULL;
    rootProc->children[0] = (struct Proc *) malloc(sizeof (struct Proc));
    rootProc->children[0]->PID = 1;
    
    constructTreeOfProcesses(rootProc->children[0]);

//    char *path = (char *) malloc(sizeof (char) * 100);
//    strcpy(path, "/");
//    testGetProc(path);
//
//    strcpy(path, "/1");
//    testGetProc(path);
//
//    strcpy(path, "/1/2286/stats");
//    testGetProc(path);
//
//    strcpy(path, "/1/2285");
//    testGetProc(path);
    testTree(rootProc);
    return 0;
}
