#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>

#define MAX_PATH 4096
#define PERM_LEN 9

// Function to validate permissions string
int validate_permissions(const char *perms) {
    if (strlen(perms) != PERM_LEN) return 0;

    for (int i = 0; i < PERM_LEN; i++) {
        if (perms[i] != '-' && perms[i] != 'r' && perms[i] != 'w' && perms[i] != 'x') {
            return 0;
        }
    }
    return 1;
}

// Function to get file permissions string
void get_file_permissions(mode_t mode, char *perms) {
    perms[0] = (mode & S_IRUSR) ? 'r' : '-';
    perms[1] = (mode & S_IWUSR) ? 'w' : '-';
    perms[2] = (mode & S_IXUSR) ? 'x' : '-';
    perms[3] = (mode & S_IRGRP) ? 'r' : '-';
    perms[4] = (mode & S_IWGRP) ? 'w' : '-';
    perms[5] = (mode & S_IXGRP) ? 'x' : '-';
    perms[6] = (mode & S_IROTH) ? 'r' : '-';
    perms[7] = (mode & S_IWOTH) ? 'w' : '-';
    perms[8] = (mode & S_IXOTH) ? 'x' : '-';
    perms[9] = '\0';
}

// Function to recursively search directory
void search_directory(const char *dir_path, const char *target_perms) {
    DIR *dir = opendir(dir_path);
    if (!dir) {
        fprintf(stderr, "Error: Cannot open directory '%s'. %s.\n", dir_path, strerror(errno));
        return;
    }

    struct dirent *entry;
    char full_path[MAX_PATH];
    struct stat st;
    char file_perms[PERM_LEN + 1];

    while ((entry = readdir(dir)) != NULL) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Check path length
        if (strlen(dir_path) + strlen(entry->d_name) + 2 > MAX_PATH) {
            fprintf(stderr, "Error: Path too long when trying to access '%s/%s'.\n",
                    dir_path, entry->d_name);
            continue;
        }

        // Construct full path
        snprintf(full_path, MAX_PATH, "%s/%s", dir_path, entry->d_name);

        // Get file status
        if (lstat(full_path, &st) != 0) {
            fprintf(stderr, "Error: Cannot stat '%s'. %s.\n", full_path, strerror(errno));
            continue;
        }

        // Get and check permissions
        get_file_permissions(st.st_mode, file_perms);
        if (strcmp(file_perms, target_perms) == 0) {
            printf("%s\n", full_path);
        }

        // Recursively search if directory
        if (S_ISDIR(st.st_mode)) {
            search_directory(full_path, target_perms);
        }
    }

    closedir(dir);
}

int main(int argc, char *argv[]) {
    // Print usage if no arguments
    if (argc == 1) {
        printf("Usage: ./pfind -d <directory> -p <permissions string> [-h]\n");
        return EXIT_FAILURE;
    }

    char *dir_path = NULL;
    char *perms = NULL;
    int opt;

    // Parse command line arguments
    while ((opt = getopt(argc, argv, "d:p:h")) != -1) {
        switch (opt) {
            case 'd':
                dir_path = optarg;
                break;
            case 'p':
                perms = optarg;
                break;
            case 'h':
                printf("Usage: ./pfind -d <directory> -p <permissions string> [-h]\n");
                return EXIT_SUCCESS;
            default:
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                return EXIT_FAILURE;
        }
    }

    // Check required arguments
    if (!dir_path) {
        fprintf(stderr, "Error: Required argument -d <directory> not found.\n");
        return EXIT_FAILURE;
    }
    if (!perms) {
        fprintf(stderr, "Error: Required argument -p <permissions string> not found.\n");
        return EXIT_FAILURE;
    }

    // Validate permissions string
    if (!validate_permissions(perms)) {
        fprintf(stderr, "Error: Permissions string '%s' is invalid.\n", perms);
        return EXIT_FAILURE;
    }

    // Check if directory exists and is accessible
    struct stat st;
    if (stat(dir_path, &st) != 0) {
        fprintf(stderr, "Error: Cannot stat '%s'. %s.\n", dir_path, strerror(errno));
        return EXIT_FAILURE;
    }

    if (!S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a directory.\n", dir_path);
        return EXIT_FAILURE;
    }

    // Check if directory is readable
    if (access(dir_path, R_OK) != 0) {
        fprintf(stderr, "Error: Cannot open directory '%s'. Permission denied.\n", dir_path);
        return EXIT_FAILURE;
    }

    // Start recursive search
    search_directory(dir_path, perms);
    return EXIT_SUCCESS;
}
