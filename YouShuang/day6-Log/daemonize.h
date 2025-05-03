#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>


bool deamonize(){
    pid_t pid = fork();
    if(pid < 0){
        return false;
    }
    else if(pid > 0){
        exit(0); // exit parent process
    }

    // set file mode creation mask
    umask(0);

    // create new session and set this process as session leader
    pid_t sid = setsid();
    if(sid < 0){
        return false;
    }
    
    
    // switch to root directory
    if(chdir("/") < 0){
        return false;
    }

    // close standard input
    close(STDIN_FILENO);
    // close standard output
    close(STDOUT_FILENO);
    // close standard error
    close(STDERR_FILENO);

    // close all open file descriptors
    for(int x = sysconf(_SC_OPEN_MAX); x >= 0; x--){
        close(x);
    }

    // redirect standard input, output, error to /dev/null
    open("/dev/null", O_RDONLY); // stdin
    open("/dev/null", O_WRONLY); // stdout
    open("/dev/null", O_RDWR); // stderr
    return true;
}