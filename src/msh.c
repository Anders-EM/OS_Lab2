// P2-SSOO-23/24

//  MSH main file
// Write your msh source code here

// #include "parser.h"
#include <stddef.h> /* NULL */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

#define MAX_COMMANDS 8

// files in case of redirection
char filev[3][64];

// to store the execvp second parameter
char *argv_execvp[8];

void siginthandler(int param)
{
    printf("****  Exiting MSH **** \n");
    // signal(SIGINT, siginthandler);
    exit(0);
}

struct command
{
    // Store the number of commands in argvv
    int num_commands;
    // Store the number of arguments of each command
    int *args;
    // Store the commands
    char ***argvv;
    // Store the I/O redirection
    char filev[3][64];
    // Store if the command is executed in background or foreground
    int in_background;
};

void free_command(struct command *cmd)
{
    if ((*cmd).argvv != NULL)
    {
        char **argv;
        for (; (*cmd).argvv && *(*cmd).argvv; (*cmd).argvv++)
        {
            for (argv = *(*cmd).argvv; argv && *argv; argv++)
            {
                if (*argv)
                {
                    free(*argv);
                    *argv = NULL;
                }
            }
        }
    }
    free((*cmd).args);
}

void store_command(char ***argvv, char filev[3][64], int in_background, struct command *cmd)
{
    int num_commands = 0;
    while (argvv[num_commands] != NULL)
    {
        num_commands++;
    }

    for (int f = 0; f < 3; f++)
    {
        if (strcmp(filev[f], "0") != 0)
        {
            strcpy((*cmd).filev[f], filev[f]);
        }
        else
        {
            strcpy((*cmd).filev[f], "0");
        }
    }

    (*cmd).in_background = in_background;
    (*cmd).num_commands = num_commands - 1;
    (*cmd).argvv = (char ***)calloc((num_commands), sizeof(char **));
    (*cmd).args = (int *)calloc(num_commands, sizeof(int));

    for (int i = 0; i < num_commands; i++)
    {
        int args = 0;
        while (argvv[i][args] != NULL)
        {
            args++;
        }
        (*cmd).args[i] = args;
        (*cmd).argvv[i] = (char **)calloc((args + 1), sizeof(char *));
        int j;
        for (j = 0; j < args; j++)
        {
            (*cmd).argvv[i][j] = (char *)calloc(strlen(argvv[i][j]), sizeof(char));
            strcpy((*cmd).argvv[i][j], argvv[i][j]);
        }
    }
}

int history_size = 20;
struct command *history;
int head = 0;
int tail = 0;
int n_elem = 0;

/* myhistory */
// "history" points to an array of command structs
// create another pointer to iterate down the array?
// call print command every time?
// how to shift items once we reach a history length over 20?
// use head and tail pointers. update them using modulo size of buffer
// n_elem indicates number of elements in history

void addToHistory(char ***argvv, char filev[3][64], int in_background)
{

    if (n_elem == history_size)
    {
        free_command(&history[head]); // at tail or at head
    }

    store_command(argvv, filev, in_background, &history[tail]);

    if (n_elem == history_size)
    {
        head = (head + 1) % (history_size);
    }

    if (n_elem != history_size)
    {
        ++n_elem;
    }

    tail = (tail + 1) % (history_size);
}

void printHistory()
{

    // STILL NEED TO ADD FUNCTIONALITY FOR FILE REDIRECTION
    // PRINT TO STANDARD OUTPUT ERROR?

    int cur = head;
    int num = 0;

    do
    {

        fprintf(stderr, "%d ", num);

        for (int i = 0; i < history[cur].num_commands; ++i)
        {
            for (int j = 0; history[cur].argvv[i][j] != NULL; ++j)
            {
                fprintf(stderr, "%s", history[cur].argvv[i][j]);
                if (history[cur].argvv[i][j + 1] != NULL)
                {
                    fprintf(stderr, " ");
                }
            }
            if (i < history[cur].num_commands - 1)
            {
                fprintf(stderr, " | ");
            }
        }

        if (history[cur].in_background != 0)
        {
            fprintf(stderr, " &");
        }

        fprintf(stderr, "\n");

        cur = (cur + 1) % (history_size);
        num++;
    } while ((n_elem == history_size && cur != tail) || (n_elem != history_size && cur < tail));
}

/* myhistory */

/**
 * Get the command with its parameters for execvp
 * Execute this instruction before run an execvp to obtain the complete command
 * @param argvv
 * @param num_command
 * @return
 */
void getCompleteCommand(char ***argvv, int num_command)
{
    // reset first
    for (int j = 0; j < 8; j++)
        argv_execvp[j] = NULL;

    int i = 0;
    for (i = 0; argvv[num_command][i] != NULL; i++)
        argv_execvp[i] = argvv[num_command][i];
}

void executeCommand(struct command *curCmd, int *status)
{

    int numPipes = curCmd->num_commands - 1;

    int pipefds[2 * numPipes];

    for (int i = 0; i < numPipes; i++)
    {
        if (pipe(pipefds + (i * 2)) < 0)
        {
            perror("Error with pipe");
            exit(-1);
        }
    }

    for (int i = 0; i < curCmd->num_commands; i++)
    {
        int pid = fork();

        if (pid == 0)
        { // child process things

            if (i == 0)
            {
                if (strcmp(filev[0], "0") != 0)
                {
                    int fd_in = open(filev[0], O_RDONLY);

                    if (fd_in < 0)
                    {
                        perror("Error opening file");
                    }

                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                }
            }

            if (i > 0)
            {
                dup2(pipefds[(i - 1) * 2], STDIN_FILENO);
            }

            if (i < numPipes)
            {
                dup2(pipefds[i * 2 + 1], STDOUT_FILENO);
            }

            if (i == numPipes)
            {
                if (strcmp(filev[1], "0") != 0)
                {
                    int fd_out = open(filev[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);

                    if (fd_out < 0)
                    {
                        perror("Error opening file");
                    }

                    dup2(fd_out, STDOUT_FILENO);
                    close(fd_out);
                }
                if (strcmp(filev[2], "0") != 0)
                {
                    int fd_err = open(filev[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);

                    if (fd_err < 0)
                    {
                        perror("Error opening file");
                    }

                    dup2(fd_err, STDERR_FILENO);
                    close(fd_err);
                }
            }

            for (int j = 0; j < 2 * numPipes; j++)
            {
                close(pipefds[j]);
            }

            getCompleteCommand(curCmd->argvv, i);
            execvp(argv_execvp[0], argv_execvp);
            perror("Error in exec call");
            break;
        }
        else if (pid < 0)
        {
            perror("Error in fork");
            exit(-1);
        }
    }

    for (int i = 0; i < 2 * numPipes; i++)
    {
        close(pipefds[i]);
    }

    if (curCmd->in_background == 0)
    {
        for (int i = 0; i < curCmd->num_commands; i++)
        {
            wait(NULL);
        }
    }
    else
    {
        printf("Child process identifier: %d\n", getpid());
    }
}

/**
 * Main sheell  Loop
 */
int main(int argc, char *argv[])
{
    /**** Do not delete this code.****/
    int end = 0;
    int executed_cmd_lines = -1;
    char *cmd_line = NULL;
    char *cmd_lines[10];

    if (!isatty(STDIN_FILENO))
    {
        cmd_line = (char *)malloc(100);
        while (scanf(" %[^\n]", cmd_line) != EOF)
        {
            if (strlen(cmd_line) <= 0)
                return 0;
            cmd_lines[end] = (char *)malloc(strlen(cmd_line) + 1);
            strcpy(cmd_lines[end], cmd_line);
            end++;
            fflush(stdin);
            fflush(stdout);
        }
    }

    /*********************************/

    char ***argvv = NULL;
    int num_commands;

    history = (struct command *)malloc(history_size * sizeof(struct command));
    int run_history = 0;

    while (1)
    {
        int status = 0;
        int command_counter = 0;
        int in_background = 0;
        signal(SIGINT, siginthandler);

        if (run_history)
        {
            // just call history function
            printHistory();
            run_history = 0;
        }
        else
        {
            // Prompt
            write(STDERR_FILENO, "MSH>>", strlen("MSH>>"));

            // Get command
            //********** DO NOT MODIFY THIS PART. IT DISTINGUISH BETWEEN NORMAL/CORRECTION MODE***************
            executed_cmd_lines++;
            if (end != 0 && executed_cmd_lines < end)
            {
                command_counter = read_command_correction(&argvv, filev, &in_background, cmd_lines[executed_cmd_lines]);
            }
            else if (end != 0 && executed_cmd_lines == end)
                return 0;
            else
                command_counter = read_command(&argvv, filev, &in_background); // NORMAL MODE
        }
        //************************************************************************************************

        /************************ STUDENTS CODE ********************************/
        if (command_counter > 0)
        {
            if (command_counter > MAX_COMMANDS)
            {
                printf("Error: Maximum number of commands is %d \n", MAX_COMMANDS);
            }
            else if (strcmp(argvv[0][0], "myhistory") == 0)
            {
                if (argvv[0][1] != NULL)
                {
                    // myhistory w specified number
                    int a = atoi(argvv[0][1]);

                    if (a < 0 || a > 19)
                    {
                        printf("ERROR: Command not found\n");
                    }
                    else
                    {
                        // SOMEHOW RUN SPECIFIED COMMAND HERE
                        struct command specifiedCmd = history[(head + a) % history_size];
                        // call to generic function here
                        fprintf(stderr, "Running command %d\n", a);

                        // THIS WILL BE THE FUNCTION TO EXECUTE A COMMAND PASSED AS A PARAMETER
                        executeCommand(&specifiedCmd, &status);
                    }
                }
                else
                {
                    run_history = 1;
                }
            }
            else
            {
                // Print command
                print_command(argvv, filev, in_background);

                // store command sequence in history
                addToHistory(argvv, filev, in_background);

                // STORE ACTUAL COMMAND TO BE RUN
                struct command curCmd;
                store_command(argvv, filev, in_background, &curCmd);
                executeCommand(&curCmd, &status);
            }
        }
    }

    return 0;
}
