#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

struct dirent {
  ushort inum;
  char name[14];
};

char * my_strcat(char *dest, const char *src) {
    int i,j;
    for (i = 0; dest[i] != '\0'; i++);
    for (j = 0; src[j] != '\0'; j++)
        dest[i+j] = src[j];
    dest[i+j] = '\0';
    return dest;
}

void browse_folder(int folder_fd, char* current_path, char* target_name) {
    struct dirent e;
    struct stat file_info;
    int current_fd;
    while (read(folder_fd, &e, sizeof(e)) == sizeof(e)) {
        if (e.name[0] != '\0') {
            if ((strcmp(e.name,".") == 0) || (strcmp(e.name,"..") == 0)) {
                continue;
            }
            char str[256];
            strcpy(str, current_path);
            my_strcat(str, "/");
            my_strcat(str, e.name);
            if ((current_fd = open(str, 0) ) < 0) {
                fprintf(2, "error: cannot open file\n");
                exit();
            }
            if (fstat(current_fd, &file_info) < 0) {
                fprintf(2, "error: failed fstat\n");
                exit();
            }
            if (strcmp(e.name, target_name) == 0) {
              printf("%s/%s\n", current_path, e.name);
            }
            if (file_info.type == T_DIR) {
                browse_folder(current_fd, str, target_name);
            }
            close(current_fd);
        }
    }
    close(folder_fd);
}

int main(int argc, char *argv[]) {
    struct stat file_info;
    int fd;
    if (argc != 3) {
        fprintf(2, "error: wrong number of arguments, use like $ find . a\n");
        exit();
    }

    if ((fd = open(argv[1], 0) ) < 0) {
        fprintf(2, "error: cannot open folder\n");
        exit();
    }
    
    if (fstat(fd, &file_info) < 0) {
        fprintf(2, "error: failed fstat\n");
        exit();
    }

    if (file_info.type != T_DIR) {
        fprintf(2, "error: not a folder\n");
        exit();
    }

    // printf("Folder loaded sucessfuly!\n");
    browse_folder(fd, argv[1], argv[2]);

    exit();
}