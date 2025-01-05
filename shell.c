// -------------------------------------------------- Program 1 Unix Shell----------------------------------------------------------
// Peter Chang
// 4/13/2022
// --------------------------------------------------------------------------------------------------------------------
// Purpose - Using C, design a shell program that takes in user commands/inputs and executes each command as a separate process
// --------------------------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>


#define MAX_LINE 80 /* 80 chars per line, per command */
#define DELIMITERS " \t\n\v\f\r"


// ------------------------------------------ init_args() --------------------------------------------
// Description: Initialize args and make default content to be null
// ---------------------------------------------------------------------------------------------------
void init_args(char *args[]) {
    for(size_t i = 0; i != MAX_LINE / 2 + 1; ++i) {
        args[i] = NULL;
    }
}

// ------------------------------------------ init_command() --------------------------------------------
// Description: Initialize command and set default to empty string
// ---------------------------------------------------------------------------------------------------
void init_command(char *userCmd) {
    strcpy(userCmd, "");
}

// ------------------------------------------ refresh_args() --------------------------------------------
// Description: Refresh args by freeing old content and then set to null
// ---------------------------------------------------------------------------------------------------
void refresh_args(char *args[]) {
    while(*args) {
        free(*args);  // to avoid memory leaks
        *args++ = NULL;
    }
}

// ------------------------------------------ parse_input() --------------------------------------------
// Description: Take the input and parse it using tokens
// Return the number of args
// ---------------------------------------------------------------------------------------------------
size_t parse_input(char *args[], char *original_command) {
    size_t val = 0;
    char usrCmd[MAX_LINE + 1];
    strcpy(usrCmd, original_command);  // make a copy since `strtok` will modify it
    char *token = strtok(usrCmd, DELIMITERS);
    while(token != NULL) {
        args[val] = malloc(strlen(token) + 1);
        strcpy(args[val], token);
        ++val;
        token = strtok(NULL, DELIMITERS);
    }
    return val;
}

// ------------------------------------------ retrieve_input() --------------------------------------------
// Description: Get the command from input history. Also check for "!!"
// Return 1 for success
// ---------------------------------------------------------------------------------------------------
int retrieve_input(char *usrCmd) {
    char input_buffer[MAX_LINE + 1];
//    fflush(stdin);
    if(fgets(input_buffer, MAX_LINE + 1, stdin) == NULL) {
//        fprintf(stderr, "Failed to read input!\n");
//        sscanf(input_buffer, "%d", &temp_buffer);
        return 0;
    }
//    if(getc(stdin) == EOF){
//        fprintf(stderr, "Failed to read input!\n");
//        return 0;
//    }
    if(strncmp(input_buffer, "!!", 2) == 0) {
        if(strlen(usrCmd) == 0) {  // no history yet
            fprintf(stderr, "No commands in history!\n");
            return 0;
        }
        // keep the command unchanged and print it
        printf("%s", usrCmd);
        return 1;
    }
    strcpy(usrCmd, input_buffer);  // update the command
    return 1;
}

// ------------------------------------------ get_ampersand() --------------------------------------------
// Description: Check if & is at the end of the args. If so, remove from args and reduce size of args as needed
// Return 1 if ampersand found
// ---------------------------------------------------------------------------------------------------
int get_ampersand(char **args, size_t *size) {
    size_t len = strlen(args[*size - 1]);
    if(args[*size - 1][len - 1] != '&') {
        return 0;
    }
    if(len == 1) {  // remove this argument if it only contains '&'
        free(args[*size - 1]);
        args[*size - 1] = NULL;
        --(*size);  // reduce its size
    } else {
        args[*size - 1][len - 1] = '\0'; //set to 0
    }
    return 1;
}

// ------------------------------------------ get_redirection() --------------------------------------------
// Description: Check for redirection symbols in argument and remove symbol as well as input/output file from args array
// Return IO flag: 0 for none, 1 for input, 2 for output
// ---------------------------------------------------------------------------------------------------
unsigned get_redirection(char **args, size_t *size, char **iFile, char **oFile) {
    unsigned flag = 0;
    size_t remove_arr[4], remove_cnt = 0; //remove_arr will hold the index positions of the args to remove
    for(size_t i = 0; i != *size; ++i) {
        if(remove_cnt >= 4) {
            break;
        }
        if(strcmp("<", args[i]) == 0) {     // input
            remove_arr[remove_cnt++] = i;
            //handle if no input file is provided by printing error message
            if(i == (*size) - 1) {
                fprintf(stderr, "No input file provided!\n");
                break;
            }
            flag |= 1;
            *iFile = args[i + 1];
            remove_arr[remove_cnt++] = ++i;
        } else if(strcmp(">", args[i]) == 0) {   // output
            remove_arr[remove_cnt++] = i;
            //handle if no output file is provided by printing error message
            if(i == (*size) - 1) {
                fprintf(stderr, "No output file provided!\n");
                break;
            }
            flag |= 2;
            *oFile = args[i + 1];
            remove_arr[remove_cnt++] = ++i;
        }
    }
    //Remove I/O indicators and filenames from arguments
    for(int i = remove_cnt - 1; i >= 0; --i) {
        size_t pos = remove_arr[i];  // the index of arg to remove
        // printf("%lu %s\n", pos, args[pos]);
        while(pos != *size) { //loop while pos variable is not equal to the size
            args[pos] = args[pos + 1]; //set pos in args equal to pos + 1
            ++pos; //increment
        }
        --(*size); //decrement
    }
    return flag;
}

// ------------------------------------------ redirect_io() --------------------------------------------
// Description: Open the files and redirect I/O. Comfigure the chmod settings to allow user to open file
// io_flag: the flag for IO redirection
// iFile: file name for input
// oFile: file name for output
// iDesc: file descriptor of input file
// oDesc: file descriptor of output file
// ---------------------------------------------------------------------------------------------------
int redirect_io(unsigned io_flag, char *iFile, char *oFile, int *iDesc, int *oDesc) {
    // printf("IO flag: %u\n", io_flag);
    if(io_flag & 2) {  // redirecting output
        *oDesc = open(oFile, O_WRONLY | O_CREAT | O_TRUNC, 0666); //original: 644 update to --> 0666
        //if file can't be open then print error message
        if(*oDesc < 0) {
            fprintf(stderr, "Failed to open the output file: %s\n", oFile);
            return 0;
        }
        // printf("Output To: %s %d\n", output_file, *oDesc);
        dup2(*oDesc, STDOUT_FILENO); //copies oDesc into STDOUT_FILENO (or slot 1) of the file descriptor
    }
    if(io_flag & 1) { // redirecting input
        *iDesc = open(iFile, O_RDONLY, 0644);
        //if file can't be open then print error message
        if(*iDesc < 0) {
            fprintf(stderr, "Failed to open the input file: %s\n", iFile);
            return 0;
        }
        // printf("Input from: %s %d\n", iFile, *iDesc);
        dup2(*iDesc, STDIN_FILENO); //copies iDesc into STDIN_FILNO (or slot 0) of the file descriptor
    }
    return 1;
}

// ------------------------------------------ close_file() --------------------------------------------
// Description: Closes the input/output files
// iDesc: file descriptor of input file
// oDesc: file descriptor of output file
// return type void
// ---------------------------------------------------------------------------------------------------
void close_file(unsigned io_flag, int iDesc, int oDesc) {
    if(io_flag & 2) { //checks if ioflag is 2 meaning output
        close(oDesc);
    }
    if(io_flag & 1) { //checks if ioflag is 1 meaning input
        close(iDesc);
    }
}

// ------------------------------------------ check_pipe() --------------------------------------------
// Description: Check for pipe and then split the args
// args: args list for the first command
// argNum: # of args for the first command
// args2: args list for the second command
// argNum2: # of args for the second command
// return type void
// ---------------------------------------------------------------------------------------------------
void check_pipe(char **args, size_t *argNum, char ***args2, size_t *argNum2) {
    size_t i = 0;
    while(i != *argNum){
        if (strcmp(args[i], "|") == 0) {
            free(args[i]);
            args[i] = NULL;
            *argNum2 = *argNum -  i - 1;
            *argNum = i;
            *args2 = args + i + 1;
            break;
        }
        ++i;
    }
}

// ------------------------------------------ exec_command() --------------------------------------------
// Description: Run cmd
// args: args list
// argNum: # of args
// Return 1 for success 0 for fail
// ---------------------------------------------------------------------------------------------------
int exec_command(char **args, size_t argNum) {
    //Detect '&' to determine whether to run concurrently
    int runTogether = get_ampersand(args, &argNum);
    //Detect pipe
    char **args2;
    size_t argNum2 = 0;
    check_pipe(args, &argNum, &args2, &argNum2);
    //Create a child process and execute the command
    pid_t pid = fork();
    if(pid < 0) {   // fork failed
        fprintf(stderr, "Failed to fork!\n");
        return 0;
    } else if (pid == 0) { // child process
        if(argNum2 != 0) {    // pipe
            /* Create pipe */
            int fd[2];
            pipe(fd);
            //Fork into another two processes
            pid_t pid2 = fork();
            if(pid2 > 0) {  // child process for the second command
                //Redirect I/O
                char *iFile, *oFile;
                int iDesc, oDesc;
                unsigned io_flag = get_redirection(args2, &argNum2, &iFile, &oFile);    // 1 for output, 0 for input
                io_flag &= 2;   // disable input redirection
                if(redirect_io(io_flag, iFile, oFile, &iDesc, &oDesc) == 0) {
                    return 0;
                }
                close(fd[1]);
                dup2(fd[0], STDIN_FILENO); //copies fd[0] to fd slot 0 (stdin)
                wait(NULL);     // wait for the first command to finish
                execvp(args2[0], args2);
                close_file(io_flag, iDesc, oDesc);
                close(fd[0]);
                fflush(stdin); //clears out stdin
            } else if(pid2 == 0) {  // grandchild process for the first command
                //Redirect I/O
                char *iFile, *oFile;
                int iDesc, oDesc;
                unsigned io_flag = get_redirection(args, &argNum, &iFile, &oFile);    // 1 for output, 0 for input
                io_flag &= 1;   // disable output redirection
                if(redirect_io(io_flag, iFile, oFile, &iDesc, &oDesc) == 0) {
                    return 0;
                }
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO); //copies fd[1] to fd slot 1 (stdout)
                execvp(args[0], args);
                close_file(io_flag, iDesc, oDesc);
                close(fd[1]);
                fflush(stdin); //clears out stdin
            }
        } else {    // no pipe
            //Redirect I/O
            char *iFile, *oFile;
            int iDesc, oDesc;
            unsigned io_flag = get_redirection(args, &argNum, &iFile, &oFile);    // 1 for output, 0 for input
            if(redirect_io(io_flag, iFile, oFile, &iDesc, &oDesc) == 0) {
                return 0;
            }
            execvp(args[0], args);
            close_file(io_flag, iDesc, oDesc);
            fflush(stdin); //clears out stdin
        }
    } else { // parent process
        if(!runTogether) { // parent and child run concurrently
            wait(NULL);
        }
    }
    return 1;
}

int main(void) {
    char *args[MAX_LINE / 2 + 1]; // command line (of 80) has max of 40 arguments
    char usrCmd[MAX_LINE + 1];
    int should_run = 1; //flag to determine when to exit program
    init_args(args);
    init_command(usrCmd);
    while (should_run) {
        printf("osh>");
        fflush(stdout); //clears out stdout
        fflush(stdin); //clears out stdin
        // Make args empty before parsing
        refresh_args(args);
        // Get input and parse it
        if(!retrieve_input(usrCmd)) {
            continue;
        }
        size_t argNum = parse_input(args, usrCmd);
        // Continue or exit
        if(argNum == 0) { // empty input
            printf("Please enter the command! (or type \"exit\" to exit)\n");
            continue;
        }
        if(strcmp(args[0], "exit") == 0) {
            should_run = 0;
        }
        // Run command
        exec_command(args, argNum);
    }
    refresh_args(args);     // avoid memory leaks
    return 0;
}
