/*
Implementing linux tee command.
Implement printing a message to both the standard output (STDOUT) and a file using the splice() and tee().
*/

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>

int main(int argc, char const *argv[])
{
    if(argc < 2){
        printf("Usage: %s <file>\n", argv[0]);
        return 1;
    }

    int filefd = open(argv[1], O_CREAT | O_WRONLY | O_TRUNC, 0666);
    assert(filefd>0);

    int pipefd_file[2];
    int pipefd_stdout[2];
    int ret = pipe(pipefd_stdout);
    assert(ret != -1);
    ret = pipe(pipefd_file);
    assert(ret != -1);

    assert(splice(STDIN_FILENO, NULL, pipefd_stdout[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE) != -1);
    assert(tee(pipefd_stdout[0], pipefd_file[1], 32768, SPLICE_F_NONBLOCK) != -1);
    assert(splice(pipefd_file[0], NULL, filefd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE) != -1);
    assert(splice(pipefd_stdout[0], NULL, STDOUT_FILENO, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE) != -1);

    close(filefd);
    close(pipefd_stdout[0]);
    close(pipefd_stdout[1]);
    close(pipefd_file[0]);
    close(pipefd_file[1]);
    return 0;
}

