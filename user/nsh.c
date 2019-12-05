#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fcntl.h"

#define MAXARGS      10
#define MAXARGLENGTH 20
#define MAXINPUT     100
#define SHELLSIGN    "@ "

#define EXECUTE      1
#define REDIR_INPUT  2
#define REDIR_OUTPUT 3
#define PIPE         4

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";
int is_pipe_in_chain;

struct command {
    int type;
    char argv[MAXARGS][MAXARGLENGTH];
    int argc;
};

void* memcpy(void *dst, const void *src, uint n) {
  return memmove(dst, src, n);
}

void error(char *s) {
  fprintf(2, "%s\n", s);
  exit(-1);
}

void check_for_exit(char* command_line) {
    if ((command_line == 0) || (command_line[0] == '\0')) {
        exit(0);
    }
}

void load_line(char *buf, int buf_size) {
  memset(buf, 0, buf_size);
  gets(buf, buf_size);
}

int parse_line(char* buf, char argv[MAXARGS][MAXARGLENGTH]) {
    int i = 0;
    int argc = 0;
    int word_start = 0;
    int word_end = 0;
    if (buf[0] == '\n' || buf[0] == '\0') {
        return 0;
    }
    while (1) {
        if (argc >= MAXARGS) {
            error("error: too many arguments passed.");
        }
        if (strchr(whitespace, buf[i])) {
            if ((i == 0) || ((i >= 1) && ((buf[i - 1] == ' ')))) {
                i++;
                if (buf[i] == '\n') {
                    break;
                }
                word_start++;
                continue;
            }
            word_end = i - 1;
            memcpy(argv[argc], buf + word_start, word_end - word_start + 1);
            argv[argc][word_end - word_start + 1] = '\0';
            argc++;
            word_start = i + 1;
            word_end = 0;
        }
        if (buf[i] == '\n') {
                break;
        }
        i++;
    }
    return argc;
}

void parse_commands(char argv[MAXARGS][MAXARGLENGTH], int argc, struct command commands[], int* command_count) {
    *command_count = 0;
    int argument_count = 0;
    if (argc == 0 || (strchr(symbols, argv[0][0]))) {
        error("error: cannot parse command");
    }
    commands[*command_count].argc = 0;
    for (int i = 0; i < argc; i++) {
        // Nasiel som specialny znak
        if (strchr(symbols, argv[i][0])) {
            if (!commands[*command_count].type) {
                commands[*command_count].type = EXECUTE;
            }
            argument_count = 0;
            *command_count = *command_count + 1;
            commands[*command_count].argc = 0;
            if (argv[i][0] == '<') {
                commands[*command_count].type = REDIR_INPUT;
            } else if (argv[i][0] == '>') {
                commands[*command_count].type = REDIR_OUTPUT;
            } else if (argv[i][0] == '|') {
                is_pipe_in_chain = *command_count;
                commands[*command_count].type = PIPE;
            } else {
                error("error: cannot parse command");
            }
            memcpy(commands[*command_count].argv[argument_count++], argv[i], strlen(argv[i]) + 1);
            commands[*command_count].argc++;
            if (commands[*command_count].type == PIPE) {
                *command_count = *command_count + 1;
                argument_count = 0;
            }
            continue;
        }
        // Som specialny command
        if (commands[*command_count].type == REDIR_INPUT || commands[*command_count].type == REDIR_OUTPUT) {
            memcpy(commands[*command_count].argv[argument_count++], argv[i], strlen(argv[i]) + 1);
            commands[*command_count].argc++;
            if (i + 1 == argc) break;
            if (!strchr(symbols, argv[i+1][0])) {
                *command_count = *command_count + 1;
                argument_count = 0;
            }
            continue;
        }
        // Som normalny command
        memcpy(commands[*command_count].argv[argument_count++], argv[i], strlen(argv[i]) + 1);
        commands[*command_count].argc++;
    }
    if (*command_count == 0) {
        commands[*command_count].type = EXECUTE;
        *command_count = 1;
    } else if (commands[*command_count].type == 0) {
        commands[*command_count].type = EXECUTE;
        *command_count = *command_count + 1;
    }else {
        *command_count = *command_count + 1;
    }
}

int redirect(int command_type, char* file_name) {
    if (command_type == REDIR_INPUT) {
        // Redirect input
        close(0);
        if (open(file_name, O_RDONLY) < 0) {
            error("error: cannot open requested file!");
        }
        return 1;
    } else if(command_type == REDIR_OUTPUT) {
        // Redirect output
        close(1);
        if (open(file_name, O_WRONLY|O_CREATE) < 0) {
            error("error: cannot open requested file!");
        }
        return 1;
    }
    return 0;
}

void execute_commands_from_range(struct command commands[], int start_command_index, int end_command_index) {
    for (int i = start_command_index; i < end_command_index; i++) {
        // Pozri sa, ci command potrebuje redirect
        if (commands[i].type == EXECUTE) {
            int pid = fork();
            if (pid == 0) {
                // I am child
                // 1. step REDIRECT if necessary
                struct command next_command = commands[i+1];
                if ((i + 1 < end_command_index) && (next_command.type == REDIR_INPUT || next_command.type == REDIR_OUTPUT || next_command.type == PIPE)) {
                    if (next_command.type == REDIR_INPUT || next_command.type == REDIR_OUTPUT) {
                        int count = 2;
                        while ((redirect(next_command.type, next_command.argv[1]) != 0) && (i + count != end_command_index)) {
                            next_command = commands[i+count];
                            count++;
                        }                        
                    }
                }
                // 2. step EXECUTE command
                char* argv[MAXARGS];
                for (int j = 0; j < commands[i].argc; j++) {
                    argv[j] = commands[i].argv[j];
                }
                argv[commands[i].argc] = 0;
                exec(argv[0], argv);
                error("error: exec failed!");
                exit(-1);
            } else if (pid > 0) {
                // I am parent       
                wait(0);   
            } else {
                // Fork err
                error("error: fork failed");
            }
        }
    }
}

void execute_commands(struct command commands[], int command_count) {
    if (is_pipe_in_chain > 0) {
        int pid = fork();
        if (pid == 0) {
            // I am child
            int fds[2];
            if (pipe(fds) < 0) {
                error("error: cannot create pipe!");
            }
            if (fork() == 0) {
                close(1);
                dup(fds[1]);
                close(fds[0]);
                close(fds[1]);
                execute_commands_from_range(commands, 0, is_pipe_in_chain);
                exit(0);
            }
            if (fork() == 0) {
                close(0);
                dup(fds[0]);
                close(fds[0]);
                close(fds[1]);
                execute_commands_from_range(commands, is_pipe_in_chain, command_count);
            }
            close(fds[0]);
            close(fds[1]);
            wait(0);
            wait(0);
            exit(0);
        } else if (pid > 0) {
            // I am parent
            wait(0);
        } else {
            // Fork err
            fprintf(2,"error: fork failed\n");
        }
    } else {
        execute_commands_from_range(commands, 0, command_count);
    }

}

int main() {
    int fd, argc, command_count;

    // Kontrola otvorenosti stdin, stdout, stderr
    while ((fd = open("fdtest",O_RDWR)) >= 0) {
        if (fd > 2) {
            close(fd);
            break;
        }
    }
    
    // Hlavny chod shellu
    while (1) {
        is_pipe_in_chain = -1;
        char buf[MAXINPUT];
        char argv[MAXARGS][MAXARGLENGTH];
        struct command commands[MAXARGS];
        argc = 0;

        printf(SHELLSIGN);
        // Nacitanie riadku
        load_line(buf, sizeof buf);
        check_for_exit(buf);
        if (buf[0] == '\n') {
            continue;
        }
        argc = parse_line(buf, argv);
        parse_commands(argv, argc, commands, &command_count);
        execute_commands(commands, command_count);
    }

    exit(0);
}

