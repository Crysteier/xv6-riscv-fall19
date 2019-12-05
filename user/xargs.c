#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#define C(x)  ((x)-'@') 
#define MAX 500
#define MAXARG 10

void*
memcpy(void *dst, const void *src, uint n)
{
  return memmove(dst, src, n);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(2, "error: not enough arguments\n");
    }
    while (1) {
        int i = 0;
        int current_argument = argc;
        char buf[MAX];
        int new_arguments_count = 0;
        // vycisti pole od predoslych prikazov
        char **original_argv = malloc(MAXARG * sizeof(char*));
        memcpy(original_argv, argv, argc * sizeof(char*));
        // Nacitaj riadok
        gets(buf, sizeof(buf));
        if (buf[0] == '\0' || buf[0] == C('D')) {
            break;
        }
        int start = 0;
        int end = 0;
        while (1) {
            if (buf[i] == ' ' || buf[i] == '\n' || buf[i] == C('D')) {
                if ((i == 0) || ((i >= 1) && ((buf[i - 1] == ' ')))) {
                    i++;
                    if (buf[i] == '\n' || buf[i] == C('D')) {
                        break;
                    }
                    start++;
                    continue;
                }
                end = i - 1;
                char* new_argument = malloc(end - start + 2);
                memcpy(new_argument, buf + start, end - start + 1);

                // char new_argument[MAX];
                // memcpy(new_argument, buf + start, end - start + 1);

                new_argument[end - start + 1] = '\0';
                original_argv[current_argument++] = new_argument;
                new_arguments_count++;
                start = i + 1;
                end = 0;
            }
            if (buf[i] == '\n' || buf[i] == C('D')) {
                break;
            }
            i++;
        }

        int pid = fork();
        if (pid == 0) {
            // I am child
            exec(original_argv[1], original_argv + 1);
            fprintf(2,"error: exec failed\n");
            for (int k = argc; k < argc + new_arguments_count - 1; k++)
                free(original_argv[k]);
            exit();
        } else if (pid > 0) {
            // I am parent
            wait();
        } else {
            // Fork err
            fprintf(2,"error: fork failed\n");
        }
        
    }    
    exit();
}