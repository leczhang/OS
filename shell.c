#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#define PATH_MAX        4096

int getcmd(char *prompt, char **args, int *background, char **output){
    int length, i = 0;
    char *token, *loc;
    char *line = NULL;
    size_t linecap = 0;
    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);
    if (length <= 0) {
        exit(-1);
    }

    // Check if background is specified..
    // Background = 1 if specified, 0 otherwise
    if ((loc = index(line, '&')) != NULL) {
        *background = 1;
        *loc = ' ';
    } 
    else *background = 0;

    // Allocate memory for args
    /*
    int arg_count=0;
    char* s = line;
    for (arg_count=0; s[i]; (s[i]==' ' || s[i]=='\t' || s[i]=='\n') ? arg_count++ : *s++);
    args = malloc((arg_count+1) * sizeof(char*));
    */

    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++){
            if (token[j] <= 32){
                token[j] = '\0';
            }
        }

        // Allocate memory to each arg
        /*
        if (strlen(token) > 0){
            args[i] = malloc((strlen(token)+1) * sizeof(char));
            strcpy(args[i], token);
            i++;
        }
        */
        if(strcmp(token, ">") == 0){
            token = strsep(&line, " \t\n");
            for (int j = 0; j < strlen(token); j++){
                if (token[j] <= 32){
                    token[j] = '\0';
                }
            }
            args[i++] = NULL;
            *output = token;
            return 0;
        }

        if(strlen(token) > 0)
        args[i++] = token;
        else
        args[i++] = NULL;
    }

    /*
    if(token)
    free(token);
    if(loc)
    free(loc);
    if(line)
    free(line);
    */

    return i;
}

int execBuiltIn(char** args, int *backgroundPids, int backgroundPidsLen){
    if(strcmp(args[0], "cd") == 0){
    	if(args[1] == NULL){
            return 1;
        }
		else{
			if(chdir(args[1])!=0)
			fprintf(stderr, "Error changing directories!");
            return 0;
		}
    }
    else if(strcmp(args[0], "pwd") == 0){
    	char cwd[PATH_MAX];
		if (getcwd(cwd, sizeof(cwd)) != NULL) {
		    printf("%s\n", cwd);
		}
		else {
			fprintf(stderr, "Error printing current working directory!");
		}
        return 0;
    }
    else if(strcmp(args[0], "exit") == 0){
        for(int i=0; i<backgroundPidsLen; i++){
            kill(backgroundPids[i], SIGKILL);
        }
        exit(EXIT_SUCCESS);
    }
    else if(strcmp(args[0], "fg") == 0){
        if(args[1] == NULL){
            return 1;
        }
        int jobNumber = atoi(args[1]);
        if(jobNumber >= 1 && jobNumber <= backgroundPidsLen){
            waitpid(backgroundPids[jobNumber-1], NULL, 0);
            return 0;
        }
        else return 1;
    }

    else if(strcmp(args[0], "jobs") == 0){
        for(int i=0; i<backgroundPidsLen; i++){
            printf("\nProgram %d PID is: [%d]", i, backgroundPids[i]);
        }
        return 0;
    }
    else
    return 1;
}

bool isBuiltIn(char **args){
    if(strcmp(args[0], "cd") == 0)
    return true;
    else if(strcmp(args[0], "pwd") == 0)
    return true;
    else if(strcmp(args[0], "exit") == 0)
    return true;
    else if(strcmp(args[0], "fg") == 0)
    return true;
    else if(strcmp(args[0], "jobs") == 0)
    return true;
    else
    return false;
}

int main(void){
    char *args[20];
    int bg;
    int pid;
    bool builtIn;
    int backgroundPids[100];
    int pidIndex = 0;
    char *output = NULL;

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    while(1) {
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg, &output);
        
        if(isBuiltIn(args)){
            execBuiltIn(args, backgroundPids, pidIndex);
        }
        else if(output != NULL){
                    int out = open(output, O_RDWR|O_CREAT|O_APPEND, 0600);
                    int save_out = dup(fileno(stdout));
                    if (-1 == dup2(out, fileno(stdout))) { perror("cannot redirect stdout"); return 255; }
                    execvp(args[0], args);
                    fflush(stdout); close(out);
                    dup2(save_out, fileno(stdout));
                    close(save_out);

        }
        else{
            pid = fork();
            if(pid < 0){
                fprintf(stderr, "Fork Failed");
                return 1;
            }
            //Foreground child process, terminate upon ctrl-c, ignore ctrl-z
            else if(pid == 0 && bg == 0){
                signal(SIGINT, SIG_DFL);
                signal(SIGTSTP, SIG_IGN);
                execvp(args[0], args);
            }
            //Background child process, ignore ctrl-c, ignore ctrl-z
            else if(pid == 0 && bg == 1){
                signal(SIGINT, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
                execvp(args[0], args);
            }
            //Parent process, ignore-c, ignore ctrl-z
            else if(pid > 0 && bg == 0){
                waitpid(pid, NULL, 0);
                signal(SIGINT, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
            }
            else if(pid > 0 && bg == 1){
                backgroundPids[pidIndex++] = pid;
                signal(SIGINT, SIG_IGN);
                signal(SIGTSTP, SIG_IGN);
            }
            else{
                fprintf(stderr, "Error!");
                return 1;
            }
        }

        // Free memory allocated to args
        /*
        for(int i=0; args[i]!=NULL; i++){
            if(args[i])
            free(args[i]);
        }
        free(args); */

        /* the steps can be..:
        (1) fork a child process using fork()
        (2) the child process will invoke execvp()
        (3) if background is not specified, the parent will wait,
        otherwise parent starts the next command... */
    }
}