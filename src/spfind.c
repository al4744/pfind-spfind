#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <sys/select.h>

#define BUFFER_SIZE 4096

int main(int argc, char *argv[]) {
    // Check if help flag is present
    if (argc == 1 || (argc > 1 && strcmp(argv[1], "-h") == 0)) {
        printf("Usage: ./spfind -d <directory> -p <permissions string> [-h]\n");
        return argc > 1 ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // Create pipes
    int pfind_to_sort[2];
    int sort_to_parent[2];
    int error_pipe[2];

    if (pipe(pfind_to_sort) == -1 || pipe(sort_to_parent) == -1 || pipe(error_pipe) == -1) {
        fprintf(stderr, "Error: Failed to create pipes.\n");
        return EXIT_FAILURE;
    }

    // Fork first child for pfind
    pid_t pfind_pid = fork();
    if (pfind_pid == -1) {
        fprintf(stderr, "Error: Fork failed.\n");
        return EXIT_FAILURE;
    }

    if (pfind_pid == 0) {  // pfind child
        // Close unused pipe ends
        close(pfind_to_sort[0]);
        close(sort_to_parent[0]);
        close(sort_to_parent[1]);
        close(error_pipe[0]);

        // Redirect stdout and stderr
        if (dup2(pfind_to_sort[1], STDOUT_FILENO) == -1 ||
            dup2(error_pipe[1], STDERR_FILENO) == -1) {
            fprintf(stderr, "Error: Failed to redirect output.\n");
            exit(EXIT_FAILURE);
        }
        close(pfind_to_sort[1]);
        close(error_pipe[1]);

        // Execute pfind
        execv("./pfind", argv);
        fprintf(stderr, "Error: pfind failed.\n");
        exit(EXIT_FAILURE);
    }

    // Fork second child for sort
    pid_t sort_pid = fork();
    if (sort_pid == -1) {
        fprintf(stderr, "Error: Fork failed.\n");
        return EXIT_FAILURE;
    }

    if (sort_pid == 0) {  // sort child
        // Close unused pipe ends
        close(pfind_to_sort[1]);
        close(sort_to_parent[0]);
        close(error_pipe[0]);
        close(error_pipe[1]);

        // Redirect stdin and stdout
        if (dup2(pfind_to_sort[0], STDIN_FILENO) == -1 ||
            dup2(sort_to_parent[1], STDOUT_FILENO) == -1) {
            fprintf(stderr, "Error: Failed to redirect file descriptors.\n");
            exit(EXIT_FAILURE);
        }
        close(pfind_to_sort[0]);
        close(sort_to_parent[1]);

        // Execute sort
        execlp("sort", "sort", NULL);
        fprintf(stderr, "Error: sort failed.\n");
        exit(EXIT_FAILURE);
    }

    // Parent process
    // Close unused pipe ends
    close(pfind_to_sort[0]);
    close(pfind_to_sort[1]);
    close(sort_to_parent[1]);
    close(error_pipe[1]);

    // Set up file descriptor sets for select
    fd_set read_fds;
    int max_fd = (sort_to_parent[0] > error_pipe[0]) ? sort_to_parent[0] : error_pipe[0];
    int error_done = 0;
    int sort_done = 0;
    int line_count = 0;
    char buffer[BUFFER_SIZE];
    int had_error = 0;

    while (!error_done || !sort_done) {
        FD_ZERO(&read_fds);
        if (!error_done) FD_SET(error_pipe[0], &read_fds);
        if (!sort_done) FD_SET(sort_to_parent[0], &read_fds);

        if (select(max_fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            if (errno == EINTR) continue;
            perror("select");
            return EXIT_FAILURE;
        }

        // Handle error pipe
        if (!error_done && FD_ISSET(error_pipe[0], &read_fds)) {
            ssize_t bytes = read(error_pipe[0], buffer, sizeof(buffer));
            if (bytes > 0) {
                write(STDERR_FILENO, buffer, bytes);
                had_error = 1;
            } else if (bytes == 0) {
                error_done = 1;
            }
        }

        // Handle sort output
        if (!sort_done && FD_ISSET(sort_to_parent[0], &read_fds)) {
            ssize_t bytes = read(sort_to_parent[0], buffer, sizeof(buffer));
            if (bytes > 0) {
                write(STDOUT_FILENO, buffer, bytes);
                for (ssize_t i = 0; i < bytes; i++) {
                    if (buffer[i] == '\n') line_count++;
                }
            } else if (bytes == 0) {
                sort_done = 1;
            }
        }
    }

    close(error_pipe[0]);
    close(sort_to_parent[0]);

    // Wait for children
    int pfind_status, sort_status;
    waitpid(pfind_pid, &pfind_status, 0);
    waitpid(sort_pid, &sort_status, 0);

    // Print total matches
    printf("Total matches: %d\n", line_count);

    // Return failure if either child failed or if we had errors
    if (had_error || !WIFEXITED(pfind_status) || !WIFEXITED(sort_status) ||
        WEXITSTATUS(pfind_status) != 0 || WEXITSTATUS(sort_status) != 0) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
