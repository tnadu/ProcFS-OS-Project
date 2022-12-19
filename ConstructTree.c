#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>

struct Proc{
    int PID;
    int numberOfChildren;
    struct Proc *children[201];
    char *status;
} rootProc;

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

    // pipeFD[0] -> will refer to the read end of the pipe
    // pipeFD[1] -> will refer to the write end of the pipe
    // nbytes -> will be the number of bytes written at a time
    int pipeFD[2], nbytes;

    if( pipe(pipeFD) == -1){
        perror("Piping failed! Please try again.");
    } else{
        pid_t pid = fork ();
        if ( pid < 0)
            perror("Forking failed! Please try again.");
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

            // Get the size of the file with `stat`
            struct stat catFileStat; 
            stat(catPathBuff, &catFileStat);
            int status_size = catFileStat.st_size;

            // PROBLEM! Stat won't return information about /proc/{num}/stat file
            // we will use 3000 as default => TO DO
            process->status = malloc(sizeof(char) * 3000);

            int StatusOffset = 0;
            // use pipeFD[0] to read from the pipe
            while( (nbytes = read(pipeFD[0], process->status + StatusOffset, 255)) > 0){
                StatusOffset += nbytes;
            }
        }
    }
}

void constructTreeOfProcesses(struct Proc *process){
    // First of all, set the status file for the current process
    setStatus(process);

    char catPathBuff[40], PIDtoChar[8];
    char *readBuffer = malloc(sizeof(char) * 255);

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
    } else{
        pid_t pid = fork();

        if ( pid < 0)
            perror("Forking failed! Please try again.");
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
                process->children[numOfChildren] = malloc(sizeof(struct Proc));
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

// Command to get all the PIDs of processes: ls /proc | grep -Eo '[0-9]{1,4}'
int main(int argc, char *argv[]){ 
    // Father of fathers: process 1

    rootProc.PID = 1;
    constructTreeOfProcesses(&rootProc);
    testTree(&rootProc);
    return 0;
}
