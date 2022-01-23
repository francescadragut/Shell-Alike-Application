#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <fcntl.h>
#define _GNU_SOURCE

char *string_for_empty_commands;
void readstring(){
  ssize_t buffer_size = 0;
  string_for_empty_commands = NULL;
  int status = getline(&string_for_empty_commands, &buffer_size, stdin);
  if(status == -1){
    if(feof(stdin)){
      exit(EXIT_SUCCESS);
    }
    else{
      printf("An error occured at readline.");
      exit(EXIT_FAILURE);
    }
  }
}

char** parse(char *command){
  int buffer_size = 64;
  char *word;
  char **parsed_command_array = malloc(buffer_size * sizeof(char *));
  int index = 0;
  if(!parsed_command_array){
    fprintf(stderr, "Memory allocation error.\n");
    exit(EXIT_FAILURE);
  }
  word = strtok(command," \t\r\n\a");
  while(word != NULL){
    parsed_command_array[index] = word;
    index++;
    word = strtok(NULL," \t\r\n\a");
  }
  parsed_command_array[index] = 0;
  return parsed_command_array;
}

int redirect_pipe;
char *redirect_file_pipe;

char **parse_pipe(char *command){
  int buffer_size = 64;
  char *word;
  char **parsed_command_array = malloc(buffer_size * sizeof(char *));
  int index = 0;
  if(!parsed_command_array){
    fprintf(stderr, "Memory allocation error.\n");
    exit(EXIT_FAILURE);
  }
  word = strtok(command,"|\n");
  while(word != NULL){
     if(word[strlen(word)-1]==' '){
        word[strlen(word)-1] = '\0';
      }
      if(word[0] != ' ')
        parsed_command_array[index] = word;
      else
        parsed_command_array[index] = word+1;
      index++;
      word = strtok(NULL,"|\n");     
  }
  char *last = malloc(100);
  strcpy(last,parsed_command_array[index-1]);

  char **last_args = malloc(64*sizeof(char*));
  int length = 0;

  char *w;
  w = strtok(last," \n");
  while(w){
    last_args[length] = w;
    length ++;
    w = strtok(NULL," \n");
  }

  redirect_pipe = -1;
  redirect_file_pipe = malloc(100);
  for(int i = 0; i < length; i++){
    if(strcmp(last_args[i],">")==0){
      redirect_pipe = 1;
      last_args[i] = "";
      if(last_args[i+1] != NULL){
        strcpy(redirect_file_pipe,last_args[i+1]);
        last_args[i+1] = "";
      }
      i+=2;
      length -=2;
    }
    else if(strcmp(last_args[i],">>")==0){
      redirect_pipe = 2;
      last_args[i] = "";
      if(last_args[i+1] != NULL){
        strcpy(redirect_file_pipe,last_args[i+1]);
        last_args[i+1] = "";
      }
      i+=2;
      length -=2;
    }
  }
    
  char *new = malloc(100);
  for(int i = 0; i < length; i++){
    strcat(new,last_args[i]);
    strcat(new," ");
  }
  parsed_command_array[index-1] = new;
  return parsed_command_array;
}

char *launch(char **parsed_command_array){
  pid_t pid, wait_pid;
  
  pid = fork();
  if(pid == 0){
    if(execvp(parsed_command_array[0],parsed_command_array) == -1){
      printf("%s: command not found\n",parsed_command_array[0]);
      exit(EXIT_FAILURE);
    }
  }
  else if(pid < 0){
    perror("Error forking.\n");
  }
  else{
    wait(NULL);
  }
  return "Success!\n";
}

char *execute(char **parsed_command_array){
  if(parsed_command_array[0]==NULL || strcmp(parsed_command_array[0],"\n")==0){
    return "No command provided.\n";
  }
  if(strcmp(parsed_command_array[0],"exit")==0)
    return 0;

  return launch(parsed_command_array);
}

int is_pipe(char *command){
  if(strchr(command,'|') != NULL)
    return 1;
  return 0;
}

int execute_pipes(char *command, char **parsed_command_array, int arg, char **argv){

  int number_pipes = 0;
  char *string = strchr(command,'|');
  while(string != NULL){
    number_pipes++;
    string = strchr(string+1,'|');
  }
    
  if(redirect_pipe == 1){
    if(redirect_file_pipe)
      remove(redirect_file_pipe);
    int file = open(redirect_file_pipe, O_WRONLY | O_CREAT, 0777);
    dup2(file,STDOUT_FILENO);
    close(file);
  }
  else if(redirect_pipe == 2){
    int file = open(redirect_file_pipe, O_WRONLY | O_CREAT, 0777);
    dup2(file,STDOUT_FILENO);
    close(file);
  }
  
  int pipe_ends[number_pipes*2];
  for(int i = 0; i < number_pipes; i++){
    if(pipe(pipe_ends+i*2)==-1){
      printf("Pipe couldn't be generated.");
      return 1;
    }
  }
  int index_f = 0, index_d = 0;
  pid_t pid;
  while(parsed_command_array[index_f]){
    pid = fork();
    if(pid < 0){
      perror("Process couldn't be created.");
      return 1;
    }
    else{
      if(pid == 0){
        if(parsed_command_array[index_f+1])
          dup2(pipe_ends[index_d+1],STDOUT_FILENO);
        if(index_d != 0)
          dup2(pipe_ends[index_d-2],STDIN_FILENO);
        for(int j = 0; j < number_pipes*2; j++)
          close(pipe_ends[index_f]);
        char **cmd = parse(parsed_command_array[index_f]);
        execvp(cmd[0],cmd);
      }
    }
    index_f++;
    index_d+=2;
  }
  for(int i = 0; i < number_pipes*2; i++)
    close(pipe_ends[i]);
  for(int i = 0; i < number_pipes; i++)
    wait(NULL);
  return 1;
}

void universal_help(){
  printf("\nThe available commands in this shell are the following:\n\n");
  printf("     cat   Usage: cat [OPTION]... [FILE]...\n");
  printf("           Concatenate FILE(s) to standard output.\n\n");
  printf("                -b, --number-nonblank    number nonempty output lines,\n");
  printf("                                         overrides -n\n");
  printf("                -E, --show-ends          display $ at end of each line\n");
  printf("                -n, --number             number all output lines\n");
  printf("                -s, --squeeze-blank      suppress repeated empty output lines\n\n");
  printf("     head  Usage: head [OPTION]... [FILE]...\n");
  printf("           Print the first 10 lines of each FILE to standard output.\n\n");
  printf("                -c, --bytes=[-]NUM       print the first NUM bytes of each file;\n");                         
  printf("                -n, --lines=[-]NUM       print the first NUM lines instead\n");
  printf("                                         of the first 10;\n");
  printf("                -q, --quiet, --silent    never print headers giving file names\n");
  printf("                -v, --verbose            always print headers giving file names\n\n");
  printf("     env   Usage: env [OPTION]... [-] [NAME=VALUE]... [COMMAND [ARG]...]\n");
  printf("           Set each NAME to VALUE in the environment and run COMMAND.\n\n");
  printf("                -u, --unset=NAME     remove variable from the environment\n\n\n");
  printf("     Pipes\n");
}

char * exit_process(char **parsed_command_array){
  return "Exited successfully\n.";
  exit(EXIT_SUCCESS);
}

int checker(char *line){
  char c;
  char *copy = strdup(line);
  do{
    c = *(copy++);
    if(c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\0')
      return 0;
  }while (c != '\0');
  return 1;
}

int file_checker(char *filename){
  if(strcmp(filename,"-") == 0){
    return 1;
  }
  else{
    int l = strlen(filename);
    char *extension = &filename[l-4];
    if(strcmp(extension,".txt") == 0){
        return 1;
    }
  }
  return 0;
}

int simple_compare(const void* a, const void* b){
    return strcmp(*(const char**)a, *(const char**)b);
}
  
void sort(char **array, int n){
    qsort(array, n, sizeof(char*), simple_compare);
}

/**IMPLEMENT CAT**/

int cat_param_checker(char *param){
  if(strcmp(param,"-b") == 0)
    return 1;
  else if(strcmp(param,"-E") == 0)
    return 2;
  else if(strcmp(param,"-n") == 0)
    return 3;
  else if(strcmp(param,"-s") == 0)
    return 4;
  else return -1;
}
void cat_b2(char **parsed_command_array, char **files, int n, int start, int ind, int flag, char *red){
  int count = ind;
  
  if(flag == -1){
    size_t length = 0;
    FILE *ff;
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(!checker(string)){
              count++;
              printf("     %d  %s", count, string);
            }
          else printf(" %s", string);
        }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f, *ff;
    f = fopen(red,"a+");
    char *string = malloc(100);
    size_t length = 0;
    for(int i = start; i < n; i++){
      ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          while(status = getline(&string, &length, ff)!=-1){
            if(status != -1){
              if(!checker(string)){
                count++;
                fprintf(f,"     %d  %s", count, string);
              }
              else fprintf(f," %s", string);
            }
          }
          fclose(ff);
        }
    }
  fclose(f);
  }
}
  
void cat_b(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  FILE *ff;
  size_t length = 0;
  if(flag == -1){
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(!checker(string_for_empty_commands)){
            count++;
            printf("     %d  %s", count, string_for_empty_commands);
          }
          else{
            printf(" %s", string_for_empty_commands);
          }
        }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          if(!checker(string_for_empty_commands)){
            count++;
            printf("     %d  %s", count, string_for_empty_commands);
          }
          else{
            printf(" %s", string_for_empty_commands);
          }
        }while(string_for_empty_commands != 0);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{   
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_b2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(!checker(string)){
            count++;
            printf("     %d  %s\n", count, string);
          }
          else{
            printf(" %s\n", string);
          }
        }while(string);
        if(string)
          free(string);
      }
      else{
        FILE *file_parse = fopen(files[i], "r");
        size_t length = 0;
        ssize_t field;
        char *string = malloc(100);
        while ((field = getline(&string, &length, file_parse)) != -1) {
          if(!checker(string)){
            count++;
            printf("     %d  %s", count, string);
          }
          else{
            printf(" %s", string);
          }
        }
        if(string)
          free(string);
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(!checker(string_for_empty_commands)){
            count++;
            fprintf(f,"     %d  %s", count, string_for_empty_commands);
          }
          else{
            fprintf(f," %s", string_for_empty_commands);
          }
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
            readstring(string_for_empty_commands);
            if(!checker(string_for_empty_commands)){
              count++;
              fprintf(f,"     %d  %s", count, string_for_empty_commands);
            }
            else{
              fprintf(f," %s", string_for_empty_commands);
            }
        }while(string_for_empty_commands);
      }
      
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_b2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(!checker(string)){
            count++;
            fprintf(f,"     %d  %s\n", count, string);
          }
          else fprintf(f," %s\n", string);
          
        }while(string);
        if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(!checker(string)){
                count++;
                fprintf(f,"     %d  %s", count, string);
              }
              else fprintf(f," %s", string);
            }
          }while(status != -1);
          fclose(ff);
        }
      }
    } 
    fclose(f);
  }
  else if(flag == 3){
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        if(!checker(string)){
          count++;
          printf("     %d  %s", count, string);
        }
        else{
          printf(" %s", string);
        }
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            if(!checker(string)){
              count++;
              printf("     %d  %s", count, string);
            }
            else{
              printf(" %s", string);
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(!checker(string)){
              count++;
              printf("     %d  %s", count, string);
            }
            else{
              printf(" %s", string);
            }
          }
          if(string)
            free(string);
        }   
        
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          if(!checker(string)){
              count++;
              printf("     %d  %s", count, string);
            }
            else{
              printf(" %s", string);
            }
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            if(!checker(lines[j])){
              count++;
              printf("     %d  %s", count, lines[j]);
            }
            else{
              printf("%s", lines[j]);
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(!checker(string)){
              count++;
              printf("     %d  %s", count, string);
            }
            else{
              printf(" %s", string);
            }
          }
          if(string)
            free(string);
          }         
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        if(!checker(lines[i])){
          count++;
          printf("     %d  %s", count, lines[i]);
        }
        else{
          printf(" %s", lines[i]);
        }
      }
    }
  }
}
void cat_E2(char **parsed_command_array, char **files, int n, int start, int flag, char *red){
  if(flag == -1){
    char *string = malloc(100);
    FILE *file_parse;
    size_t length = 0;
    ssize_t field;
    for(int i = start; i < n; i++){
      file_parse = fopen(files[i], "r");
      while ((field = getline(&string, &length, file_parse)) != -1) {
        for(int i = 0; i < strlen(string)-1; i++){
            printf("%c",string[i]);
          }
        printf("$\n");
      }
    }
    if(string)
      free(string);
  }
  else if(flag == 1 || flag == 2){
    FILE *f, *ff;
    f = fopen(red,"a+");
    char *string = malloc(100);
    size_t length = 0;
    for(int i = start; i < n; i++){
      ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          while(status = getline(&string, &length, ff)!=-1){
            if(status != -1){
              for(int j = 0; j < strlen(string)-1; j++){
                fprintf(f,"%c",string[j]);
              }
              fprintf(f,"$\n");
            }
          }
          fclose(ff);
        }
    }
  fclose(f);
  }
}
void cat_E(char **parsed_command_array, char **files, int n, int flag, char *red){
  if(flag == -1){
    if(n == 0){
      do{
        readstring(string_for_empty_commands);
        for(int i = 0; i < strlen(string_for_empty_commands)-1; i++){
          printf("%c",string_for_empty_commands[i]);
        }
        printf("$\n");
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          for(int j = 0; j < strlen(string_for_empty_commands)-1; j++){
            printf("%c",string_for_empty_commands[j]);
          }
          printf("$\n");
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);

        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_E2(parsed_command_array,files,n,i+1,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          for(int j = 0; j < strlen(string)-1; j++){
            printf("%c",string[j]);
          }
          printf("$\n");
        }while(string);
        if(string)
          free(string);
      }
      else{
        FILE *file_parse;
        char *string = malloc(100);
        size_t length = 0;
        ssize_t field;
        file_parse = fopen(files[i], "r");
        while ((field = getline(&string, &length, file_parse)) != -1) {
          for(int j = 0; j < strlen(string)-1; j++){
            printf("%c",string[j]);
          }
          printf("$\n");
        }
        if(string)
          free(string);
      }
    } 
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
        readstring(string_for_empty_commands);
        for(int i = 0; i < strlen(string_for_empty_commands)-1; i++){
          fprintf(f,"%c",string_for_empty_commands[i]);
        }
        fprintf(f,"$\n");
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          for(int j = 0; j < strlen(string_for_empty_commands)-1; j++){
            fprintf(f,"%c",string_for_empty_commands[j]);
          }
          fprintf(f,"$\n");
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_E2(parsed_command_array,files,n,i+1,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          for(int j = 0; j < strlen(string)-1; j++){
            fprintf(f,"%c",string[j]);
          }
          fprintf(f,"$\n");
        }while(string);
        if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              for(int j = 0; j < strlen(string)-1; j++){
                fprintf(f,"%c",string[j]);
              }
              fprintf(f,"$\n");
            }
          }while(status != -1);
          fclose(ff);
        }  
      }  
    }
    fclose(f);
  }
  else if(flag == 3){
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        for(int j = 0; j < strlen(string)-1; j++){
          printf("%c",string[j]);
        }
        printf("$\n");
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            for(int j = 0; j < strlen(string)-1; j++){
              printf("%c",string[j]);
            }
            printf("$\n");
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            for(int j = 0; j < strlen(string)-1; j++){
              printf("%c",string[j]);
            }
            printf("$\n");
          }
          if(string)
            free(string);
        }
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          for(int j = 0; j < strlen(string)-1; j++){
            printf("%c",string[j]);
          }
          printf("$\n");
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      } 
    }
  }
  else if(flag == 4){
    size_t length = 0;
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n > 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          for(int j = 0; j < ind; j++){
            for(int k = 0; k < strlen(lines[j])-1; k++){
              printf("%c",lines[j][k]);
            }
            printf("$\n");
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            for(int j = 0; j < strlen(string)-1; j++){
              printf("%c",string[j]);
            }
            printf("$\n");
          }
          if(string)
            free(string);
        }       
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        for(int j = 0; j < strlen(lines[i])-1; j++){
            printf("%c",lines[i][j]);
          }
        printf("$\n");
      }
      printf("\n");
    }
  }
}
void cat_n2(char **parsed_command_array, char **files, int n, int start, int ind, int flag, char *red){
  int count = ind;
  size_t length = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          count++;
          printf("     %d  %s", count, string);
        }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f, *ff;
    f = fopen(red,"a+");
    char *string = malloc(100);
    size_t length = 0;
    for(int i = start; i < n; i++){
      ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          while(status = getline(&string, &length, ff)!=-1){
            if(status != -1){
              count++;
              fprintf(f,"     %d  %s", count, string);
            }
          }
          fclose(ff);
        }
    }
    fclose(f);
  }
}
void cat_n(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  if(flag == -1){
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          count++;
          printf("     %d  %s", count, string_for_empty_commands);
        }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          count++;
          printf("     %d  %s", count, string_for_empty_commands);
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_n2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          count++;
          printf("     %d  %s\n", count, string);
        }while(string);
        if(string)
          free(string);
      }
      else{
        FILE *file_parse;
        size_t length = 0;
        ssize_t field;
        char *string = malloc(100);
        file_parse = fopen(files[i], "r");
        while ((field = getline(&string, &length, file_parse)) != -1) {
          count++;
          printf("     %d  %s", count, string);
        }
        if(string)
          free(string);
      }
    } 
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          count++;
          fprintf(f,"     %d  %s", count, string_for_empty_commands);
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0){
        do{
          readstring(string_for_empty_commands);
          count++;
          fprintf(f,"     %d  %s", count, string_for_empty_commands);
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_n2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          count++;
          fprintf(f,"     %d  %s\n", count, string);
        }while(string);
        if(string)
          free(string);
      }
      else{
        FILE *ff;
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              count++;
              fprintf(f,"     %d  %s", count, string);
            }
          }while(status != -1);
          fclose(ff);
        }
      }   
    }
    fclose(f);
  }
  else if(flag == 3){
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        count++;
        printf("     %d  %s", count, string);
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            count++;
            printf("     %d  %s", count, string);
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            count++;
            printf("     %d  %s", count, string);
          }
          if(string)
            free(string);
        }   
        
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          count++;
          printf("     %d  %s", count, string);
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }  
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    size_t length = 0;
    int status;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            count++;
            printf("     %d  %s", count, lines[j]);
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            count++;
            printf("     %d  %s", count, string);
          }
          if(string)
            free(string);
          }
          
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        count++;
        printf("     %d  %s", count, lines[i]);
      }
    }
    printf("\n");
  }
}
void cat_s2(char **parsed_command_array, char **files, int n, int start, int flag, char *red){
  size_t length = 0;
  int count = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
      while(getline(&string,&length,ff)!=-1){
        if(checker(string) == 0){
          printf("%s",string);
          count = 0;
        }
        else if(checker(string) == 1 && count == 0){
          printf("%s",string);
          count++;
        }
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f, *ff;
    f = fopen(red,"a+");
    char *string = malloc(100);
    size_t length = 0;
    for(int i = start; i < n; i++){
      ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          while(status = getline(&string, &length, ff)!=-1){
            if(status != -1){
              if(!checker(string)){
                count = 0;
                fprintf(f,"%s", string);
              }
              else if(checker(string) == 1 && count == 0){
                fprintf(f,"%s", string);
                count = 0;
              }
            }
          }
          fclose(ff);
        }
    }
  fclose(f);
  }
  
}
void cat_s(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  if(flag == -1){
    if(n == 0){
      do{
        readstring(string_for_empty_commands);
        if(checker(string_for_empty_commands) == 0){
          printf("%s",string_for_empty_commands);
          count = 0;
        }
        else if(checker(string_for_empty_commands) == 1 && count == 0){
          printf("%s",string_for_empty_commands);
          count++;
        }    
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0 && i == n-1){
          do{
            readstring(string_for_empty_commands);
            if(checker(string_for_empty_commands) == 0){
              printf("%s",string_for_empty_commands);
              count = 0;
            }
            else if(checker(string_for_empty_commands) == 1 && count == 0){
              printf("%s",string_for_empty_commands);
              count++;
            }    
          }while(string_for_empty_commands);
        }
        else if(strcmp(files[i],"-") == 0 && i != n-1){
          int status;
          char *string = malloc(100);
          do{
            status = scanf("%s",string);
            if(status == -1){
              if(feof(stdin)){
                cat_s2(parsed_command_array,files,n,i+1,flag,red);
                exit(EXIT_SUCCESS);
              }
              else{
                printf("An error occured at readline.");
                exit(EXIT_FAILURE);
              }
            }
            if(checker(string) == 0){
              printf("%s\n",string);
              count = 0;
            }
            else if(checker(string) == 1 && count == 0){
              printf("%s\n",string);
              count++;
            }
          }while(string);
          if(string)
            free(string);
        }
        else{
          FILE *file_parse;
          size_t length = 0;
          char *string = malloc(100);
          file_parse = fopen(files[i], "r");
          while ((getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              printf("%s",string);
              count = 0;
            }
            else if(checker(string) == 1 && count == 0){
              printf("%s",string);
              count++;
            }
          }
        }
      }
    }
   else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            fprintf(f,"%s", string_for_empty_commands);
            count = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && count == 0){
            fprintf(f,"%s", string_for_empty_commands);
            count++;
          }
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
            readstring(string_for_empty_commands);
            if(checker(string_for_empty_commands) == 0){
              count = 0;
              fprintf(f," %s", string_for_empty_commands);
            }
            else if(checker(string_for_empty_commands) == 1 && count == 0){
              fprintf(f,"%s", string_for_empty_commands);
              count++;
            }
        }while(string_for_empty_commands);
      }
      
      else if(strcmp(files[i],"-") == 0 && i != n-1){
          int status;
          char *string = malloc(100);
          do{
            status = scanf("%s",string);
            if(status == -1){
              if(feof(stdin)){
                cat_s2(parsed_command_array,files,n,i+1,flag,red);
                exit(EXIT_SUCCESS);
              }
              else{
                printf("An error occured at readline.");
                exit(EXIT_FAILURE);
              }
            }
            if(checker(string) == 0){
              count = 0;
              fprintf(f,"%s\n", string);
            }
            else if(checker(string) == 1 && count == 0){
              fprintf(f,"%s\n", string);
              count++;
            }
            
          }while(string);
          if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(checker(string) == 0){
                count = 0;
                fprintf(f,"%s", string);
              }
              else if(checker(string) == 1 && count == 0){
                fprintf(f,"%s", string);
                count++;
              }
            }
          }while(status != -1);
          fclose(ff);
        }
      }
    } 
    fclose(f);
  }
  else if(flag == 3){ 
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        if(!checker(string)){
          count = 0;
          printf("%s", string);
        }
        else if(checker(string) == 1 && count == 0){
          printf("%s", string);
          count++;
        }
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            if(!checker(string)){
              count = 0;
              printf("%s", string);
            }
            else if(checker(string) == 1 && count == 0){
              printf("%s", string);
              count++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(!checker(string)){
              count = 0;
              printf("%s", string);
            }
            else if(checker(string) == 1 && count == 0){
              printf("%s", string);
              count++;
            }
          }
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          if(!checker(string)){
              count = 0;
              printf("%s", string);
            }
            else if(checker(string) == 1 && count == 0){
              printf("%s", string);
              count++;
            }
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    size_t length = 0;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            if(!checker(lines[j])){
              count=0;
              printf("%s",lines[j]);
            }
            else if(checker(lines[j]) == 1 && count == 0){
              printf("%s", lines[j]);
              count++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(!checker(string)){
              count=0;
              printf("%s", string);
            }
            else{
              printf("%s", string);
              count++;
            }
          }
          if(string)
            free(string);
          }
          
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        if(!checker(lines[i])){
          count = 0;
          printf("%s", lines[i]);
        }
        else if(checker(lines[i]) == 1 && count == 0){
          printf(" %s", lines[i]);
          count++;
        }
      }
    }
  }
}
void cat_bE2(char **parsed_command_array, char **files, int n, int start, int ind, int flag, char *red){
  int count = ind;
  size_t length = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(!checker(string)){
            count++;
            printf("     %d  %s\n", count, string);
          }
          else printf("$\n");
        }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    for(int i = start; i < n; i++){
      FILE *ff;
      ff = fopen(files[i],"r");
      if(ff != NULL){
        int status;
        do{
          char *string = malloc(100);
          size_t length = 0;
          status = getline(&string, &length, ff);
          if(status != -1){
            if(!checker(string)){
              count++;
              fprintf(f, "     %d  ",count);
              for(int j = 0; j < strlen(string)-1; j++){
                fprintf(f,"%c",string[j]);
              }
              fprintf(f,"$\n");
            }
            else fprintf(f,"$\n");
          }
        }while(status != -1);
        fclose(ff);
      }
    }
    fclose(f);
  }
}
void cat_bE(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  if(flag == -1){
    if(n == 0){
      do{
        readstring(string_for_empty_commands);
        if(!checker(string_for_empty_commands)){
          count++;
          printf("     %d  ",count);
          for(int j = 0; j < strlen(string_for_empty_commands)-1; j++){
            printf("%c",string_for_empty_commands[j]);
          }
          printf("$\n");
        }
        else printf("$\n");
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          if(!checker(string_for_empty_commands)){
            count++;
            printf("     %d  ",count);
            for(int j = 0; j < strlen(string_for_empty_commands)-1; j++){
              printf("%c",string_for_empty_commands[j]);
            }
            printf("$\n");
          }
          else printf("$\n");
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_bE2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(!checker(string)){
            count++;
            printf("     %d  ",count);
            for(int j = 0; j < strlen(string)-1; j++){
              printf("%c",string[j]);
            }
            printf("$\n");
          }
          else printf("$\n");
        }while(string);
      }
      else{
        char *string = malloc(100);
        FILE *ff;
        size_t length = 0;
        ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(!checker(string)){
            count++;
            printf("     %d  ",count);
            for(int j = 0; j < strlen(string)-1; j++){
              printf("%c",string[j]);
            }
            printf("$\n");
          }
          else printf("$\n");
        }
      }
    }
  }

  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
        readstring(string_for_empty_commands);
        if(!checker(string_for_empty_commands)){
            count++;
            fprintf(f,"     %d  ",count);
            for(int j = 0; j < strlen(string_for_empty_commands)-1; j++){
              fprintf(f,"%c",string_for_empty_commands[j]);
            }
            fprintf(f,"$\n");
          }
          else fprintf(f,"$\n");
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          if(!checker(string_for_empty_commands)){
            count++;
            fprintf(f,"     %d  ",count);
            for(int j = 0; j < strlen(string_for_empty_commands)-1; j++){
              fprintf(f,"%c",string_for_empty_commands[j]);
            }
            fprintf(f,"$\n");
          }
          else fprintf(f,"$\n");
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_bE2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(!checker(string)){
            count++;
            fprintf(f,"     %d  ",count);
            for(int j = 0; j < strlen(string)-1; j++){
              fprintf(f,"%c",string[j]);
            }
            fprintf(f,"$\n");
          }
          else fprintf(f,"$\n");
        }while(string);
        if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(!checker(string)){
                count++;
                fprintf(f,"     %d  ",count);
                for(int j = 0; j < strlen(string)-1; j++){
                  fprintf(f,"%c",string[j]);
                }
                fprintf(f,"$\n");
              }
              else fprintf(f,"$\n");
             } 
          }while(status != -1);
          fclose(ff);
        }  
      }  
    }
    fclose(f);
  }

 else if(flag == 3){
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        if(!checker(string)){
          count++;
          printf("     %d  ",count);
          for(int j = 0; j < strlen(string)-1; j++){
            printf("%c",string[j]);
          }
          printf("$\n");
        }
        else printf("$\n");
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            if(!checker(string)){
              count++;
              printf("     %d  ",count);
              for(int j = 0; j < strlen(string)-1; j++){
                printf("%c",string[j]);
              }
              printf("$\n");
            }
            else printf("$\n");
              }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(!checker(string)){
              count++;
              printf("     %d  ",count);
              for(int j = 0; j < strlen(string)-1; j++){
                printf("%c",string[j]);
              }
              printf("$\n");
            }
            else printf("$\n");
          }
          if(string)
            free(string);
        }
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          if(!checker(string)){
          count++;
          printf("     %d  ",count);
          for(int j = 0; j < strlen(string)-1; j++){
            printf("%c",string[j]);
          }
          printf("$\n");
        }
        else printf("$\n");
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      } 
    }
  }
  else if(flag == 4){
    size_t length = 0;
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n > 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          for(int j = 0; j < ind; j++){
            if(!checker(lines[i])){
              count++;
              printf("     %d  ",count);
              for(int j = 0; j < strlen(lines[i])-1; j++){
                printf("%c",lines[i][j]);
              }
              printf("$\n");
            }
            else printf("$\n");
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(!checker(lines[i])){
              count++;
              printf("     %d  ",count);
              for(int j = 0; j < strlen(lines[i])-1; j++){
                printf("%c",lines[i][j]);
              }
              printf("$\n");
            }
            else printf("$\n");
          }
          if(string)
            free(string);
        }       
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        if(!checker(lines[i])){
          count++;
          printf("     %d  ",count);
          for(int j = 0; j < strlen(lines[i])-1; j++){
            printf("%c",lines[i][j]);
          }
          printf("$\n");
        }
        else printf("$\n");
      }
      printf("\n");
    }
  }
}
void cat_bs2(char **parsed_command_array, char **files, int n, int start, int ind, int flag, char *red){
  int count = ind;
  int cnt = 0;
  size_t length = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(checker(string) == 0){
             count++;
              printf("     %d  %s",count,string);
              cnt = 0;
            }
          else if(checker(string) == 1 && cnt == 0){
            printf("       %s",string);
            cnt++;
          }
        }
    }
  }
  /**  **/
  else if(flag == 1 || flag == 2){
    FILE *f, *ff;
    f = fopen(red,"a+");
    char *string = malloc(100);
    size_t length = 0;
    for(int i = start; i < n; i++){
      ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          while(status = getline(&string, &length, ff)!=-1){
            if(status != -1){
              if(checker(string) == 0){
                  count++;
                  fprintf(f,"     %d  %s",count,string);
                  cnt = 0;
                }
              else if(checker(string) == 1 && cnt == 0){
                fprintf(f,"       %s",string);
                cnt++;
              } 
            }
          }
          fclose(ff);
        }
    }
  fclose(f);
  }
}
void cat_bs(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  int cnt = 0;
  if(flag == -1){
    if(n == 0){
      do{
        readstring(string_for_empty_commands);
        if(checker(string_for_empty_commands) == 0){
          count++;
          printf("     %d  %s",count,string_for_empty_commands);
          cnt = 0;
        }
        else if(checker(string_for_empty_commands) == 1 && cnt == 0){
          printf("       %s",string_for_empty_commands);
          cnt++;
        }    
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            printf("     %d  %s",count,string_for_empty_commands);
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            printf("       %s",string_for_empty_commands);
            cnt++;
          }    
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          size_t length = 0;
          if(status == -1){
            if(feof(stdin)){
              cat_bs2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(checker(string) == 0){
              count++;
              printf("     %d  %s\n",count,string);
              cnt = 0;
            }
          else if(checker(string) == 1 && cnt == 0){
            printf("       %s\n",string);
            cnt++;
          }
        }while(string);
      }
      else{
        char *string = malloc(100);
        FILE *ff;
        size_t length = 0;
        ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(checker(string) == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt = 0;
            }
          else if(checker(string) == 1 && cnt == 0){
            printf("       %s",string);
            cnt++;
          }
        }
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
              count++;
              fprintf(f,"     %d  %s",count,string_for_empty_commands);
              cnt = 0;
            }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            fprintf(f,"       %s",string_for_empty_commands);
            cnt++;
          }
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
            readstring(string_for_empty_commands);
            if(checker(string_for_empty_commands) == 0){
              count++;
              fprintf(f,"     %d  %s",count,string_for_empty_commands);
              cnt = 0;
            }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            fprintf(f,"       %s",string_for_empty_commands);
            cnt++;
          }
        }while(string_for_empty_commands);
      }
      
      else if(strcmp(files[i],"-") == 0 && i != n-1){
          int status;
          char *string = malloc(100);
          do{
            status = scanf("%s",string);
            if(status == -1){
              if(feof(stdin)){
                cat_bs2(parsed_command_array,files,n,i+1,count,flag,red);
                exit(EXIT_SUCCESS);
              }
              else{
                printf("An error occured at readline.");
                exit(EXIT_FAILURE);
              }
            }
            if(checker(string) == 0){
              count++;
              fprintf(f,"     %d  %s\n",count,string);
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              fprintf(f,"       %s\n",string);
              cnt++;
            }   
          }while(string);
          if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(checker(string) == 0){
                count++;
                fprintf(f,"     %d  %s",count,string);
                cnt = 0;
              }
              else if(checker(string) == 1 && cnt == 0){
                fprintf(f,"       %s",string);
                cnt++;
              }
            }
          }while(status != -1);
          fclose(ff);
        }
      }
    } 
    fclose(f);
  }
  else if(flag == 3){ 
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        if(checker(string) == 0){
          count++;
          printf("     %d  %s",count,string);
          cnt = 0;
        }
        else if(checker(string) == 1 && cnt == 0){
          printf("       %s",string);
          cnt++;
        }
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            if(checker(string) == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              printf("       %s",string);
              cnt++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              printf("       %s",string);
              cnt++;
            }
          }
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          if(checker(string) == 0){
            count++;
            printf("     %d  %s",count,string);
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            printf("       %s",string);
            cnt++;
          }
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    size_t length = 0;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            if(checker(lines[j]) == 0){
              count++;
              printf("     %d  %s",count,lines[j]);
              cnt = 0;
            }
            else if(checker(lines[j]) == 1 && cnt == 0){
              printf("       %s",lines[j]);
              cnt++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              printf("       %s",string);
              cnt++;
            }
          }
          if(string)
            free(string);
          }
          
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        if(checker(lines[i]) == 0){
          count++;
          printf("     %d  %s",count,lines[i]);
          cnt = 0;
        }
        else if(checker(lines[i]) == 1 && cnt == 0){
          printf("       %s",lines[i]);
          cnt++;
        }
      }
    }
    printf("\n");
  }
}
void cat_En2(char **parsed_command_array, char **files, int n, int start, int ind, int flag, char *red){
  int count = ind;
  size_t length = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          printf("     %d  ", count);
          for(int i = 0; i < strlen(string)-1; i++)
            printf("%c",string[i]);
          printf("$\n");
          count++;
        }
    }
  }
  
  else if(flag == 1 || flag == 2){
    FILE *f, *ff;
    f = fopen(red,"a+");
    char *string = malloc(100);
    size_t length = 0;
    for(int i = start; i < n; i++){
      ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          while(status = getline(&string, &length, ff)!=-1){
            if(status != -1){
              fprintf(f,"     %d  ", count);
              for(int i = 0; i < strlen(string)-1; i++)
                fprintf(f,"%c",string[i]);
              fprintf(f,"$\n");
              count++;
            }
          }
          fclose(ff);
        }
    }
    fclose(f);
  } 
}
void cat_En(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 1;
  if(flag == -1){
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          printf("     %d  ", count);
          for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
            printf("%c",string_for_empty_commands[i]);
          printf("$\n");
          count++;
        }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          printf("     %d  ", count);
          for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
            printf("%c",string_for_empty_commands[i]);
          printf("$\n");
          count++;
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_En2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          printf("     %d  ", count);
          for(int i = 0; i < strlen(string)-1; i++)
            printf("%c",string[i]);
          printf("$\n");
          count++;
        }while(string);
      }
      else{
        char *string = malloc(100);
        FILE *ff;
        size_t length = 0;
        ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          printf("     %d  ", count);
          for(int i = 0; i < strlen(string)-1; i++)
            printf("%c",string[i]);
          printf("$\n");
          count++;
        }
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          fprintf(f,"     %d  ", count);
          for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
            fprintf(f,"%c",string_for_empty_commands[i]);
          fprintf(f,"$\n");
          count++;
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0){
        do{
          readstring(string_for_empty_commands);
          fprintf(f,"     %d  ", count);
          for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
            fprintf(f,"%c",string_for_empty_commands[i]);
          fprintf(f,"$\n");
          count++;
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_En2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          fprintf(f,"     %d  ", count);
          for(int i = 0; i < strlen(string)-1; i++)
            fprintf(f,"%c",string[i]);
          fprintf(f,"$\n");
          count++;
        }while(string);
        if(string)
          free(string);
      }
      else{
        FILE *ff;
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              fprintf(f,"     %d  ", count);
              for(int i = 0; i < strlen(string)-1; i++)
                fprintf(f,"%c",string[i]);
              fprintf(f,"$\n");
              count++;
            }
          }while(status != -1);
          fclose(ff);
        }
      }   
    }
    fclose(f);
  }
  else if(flag == 3){
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        printf("     %d  ", count);
        for(int i = 0; i < strlen(string)-1; i++)
          printf("%c",string[i]);
        printf("$\n");
        count++;
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            printf("     %d  ", count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            count++;
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            printf("     %d  ", count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            count++;
          }
          if(string)
            free(string);
        }   
        
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          printf("     %d  ", count);
          for(int i = 0; i < strlen(string)-1; i++)
            printf("%c",string[i]);
          printf("$\n");
          count++;
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }  
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    size_t length = 0;
    int status;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            printf("     %d  ", count);
            for(int k = 0; k < strlen(lines[j])-1; k++)
              printf("%c",lines[j][k]);
            printf("$\n");
            count++;
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            printf("     %d  ", count);
            for(int j = 0; j < strlen(string)-1; j++)
              printf("%c",string[j]);
            printf("$\n");
            count++;
          }
          if(string)
            free(string);
          }
          
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        printf("     %d  ", count);
        for(int j = 0; j < strlen(lines[i])-1; i++)
          printf("%c",lines[i][j]);
        printf("$\n");
        count++;
      }
    }
    printf("\n");
  }
}
void cat_Es2(char **parsed_command_array, char **files, int n, int start, int flag, char*red){
  char *string = malloc(100);
  int count = 0;
  FILE *file_parse;
  size_t length = 0;
  ssize_t field;
  if(flag == -1){
     for(int i = start; i < n; i++){
      file_parse = fopen(files[i], "r");
      while ((field = getline(&string, &length, file_parse)) != -1) {
        if(checker(string) == 0){
          for(int i = 0; i < strlen(string)-1; i++)
            printf("%c",string[i]);
          printf("$\n");
          count = 0;
        }
        else if(checker(string) == 1 && count == 0){
          printf("$\n");
          count++;
        }
      }
    } 
  }

  else if(flag == 1 || flag == 2){
    FILE *f, *ff;
    f = fopen(red,"a+");
    char *string = malloc(100);
    size_t length = 0;
    for(int i = start; i < n; i++){
      ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          while(status = getline(&string, &length, ff)!=-1){
            if(status != -1){
              if(checker(string) == 0){
                for(int i = 0; i < strlen(string)-1; i++)
                  fprintf(f,"%c",string[i]);
                fprintf(f,"$\n");
                count = 0;
              }
              else if(checker(string) == 1 && count == 0){
                fprintf(f,"$\n");
                count++;
              }
            }
          }
          fclose(ff);
        }
    }
  fclose(f);
  }
}
void cat_Es(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  if(flag == -1){
    if(n == 0){
      do{
        readstring(string_for_empty_commands);
        if(checker(string_for_empty_commands) == 0){
          for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
            printf("%c",string_for_empty_commands[i]);
          printf("$\n");
          count = 0;
        }
        else if(checker(string_for_empty_commands) == 1 && count == 0){
          printf("$\n");
          count++;
        }   
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        size_t length = 0;
        do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
              printf("%c",string_for_empty_commands[i]);
            printf("$\n");
            count = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && count == 0){
            printf("$\n");
            count++;
          }    
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        size_t length = 0;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_Es2(parsed_command_array,files,n,i+1,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(checker(string) == 0){
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            count = 0;
          }
          else if(checker(string) == 1 && count == 0){
            printf("$\n");
            count++;
          }
        }while(string);
      }
      else{
        char *string = malloc(100);
        FILE *ff;
        size_t length = 0;
        ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(checker(string) == 0){
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            count = 0;
          }
          else if(checker(string) == 1 && count == 0){
            printf("$\n");
            count++;
          }
        }
      }
    }
  }
 
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
              fprintf(f,"%c",string_for_empty_commands[i]);
            fprintf(f,"$\n");
            count = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && count == 0){
            fprintf(f,"$\n");
            count++;
          }
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
            readstring(string_for_empty_commands);
            if(checker(string_for_empty_commands) == 0){
              for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
                fprintf(f,"%c",string_for_empty_commands[i]);
              fprintf(f,"$\n");
              count = 0;
            }
            else if(checker(string_for_empty_commands) == 1 && count == 0){
              fprintf(f,"$\n");
              count++;
            }
        }while(string_for_empty_commands);
      }
      
      else if(strcmp(files[i],"-") == 0 && i != n-1){
          int status;
          char *string = malloc(100);
          do{
            status = scanf("%s",string);
            if(status == -1){
              if(feof(stdin)){
                cat_Es2(parsed_command_array,files,n,i+1,flag,red);
                exit(EXIT_SUCCESS);
              }
              else{
                printf("An error occured at readline.");
                exit(EXIT_FAILURE);
              }
            }
            if(checker(string) == 0){
              for(int i = 0; i < strlen(string)-1; i++)
                fprintf(f,"%c",string[i]);
              fprintf(f,"$\n");
              count = 0;
            }
            else if(checker(string) == 1 && count == 0){
              fprintf(f,"$\n");
              count++;
            }
            
          }while(string);
          if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(checker(string) == 0){
                for(int i = 0; i < strlen(string)-1; i++)
                  fprintf(f,"%c",string[i]);
                fprintf(f,"$\n");
                count = 0;
              }
              else if(checker(string) == 1 && count == 0){
                fprintf(f,"$\n");
                count++;
              }
            }
          }while(status != -1);
          fclose(ff);
        }
      }
    } 
    fclose(f);
  }
  else if(flag == 3){ 
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        if(checker(string) == 0){
          for(int i = 0; i < strlen(string)-1; i++)
            printf("%c",string[i]);
          printf("$\n");
          count = 0;
        }
        else if(checker(string) == 1 && count == 0){
          printf("$\n");
          count++;
        }
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            if(checker(string) == 0){
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
              printf("$\n");
              count = 0;
            }
            else if(checker(string) == 1 && count == 0){
              printf("$\n");
              count++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
                printf("$\n");
              count = 0;
            }
            else if(checker(string) == 1 && count == 0){
              printf("$\n");
              count++;
            }
          }
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          if(checker(string) == 0){
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            count = 0;
          }
          else if(checker(string) == 1 && count == 0){
            fprintf(f,"$\n");
            count++;
          }
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    size_t length = 0;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            if(checker(lines[j]) == 0){
              for(int k = 0; k < strlen(lines[j])-1; k++)
                printf("%c",lines[j][k]);
              printf("$\n");
              count = 0;
            }
            else if(checker(lines[j]) == 1 && count == 0){
              printf("$\n");
              count++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
           if(checker(string) == 0){
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
              printf("$\n");
              count = 0;
            }
            else if(checker(string) == 1 && count == 0){
              printf("$\n");
              count++;
            }
          }
          if(string)
            free(string);
          }
          
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        if(checker(lines[i]) == 0){
          for(int j = 0; j < strlen(lines[i])-1; j++)
            printf("%c",lines[i][j]);
          printf("$\n");
          count = 0;
        }
        else if(checker(lines[i]) == 1 && count == 0){
          printf("$\n");
          count++;
        }
      }
    }
  }
}
void cat_ns2(char **parsed_command_array, char **files, int n, int start, int ind, int flag, char *red){
  int count = ind;
  int cnt = 0;
  size_t length = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(checker(string) == 0){
            count++;
            printf("     %d  %s",count,string);
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  %s",count,string);
            cnt++;
          }
        }
    }
  }
  else if(flag == 1 || flag == 2){
      FILE *f;
      f = fopen(red,"a+"); 
      for(int i = start; i < n; i++){
        FILE *ff;
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(checker(string) == 0){
                count++;
                fprintf(f,"     %d  %s",count,string);
                cnt = 0;
              }
              else if(checker(string) == 1 && cnt == 0){
                count++;
                fprintf(f,"     %d  %s",count,string);
                cnt++;
              }
            }
          }while(status != -1);
          fclose(ff);
        }
      }
      fclose(f);
    }   
}
void cat_ns(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  int cnt = 0;
  if(flag == -1){
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            printf("     %d  %s",count,string_for_empty_commands);
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            count++;
            printf("     %d  %s",count,string_for_empty_commands);
            cnt++;
          }
        }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        size_t length = 0;
        do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            printf("     %d  %s",count,string_for_empty_commands);
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            count++;
            printf("     %d  %s",count,string_for_empty_commands);
            cnt++;
          }
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        size_t length = 0;
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_ns2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(checker(string) == 0){
            count++;
            printf("     %d  %s\n",count,string);
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  %s\n",count,string);
            cnt++;
          }
        }while(string);
      }
      else{
        char *string = malloc(100);
        FILE *ff;
        size_t length = 0;
        ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(checker(string) == 0){
            count++;
            printf("     %d  %s",count,string);
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  %s",count,string);
            cnt++;
          }
        }
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            fprintf(f,"     %d  %s",count,string_for_empty_commands);
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            count++;
            fprintf(f,"     %d  %s",count,string_for_empty_commands);
            cnt++;
          }
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
            readstring(string_for_empty_commands);
            if(checker(string_for_empty_commands) == 0){
              count++;
              fprintf(f,"     %d  %s",count,string_for_empty_commands);
              cnt = 0;
            }
            else if(checker(string_for_empty_commands) == 1 && cnt == 0){
              count++;
              fprintf(f,"     %d  %s",count,string_for_empty_commands);
              cnt++;
            }
        }while(string_for_empty_commands);
      }
      
      else if(strcmp(files[i],"-") == 0 && i != n-1){
          int status;
          char *string = malloc(100);
          do{
            status = scanf("%s",string);
            if(status == -1){
              if(feof(stdin)){
                cat_ns2(parsed_command_array,files,n,i+1,count,flag,red);
                exit(EXIT_SUCCESS);
              }
              else{
                printf("An error occured at readline.");
                exit(EXIT_FAILURE);
              }
            }
            if(checker(string) == 0){
              count++;
              fprintf(f,"     %d  %s\n",count,string);
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              fprintf(f,"     %d  %s\n",count,string);
              cnt++;
            }
          }while(string);
          if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(checker(string) == 0){
                count++;
                fprintf(f,"     %d  %s",count,string);
                cnt = 0;
              }
              else if(checker(string) == 1 && cnt == 0){
                count++;
                fprintf(f,"     %d  %s",count,string);
                cnt++;
              }
            }
          }while(status != -1);
          fclose(ff);
        }
      }
    } 
    fclose(f);
  }
  else if(flag == 3){ 
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        if(checker(string) == 0){
          count++;
          printf("     %d  %s",count,string);
          cnt = 0;
        }
        else if(checker(string) == 1 && cnt == 0){
          count++;
          printf("     %d  %s",count,string);
          cnt++;
        }
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            if(checker(string) == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt++;
            }
          }
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          if(checker(string) == 0){
            count++;
            printf("     %d  %s",count,string);
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  %s",count,string);
            cnt++;
          }
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    size_t length = 0;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            if(checker(lines[j]) == 0){
              count++;
              printf("     %d  %s",count,lines[j]);
              cnt = 0;
            }
            else if(checker(lines[j]) == 1 && cnt == 0){
              count++;
              printf("     %d  %s",count,lines[j]);
              cnt++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              count++;
              printf("     %d  %s",count, string);
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              printf("     %d  %s",count,string);
              cnt++;
            }
          }
          if(string)
            free(string);
          }
          
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        if(checker(lines[i]) == 0){
          count++;
          printf("     %d  %s",count,lines[i]);
          cnt = 0;
        }
        else if(checker(lines[i]) == 1 && cnt == 0){
          count++;
          printf("     %d  %s",count,lines[i]);
          cnt++;
        }
      }
    }
    printf("\n");
  }
}
void cat_Ebs2(char **parsed_command_array, char **files, int n, int start, int ind, int flag, char *red){
  int count = ind;
  int cnt = 0;
  size_t length = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("$\n");
            cnt++;
          }    
        }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    for(int i = start; i < n; i++){
      FILE *ff;
      ff = fopen(files[i],"r");
      if(ff != NULL){
        int status;
        do{
          char *string = malloc(100);
          size_t length = 0;
          status = getline(&string, &length, ff);
          if(status != -1){
            if(checker(string) == 0){
              count++;
              fprintf(f,"     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                fprintf(f,"%c",string[i]);
              fprintf(f,"$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              fprintf(f,"$\n");
              cnt++;
            }    
          }
        }while(status != -1);
        fclose(ff);
      }
    }
    fclose(f);
  }   
}
void cat_Ebs(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  int cnt = 0;
  if(flag == -1){
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
              printf("%c",string_for_empty_commands[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            printf("$\n");
            cnt++;
          }    
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        size_t length = 0;
        do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
              printf("%c",string_for_empty_commands[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            printf("$\n");
            cnt++;
          }    
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        size_t length = 0;
        do{
          status = getline(&string,&length,stdin);
          if(status == -1){
            if(feof(stdin)){
              cat_Ebs2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            printf("$\n");
            cnt++;
          }    
        }while(string);
      }
      else{
        char *string = malloc(100);
        FILE *ff;
        size_t length = 0;
        ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            printf("$\n");
            cnt++;
          }    
        }
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            fprintf(f,"     %d  ",count);
            for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
              fprintf(f,"%c",string_for_empty_commands[i]);
            fprintf(f,"$\n");
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            fprintf(f,"$\n");
            cnt++;
          }
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
            readstring(string_for_empty_commands);
            if(checker(string_for_empty_commands) == 0){
              count++;
              fprintf(f,"     %d  ",count);
              for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
                fprintf(f,"%c",string_for_empty_commands[i]);
              fprintf(f,"$\n");
              cnt = 0;
            }
            else if(checker(string_for_empty_commands) == 1 && cnt == 0){
              fprintf(f,"$\n");
              cnt++;
            }
        }while(string_for_empty_commands);
      }
      
      else if(strcmp(files[i],"-") == 0 && i != n-1){
          int status;
          char *string = malloc(100);
          do{
            status = scanf("%s",string);
            if(status == -1){
              if(feof(stdin)){
                cat_Ebs2(parsed_command_array,files,n,i+1,count,flag,red);
                exit(EXIT_SUCCESS);
              }
              else{
                printf("An error occured at readline.");
                exit(EXIT_FAILURE);
              }
            }
            if(checker(string) == 0){
              count++;
              fprintf(f,"     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                fprintf(f,"%c",string[i]);
              fprintf(f,"$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              fprintf(f,"$\n");
              cnt++;
            }
          }while(string);
          if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(checker(string) == 0){
                count++;
                fprintf(f,"     %d  ",count);
                for(int i = 0; i < strlen(string)-1; i++)
                  fprintf(f,"%c",string[i]);
                fprintf(f,"$\n");
                cnt = 0;
              }
              else if(checker(string) == 1 && cnt == 0){
                fprintf(f,"$\n");
                cnt++;
              }
            }
          }while(status != -1);
          fclose(ff);
        }
      }
    } 
    fclose(f);
  }
  else if(flag == 3){ 
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        if(checker(string) == 0){
          count++;
          printf("     %d  ",count);
          for(int i = 0; i < strlen(string)-1; i++)
            printf("%c",string[i]);
          printf("$\n");
          cnt = 0;
        }
        else if(checker(string) == 1 && cnt == 0){
          printf("$\n");
          cnt++;
        }
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            if(checker(string) == 0){
              count++;
              printf("     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
              printf("$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              printf("$\n");
              cnt++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              count++;
              printf("     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
              printf("$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              printf("$\n");
              cnt++;
            }
          }
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            printf("$\n");
            cnt++;
          }
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    size_t length = 0;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            if(checker(lines[j]) == 0){
              count++;
              printf("     %d  ",count);
              for(int k = 0; k < strlen(lines[j])-1; k++)
                printf("%c",lines[j][k]);
              printf("$\n");
              cnt = 0;
            }
            else if(checker(lines[j]) == 1 && cnt == 0){
              printf("$\n");
              cnt++;
            }
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              count++;
              printf("     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
              printf("$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              printf("$\n");
              cnt++;
            }
          }
          if(string)
            free(string);
          }
          
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        if(checker(lines[i]) == 0){
          count++;
          printf("     %d  ",count);
          for(int k = 0; k < strlen(lines[i])-1; k++)
            printf("%c",lines[i][k]);
          printf("$\n");
          cnt = 0;
        }
        else if(checker(lines[i]) == 1 && cnt == 0){
          printf("$\n");
          cnt++;
        }
      }
    }
  }
}
void cat_Ens2(char **parsed_command_array, char **files, int n, int start, int ind, int flag, char *red){
  int count = ind;
  int cnt = 0;
  size_t length = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      size_t length = 0;
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  $\n",count);
            cnt++;
          }    
        }
        if(string)
          free(string);
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    for(int i = start; i < n; i++){
      FILE *ff;
      ff = fopen(files[i],"r");
      if(ff != NULL){
        int status;
        do{
          char *string = malloc(100);
          size_t length = 0;
          status = getline(&string, &length, ff);
          if(status != -1){
            if(checker(string) == 0){
              count++;
              fprintf(f,"     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                fprintf(f,"%c",string[i]);
              fprintf(f,"$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              fprintf(f,"     %d  $\n",count);
              cnt++;
            }    
          }
          if(string)
            free(string);
        }while(status != -1);
        fclose(ff);
      }
    }
    fclose(f);
  }
}
void cat_Ens(char **parsed_command_array, char **files, int n, int flag, char *red){
  int count = 0;
  int cnt = 0;
  if(flag == -1){
    if(n == 0){
        do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
              printf("%c",string_for_empty_commands[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            count++;
            printf("     %d  $\n",count);
            cnt++;
          }    
       }while(string_for_empty_commands);
      }
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        FILE *file_parse;
        size_t length = 0;
        ssize_t field;
        do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
              printf("%c",string_for_empty_commands[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            count++;
            printf("     %d  $\n",count);
            cnt++;
          }    
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        FILE *file_parse;
        size_t length = 0;
        ssize_t field;
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              cat_Ens2(parsed_command_array,files,n,i+1,count,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  $\n",count);
            cnt++;
          }    
        }while(string);
        if(string) free(string);
      }
      else{
        FILE *file_parse;
        size_t length = 0;
        ssize_t field;
        char *string = malloc(100);
        file_parse = fopen(files[i], "r");
        while ((field = getline(&string, &length, file_parse)) != -1) {
          if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  $\n",count);
            cnt++;
          }    
        }
        if(string)
          free(string);
      }
    }
  } 
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    if(n == 0){
      do{
          readstring(string_for_empty_commands);
          if(checker(string_for_empty_commands) == 0){
            count++;
            fprintf(f,"     %d  ",count);
            for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
              fprintf(f,"%c",string_for_empty_commands[i]);
            fprintf(f,"$\n");
            cnt = 0;
          }
          else if(checker(string_for_empty_commands) == 1 && cnt == 0){
            count++;
            fprintf(f,"     %d  $\n",count);
            cnt++;
          }    
      }while(string_for_empty_commands);
    }
    for(int i = 0; i < n; i++){
      FILE *ff;
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
            readstring(string_for_empty_commands);
            if(checker(string_for_empty_commands) == 0){
              count++;
              fprintf(f,"     %d  ",count);
              for(int i = 0; i < strlen(string_for_empty_commands)-1; i++)
                fprintf(f,"%c",string_for_empty_commands[i]);
              fprintf(f,"$\n");
              cnt = 0;
            }
            else if(checker(string_for_empty_commands) == 1 && cnt == 0){
              count++;
              fprintf(f,"     %d  $\n",count);
              cnt++;
            }    
        }while(string_for_empty_commands);
      }
      
      else if(strcmp(files[i],"-") == 0 && i != n-1){
          int status;
          char *string = malloc(100);
          do{
            status = scanf("%s",string);
            if(status == -1){
              if(feof(stdin)){
                cat_Ens2(parsed_command_array,files,n,i+1,count,flag,red);
                exit(EXIT_SUCCESS);
              }
              else{
                printf("An error occured at readline.");
                exit(EXIT_FAILURE);
              }
            }
            if(checker(string) == 0){
              count++;
              fprintf(f,"     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                fprintf(f,"%c",string[i]);
              fprintf(f,"$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              fprintf(f,"     %d  $\n",count);
              cnt++;
            }    
          }while(string);
          if(string)
          free(string);
      }
      else{
        ff = fopen(files[i],"r");
        if(ff != NULL){
          int status;
          do{
            char *string = malloc(100);
            size_t length = 0;
            status = getline(&string, &length, ff);
            if(status != -1){
              if(checker(string) == 0){
                count++;
                fprintf(f,"     %d  ",count);
                for(int i = 0; i < strlen(string)-1; i++)
                  fprintf(f,"%c",string[i]);
                fprintf(f,"$\n");
                cnt = 0;
              }
              else if(checker(string) == 1 && cnt == 0){
                count++;
                fprintf(f,"     %d  $\n",count);
                cnt++;
              }    
            }
          }while(status != -1);
          fclose(ff);
        }
      }
    } 
    fclose(f);
  }
  else if(flag == 3){ 
    FILE *f;
    f = fopen(red,"r");
    if(n == 0){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string,&length,f)!=-1){
        if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  $\n",count);
            cnt++;
          }    
      }
    }
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          while(getline(&string, &length, f)!=-1){
            if(checker(string) == 0){
              count++;
              printf("     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
              printf("$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              printf("     %d  $\n",count);
              cnt++;
            }    
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              count++;
              printf("     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
              printf("$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              printf("     %d  $\n",count);
              cnt++;
            }    
          }
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(f != NULL){
        char *string = malloc(100);
        size_t length = 0;
        while(getline(&string, &length,f) != -1){
          if(checker(string) == 0){
            count++;
            printf("     %d  ",count);
            for(int i = 0; i < strlen(string)-1; i++)
              printf("%c",string[i]);
            printf("$\n");
            cnt = 0;
          }
          else if(checker(string) == 1 && cnt == 0){
            count++;
            printf("     %d  $\n",count);
            cnt++;
          }    
        }
        fclose(f);
      }
      else{
        printf("bash: %s: No such file or directory\n",red);
      }
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    size_t length = 0;
    do{
      printf("     > ");
      char *string = malloc(100);
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    FILE *f;
    printf("\n");
    if(n >= 1){
      for(int i = 0; i < n; i++){
        if(strcmp(files[i],"-") == 0){

          for(int j = 0; j < ind; j++){
            if(checker(lines[j]) == 0){
              count++;
              printf("     %d  ",count);
              for(int k = 0; k < strlen(lines[j])-1; k++)
                printf("%c",lines[j][k]);
              printf("$\n");
              cnt = 0;
            }
            else if(checker(lines[j]) == 1 && cnt == 0){
              count++;
              printf("     %d  $\n",count);
              cnt++;
            }    
          }
        }
        else{
          FILE *file_parse = fopen(files[i], "r");
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          while ((field = getline(&string, &length, file_parse)) != -1) {
            if(checker(string) == 0){
              count++;
              printf("     %d  ",count);
              for(int i = 0; i < strlen(string)-1; i++)
                printf("%c",string[i]);
              printf("$\n");
              cnt = 0;
            }
            else if(checker(string) == 1 && cnt == 0){
              count++;
              printf("     %d  $\n",count);
              cnt++;
          }    
          }
          if(string)
            free(string);
          }
          
      }  
    }
    if(n == 0){
      for(int i = 0; i < ind; i++){
        if(checker(lines[i]) == 0){
          count++;
          printf("     %d  ",count);
          for(int j = 0; j < strlen(lines[i])-1; j++)
            printf("%c",lines[i][j]);
          printf("$\n");
          cnt = 0;
        }
        else if(checker(lines[i]) == 1 && cnt == 0){
          count++;
          printf("     %d  $\n",count);
          cnt++;
        }    
      }
    }
  }
}
void simple_cat2(char **parsed_command_array, char **files, int n, int start, int flag, char *red){
  size_t length = 0;
  FILE *ff;
  if(flag == -1){
    for(int i = start; i < n; i++){
      char*string = malloc(100);
      ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          printf("%s",string);
        }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    for(int i = start; i < n; i++){
      FILE *ff;
      ff = fopen(files[i],"r");
      if(ff != NULL){
        int status;
        do{
          char *string = malloc(100);
          size_t length = 0;
          status = getline(&string, &length, ff);
          if(status != -1) fprintf(f,"%s",string);
        }while(status != -1);
        fclose(ff);
      }
    }
    fclose(f);
  }
}
void simple_cat(char **parsed_command_array, char **files, int n, int flag, char *red){ 
  
  if(flag == -1){
    for(int i = 0; i < n; i++){
      if(strcmp(files[i],"-") == 0 && i == n-1){
        do{
          readstring(string_for_empty_commands);
          printf("%s",string_for_empty_commands);
        }while(string_for_empty_commands);
      }
      else if(strcmp(files[i],"-") == 0 && i != n-1){
        int status;
        char *string = malloc(100);
        do{
          status = scanf("%s",string);
          if(status == -1){
            if(feof(stdin)){
              simple_cat2(parsed_command_array,files,n,i+1,flag,red);
              exit(EXIT_SUCCESS);
            }
            else{
              printf("An error occured at readline.");
              exit(EXIT_FAILURE);
            }
          }
          printf("%s\n",string);
        }while(string);
      }
      else{
        char *string = malloc(100);
        FILE *ff;
        size_t length = 0;
        ff = fopen(files[i],"r");
        while(getline(&string,&length,ff)!=-1){
          printf("%s",string);
        }
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *f;
    f = fopen(red,"a+"); 
    for(int i = 0; i < n; i++){
      FILE *ff;
      ff = fopen(files[i],"r");
      if(ff != NULL){
        int status;
        do{
          char *string = malloc(100);
          size_t length = 0;
          status = getline(&string, &length, ff);
          if(status != -1) fprintf(f,"%s",string);
        }while(status != -1);
        fclose(ff);
      }
    }
    fclose(f);
  }

 else if(flag == 3){
    FILE *f;
    f = fopen(red,"r");
    if(f != NULL){
      char *string = malloc(100);
      size_t length = 0;
      while(getline(&string, &length,f) != -1){
        printf("%s",string);
      }
      fclose(f);
    }
    else{
      printf("bash: %s: No such file or directory\n",red);
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0;
    int status;
    do{
      printf("     > ");
      char *string = malloc(100);
      size_t length = 0;
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
    }while(status != -1);
    printf("\n");
    for(int i = 0; i < ind; i++)
        printf("%s",lines[i]);
  }
}
void cat_help(){
  printf("Usage: cat [OPTION]... [FILE]...\nConcatenate FILE(s) to standard output.\n\nWith no FILE, or when FILE is -, read standard input.\n-b, --number-nonblank    number nonempty output lines, overrides -n\n-E, --show-ends          display $ at end of each line\n-n, --number             number all output lines\n-s, --squeeze-blank      suppress repeated empty output lines\n\nExamples:\ncat f - g  Output fs contents, then standard input, then gs contents.\ncat        Copy standard input to standard output.\n");
}
void parse_cat_args(char **parsed_command_array){
    int index = 1, pindex = 0, findex = 0, nindex = 0;
    int buffer_size = 64;
    char *next_redirect = malloc(100);
    char **parameters = malloc(buffer_size * sizeof(char *));
    char **files = malloc(buffer_size * sizeof(char *));
    char **nonsense = malloc(buffer_size * sizeof(char *));
    int redflag = -1;
    while(parsed_command_array[index] != NULL){
      if(cat_param_checker(parsed_command_array[index]) != -1){
        parameters[pindex] = parsed_command_array[index];
        index++;
        pindex++;
      }
      else if(file_checker(parsed_command_array[index]) == 1){
        files[findex] = parsed_command_array[index];
        findex++;
        index++;
      }
      else if(strcmp(parsed_command_array[index],">") == 0){
        if(parsed_command_array[index+1] != NULL){
          strcpy(next_redirect,parsed_command_array[index+1]);
          index+=2;
          redflag = 1;
        }
        else printf("bash: syntax error near unexpected token `newline'");
      }
      else if(strcmp(parsed_command_array[index],">>") == 0){
        if(parsed_command_array[index+1] != NULL){
          strcpy(next_redirect,parsed_command_array[index+1]);
          index+=2;
          redflag = 2;
        }
        else printf("bash: syntax error near unexpected token `newline'");
      }
      else if(strcmp(parsed_command_array[index],"<") == 0){
        if(parsed_command_array[index+1] != NULL){
          strcpy(next_redirect,parsed_command_array[index+1]);
          index+=2;
          redflag = 3;
        }
        else printf("bash: syntax error near unexpected token `newline'");
      }
      else if(strcmp(parsed_command_array[index],"<<") == 0){
        if(parsed_command_array[index+1] != NULL){
          strcpy(next_redirect,parsed_command_array[index+1]);
          index+=2;
          redflag = 4;
        }
        else printf("bash: syntax error near unexpected token `newline'");
      }
      else{
          nonsense[nindex] = parsed_command_array[index];
          nindex++;
          index++;
      }
    }
    
    sort(parameters,pindex);
    char param_merge[4] = "";
    for(int i = 0; i < pindex; i++){
      char c;
      c = parameters[i][1];
      strncat(param_merge,&c,1);
    }

    if((pindex == 0 && findex != 0 && nindex == 0) ||(pindex == 0 && findex == 0 && nindex == 0 && redflag == 3) || (pindex == 0 && findex == 0 && nindex == 0 && redflag == 4)){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      simple_cat(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(pindex == 0 && findex == 0 && nindex == 0 && redflag != 3){
      if(redflag == -1){
        char *string = malloc(100);
        size_t length = 0;
        getline(&string,&length,stdin);
         do{
            printf("%s",string);
            free(string);
            string = malloc(100);
        }while(getline(&string,&length,stdin)!=-1);
      }
      else if(redflag == 1){
        FILE *f;
        size_t length = 0;
        f = fopen(next_redirect,"w"); 
        char *string = malloc(100);
        while(getline(&string,&length,stdin)!=-1)
            fprintf(f,"%s",string);
        
        fclose(f);
      }
      else if(redflag == 2){
        FILE *f;
        size_t length = 0;
        f = fopen(next_redirect,"a+"); 
        char *string = malloc(100);
        while(getline(&string,&length,stdin)!=-1)
            fprintf(f,"%s",string);    
        fclose(f);
      }
      else if(redflag == 4){
        char **lines = malloc(64*sizeof(char*));
        int ind = 0;
        int status;
        do{
          printf("     > ");
          char *string = malloc(100);
          size_t length = 0;
          status = getline(&string,&length,stdin);
          lines[ind] = string;
          ind++;
        }while(status != -1);
        printf("\n");
        for(int i = 0; i < ind; i++)
            printf("%s",lines[i]);
      } 
    }
    else if(nindex >= 1){
      if(nindex == 1 && strcmp(nonsense[0],"--help") == 0){
        cat_help();
      }
      else{
        printf("cat: invalid option -- '%s'\nTry 'cat --help' for more information.\n",nonsense[0]);
        exit(0);
      }
    }
    if(strcmp(param_merge,"b") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_b(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"E") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_E(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"n") == 0 || strcmp(param_merge,"bn") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_n(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"s") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_s(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"Eb") == 0 || strcmp(param_merge,"Ebn") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_bE(parsed_command_array, files, findex, redflag, next_redirect); 
    }
    else if(strcmp(param_merge,"bs") == 0 || strcmp(param_merge,"bns") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_bs(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"En") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_En(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"Es") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_Es(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"ns") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_ns(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"Ebs") == 0 || strcmp(param_merge,"Ebns") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_Ebs(parsed_command_array, files, findex, redflag, next_redirect);
    }
    else if(strcmp(param_merge,"Ens") == 0){
      FILE *f;
      if(redflag == 1){
        f = fopen(next_redirect,"w");
        fclose(f);
      }
      cat_Ens(parsed_command_array, files, findex, redflag, next_redirect);
    }
}
  

/**IMPLEMENT HEAD**/
int head_simple_param_checker(char *param){
  if(strcmp(param,"-q") == 0)
    return 1;
  else if(strcmp(param,"-v") == 0)
    return 2;
  else return -1;
}
int number_checker(char *string){
  char c, *digits = "0123456789";
    do{
        c = *(string++);
        if(strchr(digits,c) == NULL)
            return 0;
    } while (c != '\0');
    return 1;
}
int head_param_numbers_checker(char *param){
  char *comm = "cnqv", *digits = "0123456789";
  int i, ok_command = 0;
  int ok_digits = 1;
  if(param[0] == '-' && strchr(comm,param[1]) != NULL){
    ok_command = 1;
    for(i = 2; i < strlen(param); i++){
      if(strchr(digits,param[i])==NULL)
        ok_digits = 0;
    }
  }
  if(ok_command == 1 && ok_digits == 1)
    return 1;
  else return 0;
}
void head_c(char **parsed_command_array, char **files, int n, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    if(f == 0){
      int j = 0;
      while(j < n){
          readstring(string_for_empty_commands);
          int m = strlen(string_for_empty_commands);
          if(j+m < n){
            j+=m;
            printf("%s",string_for_empty_commands);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              printf("%c",string_for_empty_commands[cntr]);
              cntr++;
            }
            j+=cntr;
            printf("\n");
            break;
          }
      }
    }
    for(int i = 0; i < f; i++){
      if(f > 1){
        if(strcmp(files[i],"-") == 0) printf("==> standard input <==\n");
        else printf("==> %s <==\n",files[i]);
      }
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        while(j < n){
          readstring(string_for_empty_commands);
          int m = strlen(string_for_empty_commands);
          if(j+m < n){
            j+=m;
            printf("%s",string_for_empty_commands);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              printf("%c",string_for_empty_commands[cntr]);
              cntr++;
            }
            j+=cntr;
            printf("\n");
            break;
          }
        }
      }
      else{
        file_parse = fopen(files[i], "r");
        int j = 0;
        while(j < n){
          char *string = malloc(100);
          getline(&string, &length, file_parse);
          int m = strlen(string);
          if(j+m < n){
            j+=m;
            printf("%s",string);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              printf("%c",string[cntr]);
              cntr++;
            }
            j+=cntr;
            break;
          }
          if(string) free(string);
        }
        printf("\n");
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    if(f == 0){
      int j = 0;
      while(j < n){
          readstring(string_for_empty_commands);
          int m = strlen(string_for_empty_commands);
          if(j+m < n){
            j+=m;
            fprintf(ff,"%s",string_for_empty_commands);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              fprintf(ff,"%c",string_for_empty_commands[cntr]);
              cntr++;
            }
            j+=cntr;
            fprintf(ff,"\n");
            break;
          }
        }
    }
    for(int i = 0; i < f; i++){
      if(f > 1){
        if(strcmp(files[i],"-") == 0) fprintf(ff,"==> standard input <==\n");
        else fprintf(ff,"==> %s <==\n",files[i]);
      }
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        while(j < n){
          readstring(string_for_empty_commands);
          int m = strlen(string_for_empty_commands);
          if(j+m < n){
            j+=m;
            fprintf(ff,"%s",string_for_empty_commands);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              fprintf(ff,"%c",string_for_empty_commands[cntr]);
              cntr++;
            }
            j+=cntr;
            fprintf(ff,"\n");
            break;
          }
        }
      }
      else{
        file_parse = fopen(files[i], "r");
        int j = 0;
        while(j < n){
          char *string = malloc(100);
          getline(&string, &length, file_parse);
          int m = strlen(string);
          if(j+m < n){
            j+=m;
            fprintf(ff,"%s",string);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              fprintf(ff,"%c",string[cntr]);
              cntr++;
            }
            j+=cntr;
            break;
          }
          if(string) free(string);
        }
        fprintf(ff,"\n");
      }    
    }
    fclose(ff);
  }
  else if(flag == 3){ 
    FILE *ff;
    ff = fopen(red,"r");
    if(f == 0){
      char *string = malloc(100);
      size_t length = 0;
      int j = 0;
      while(j < n){
          char *string = malloc(100);
          getline(&string, &length, ff);
          int m = strlen(string);
          if(j+m < n){
            j+=m;
            printf("%s",string);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              printf("%c",string[cntr]);
              cntr++;
            }
            j+=cntr;
            break;
          }
          if(string) free(string);
      }
      printf("\n");
      fclose(ff);
    }

    if(f >= 1){
      for(int i = 0; i < f; i++){
        if(f > 1){
          if(strcmp(files[i],"-")==0)
            printf("==> standard input <==\n");
          else printf("==> %s <==\n",files[i]);
        } 
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          int j = 0;
          while(j < n){
              char *string = malloc(100);
              getline(&string, &length, stdin);
              int m = strlen(string);
              if(j+m < n){
                j+=m;
                printf("%s",string);
              }
              else{
                int cntr = 0;
                while(j + cntr < n){
                  printf("%c",string[cntr]);
                  cntr++;
                }
                j+=cntr;
                break;
              }
              if(string) free(string);
          }
          printf("\n");
          fclose(ff);
        }
        else{
          FILE *fff = fopen(files[i], "r");        
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          int j = 0;
          while(j < n){
              char *string = malloc(100);
              getline(&string, &length, fff);
              int m = strlen(string);
              if(j+m < n){
                j+=m;
                printf("%s",string);
              }
              else{
                int cntr = 0;
                while(j + cntr < n){
                  printf("%c",string[cntr]);
                  cntr++;
                }
                j+=cntr;
                break;
              }
              if(string) free(string);
          }
          printf("\n");
          fclose(fff);
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(ff == NULL){
        printf("bash: %s: No such file or directory\n",red);
      }
    }
  }  
} 

void head_n(char **parsed_command_array, char **files, int n, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    if(f == 0){
      int j = 0;
      char *string = malloc(100);
      while(j < n){
        readstring(string_for_empty_commands);
        printf("%s",string_for_empty_commands);
        j++;
      }
      if(string) free(string);
    }
    for(int i = 0; i < f; i++){
      if(f > 1){
        if(strcmp(files[i],"-") == 0) printf("==> standard input <==\n");
        else printf("==> %s <==\n",files[i]);
      }
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        char *string = malloc(100);
        while(j < n){
          readstring(string_for_empty_commands);
          printf("%s",string_for_empty_commands);
          j++;
        }
        if(string) free(string);
      }
      else{
        int j = 0, status;
        file_parse = fopen(files[i], "r");
        char *string = malloc(100);
        while(j < n){
          status = getline(&string, &length, file_parse);
          if(status != -1) printf("%s", string);
          j++;
        }
        if(string) free(string);
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    if(f == 0){
      int j = 0;
      while(j < n){
        readstring(string_for_empty_commands);
        fprintf(ff,"%s",string_for_empty_commands);
        j++;
      }
    }
    for(int i = 0; i < f; i++){
      if(f > 1){
        if(strcmp(files[i],"-") == 0) fprintf(ff,"==> standard input <==\n");
        else fprintf(ff,"==> %s <==\n",files[i]);
      }
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        while(j < n){
          readstring(string_for_empty_commands);
          fprintf(ff,"%s",string_for_empty_commands);
          j++;
        }
      }
      else{
        file_parse = fopen(files[i], "r");
        int j = 0;
        char *string = malloc(100);
        int status;
        while(j < n){
          status = getline(&string, &length, file_parse);
          if(status != -1) fprintf(ff,"%s", string);
          j++;
        }
      }    
    }
    fclose(ff);
  }
  else if(flag == 3){ 
    FILE *ff;
    ff = fopen(red,"r");
    if(f == 0){
      char *string = malloc(100);
      size_t length = 0;
      int j = 0;
      int status;
      while(j < n){
        status = getline(&string, &length, ff);
        if(status != -1) printf("%s", string);
        j++;
      }
      fclose(ff);
    }

    if(f >= 1){
      for(int i = 0; i < f; i++){
        if(f > 1){
          if(strcmp(files[i],"-")==0)
            printf("==> standard input <==\n");
          else printf("==> %s <==\n",files[i]);
        } 
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          int j = 0;
          while(j < n){
            readstring(string_for_empty_commands);
            printf("%s",string_for_empty_commands);
            j++;
          }
        }
        else{
          FILE *fff = fopen(files[i], "r");        
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          int j = 0;
          int status;
          while(j < n){
            status = getline(&string, &length, fff);
            if(status != -1) printf("%s", string);
            j++;
          }
          fclose(fff);
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(ff != NULL){
        if(f > 1){
          printf("==> standard input <==\n");
        }
        char *string = malloc(100);
        size_t length = 0;
        int j = 0;
        int status;
        while(j < n){
          status = getline(&string, &length, ff);
          if(status != -1) printf("%s", string);
          j++;
        }
        fclose(ff);
      }
      else printf("bash: %s: No such file or directory\n",red);
    }
  }
}
void head_q(char **parsed_command_array, char **files, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    if(f == 0){
      int j = 0;
      char *string = malloc(100);
      while(j < 10){
        readstring(string_for_empty_commands);
        printf("%s",string_for_empty_commands);
        j++;
      }
      if(string) free(string);
    }
    for(int i = 0; i < f; i++){
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        char *string = malloc(100);
        while(j < 10){
          readstring(string_for_empty_commands);
          printf("%s",string_for_empty_commands);
          j++;
        }
        if(string) free(string);
      }
      else{
        int j = 0, status;
        file_parse = fopen(files[i], "r");
        char *string = malloc(100);
        while(j < 10){
          status = getline(&string, &length, file_parse);
          if(status != -1) printf("%s", string);
          j++;
        }
        if(string) free(string);
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    if(f == 0){
      int j = 0;
      while(j < 10){
        readstring(string_for_empty_commands);
        fprintf(ff,"%s",string_for_empty_commands);
        j++;
      }
    }
    for(int i = 0; i < f; i++){
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        while(j < 10){
          readstring(string_for_empty_commands);
          fprintf(ff,"%s",string_for_empty_commands);
          j++;
        }
      }
      else{
        file_parse = fopen(files[i], "r");
        int j = 0;
        char *string = malloc(100);
        int status;
        while(j < 10){
          status = getline(&string, &length, file_parse);
          if(status != -1) fprintf(ff,"%s", string);
          j++;
        }
      }    
    }
    fclose(ff);
  }
  else if(flag == 3){ 
    FILE *ff;
    ff = fopen(red,"r");
    if(f == 0){
      char *string = malloc(100);
      size_t length = 0;
      int j = 0;
      int status;
      while(j < 10){
        status = getline(&string, &length, ff);
        if(status != -1) printf("%s", string);
        j++;
      }
      fclose(ff);
    }

    if(f >= 1){
      for(int i = 0; i < f; i++){ 
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          int j = 0;
          while(j < 10){
            readstring(string_for_empty_commands);
            printf("%s",string_for_empty_commands);
            j++;
          }
        }
        else{
          FILE *fff = fopen(files[i], "r");        
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          int j = 0;
          int status;
          while(j < 10){
            status = getline(&string, &length, fff);
            if(status != -1) printf("%s", string);
            j++;
          }
          fclose(fff);
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(ff != NULL){
        char *string = malloc(100);
        size_t length = 0;
        int j = 0;
        int status;
        while(j < 10){
          status = getline(&string, &length, ff);
          if(status != -1) printf("%s", string);
          j++;
        }
        fclose(ff);
      }
      else printf("bash: %s: No such file or directory\n",red);
    }
  }
}
void head_v(char **parsed_command_array, char **files, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    if(f == 0){
      printf("==> standard input <==\n");
      int j = 0;
      char *string = malloc(100);
      while(j < 10){
        readstring(string_for_empty_commands);
        printf("%s",string_for_empty_commands);
        j++;
      }
      if(string) free(string);
    }
    for(int i = 0; i < f; i++){
      if(strcmp(files[i],"-") == 0) 
        printf("==> standard input <==\n");
      else 
        printf("==> %s <==\n",files[i]);
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        char *string = malloc(100);
        while(j < 10){
          readstring(string_for_empty_commands);
          printf("%s",string_for_empty_commands);
          j++;
        }
        if(string) free(string);
      }
      else{
        int j = 0, status;
        file_parse = fopen(files[i], "r");
        char *string = malloc(100);
        while(j < 10){
          status = getline(&string, &length, file_parse);
          if(status != -1) printf("%s", string);
          j++;
        }
        if(string) free(string);
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    if(f == 0){
      fprintf(ff,"==> standard input <==\n");
      int j = 0;
      while(j < 10){
        readstring(string_for_empty_commands);
        fprintf(ff,"%s",string_for_empty_commands);
        j++;
      }
    }
    for(int i = 0; i < f; i++){
      if(strcmp(files[i],"-") == 0)
        fprintf(ff,"==> standard input <==\n");
      else
        fprintf(ff,"==> %s <==\n",files[i]);
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        while(j < 10){
          readstring(string_for_empty_commands);
          fprintf(ff,"%s",string_for_empty_commands);
          j++;
        }
      }
      else{
        file_parse = fopen(files[i], "r");
        int j = 0;
        char *string = malloc(100);
        int status;
        while(j < 10){
          status = getline(&string, &length, file_parse);
          if(status != -1) fprintf(ff,"%s", string);
          j++;
        }
      }    
    }
    fclose(ff);
  }
  else if(flag == 3){ 
    FILE *ff;
    ff = fopen(red,"r");
    if(f == 0){
      char *string = malloc(100);
      size_t length = 0;
      int j = 0;
      int status;
      while(j < 10){
        status = getline(&string, &length, ff);
        if(status != -1) printf("%s", string);
        j++;
      }
      fclose(ff);
    }

    if(f >= 1){
      for(int i = 0; i < f; i++){
        if(strcmp(files[i],"-")==0)
          printf("==> standard input <==\n");
        else printf("==> %s <==\n",files[i]); 
        if(strcmp(files[i],"-") == 0){
          char *string = malloc(100);
          size_t length = 0;
          int j = 0;
          while(j < 10){
            readstring(string_for_empty_commands);
            printf("%s",string_for_empty_commands);
            j++;
          }
        }
        else{
          FILE *fff = fopen(files[i], "r");        
          size_t length = 0;
          ssize_t field;
          char *string = malloc(100);
          int j = 0;
          int status;
          while(j < 10){
            status = getline(&string, &length, fff);
            if(status != -1) printf("%s", string);
            j++;
          }
          fclose(fff);
          if(string)
            free(string);
        }           
      }
    }
    else{
      if(ff != NULL){
        char *string = malloc(100);
        size_t length = 0;
        int j = 0;
        int status;
        while(j < 10){
          status = getline(&string, &length, ff);
          if(status != -1) printf("%s", string);
          j++;
        }
        fclose(ff);
      }
      else printf("bash: %s: No such file or directory\n",red);
    }
  }
}
void head_cv(char **parsed_command_array, char **files, int n, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    for(int i = 0; i < f; i++){
      if(strcmp(files[i],"-") == 0) printf("==> standard input <==\n");
        else printf("==> %s <==\n",files[i]);
  
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        while(j < n){
          readstring(string_for_empty_commands);
          int m = strlen(string_for_empty_commands);
          if(j+m < n){
            j+=m;
            printf("%s",string_for_empty_commands);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              printf("%c",string_for_empty_commands[cntr]);
              cntr++;
            }
            j+=cntr;
            printf("\n");
            break;
          }
        }
      }
      else{
        file_parse = fopen(files[i], "r");
        int j = 0;
        while(j < n){
          char *string = malloc(100);
          getline(&string, &length, file_parse);
          int m = strlen(string);
          if(j+m < n){
            j+=m;
            printf("%s",string);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              printf("%c",string[cntr]);
              cntr++;
            }
            j+=cntr;
            break;
          }
          if(string) free(string);
        }
        printf("\n");
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    for(int i = 0; i < f; i++){
      if(f > 1){
        fprintf(ff,"==> %s <==\n",files[i]);
      } 
      int j = 0, status;
      file_parse = fopen(files[i], "r");
      if(file_parse!=NULL){
        char *string = malloc(100);
        while(j < n){
          char *string = malloc(100);
          getline(&string, &length, file_parse);
          int m = strlen(string);
          if(j+m < n){
            j+=m;
            fprintf(ff,"%s",string);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              fprintf(ff,"%c",string[cntr]);
              cntr++;
            }
            j+=cntr;
            break;
          }
        }
        fprintf(ff,"\n");
        fclose(file_parse);
        if(string) free(string);
      }    
    }
    fclose(ff);
  }
}
void head_nv(char **parsed_command_array, char **files, int n, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    for(int i = 0; i < f; i++){
      if(strcmp(files[i],"-") == 0) printf("==> standard input <==\n");
        else printf("==> %s <==\n",files[i]);
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        char *string = malloc(100);
        while(j < n){
          readstring(string_for_empty_commands);
          printf("%s",string_for_empty_commands);
          j++;
        }
        if(string) free(string);
        exit_process(parsed_command_array);
      }
      else{
        int j = 0, status;
        file_parse = fopen(files[i], "r");
        char *string = malloc(100);
        while(j < n){
          status = getline(&string, &length, file_parse);
          if(status != -1) printf("%s", string);
          j++;
        }
        if(string) free(string);
      }
      exit_process(parsed_command_array);
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    for(int i = 0; i < f; i++){
      fprintf(ff,"==> %s <==\n",files[i]);
      int j = 0, status;
      file_parse = fopen(files[i], "r");
      if(file_parse!=NULL){
        char *string = malloc(100);
        while(j < n){
          status = getline(&string, &length, file_parse);
          if(status != -1)
            fprintf(ff,"%s",string);
          j++;
        }
        fclose(file_parse);
        if(string) free(string);
      }    
    }
    fclose(ff);
  }
}
void head_cq(char **parsed_command_array, char **files, int n, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    for(int i = 0; i < f; i++){
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        while(j < n){
          readstring(string_for_empty_commands);
          int m = strlen(string_for_empty_commands);
          if(j+m < n){
            j+=m;
            printf("%s",string_for_empty_commands);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              printf("%c",string_for_empty_commands[cntr]);
              cntr++;
            }
            j+=cntr;
            printf("\n");
            break;
          }
        }
      }
      else{
        file_parse = fopen(files[i], "r");
        int j = 0;
        while(j < n){
          char *string = malloc(100);
          getline(&string, &length, file_parse);
          int m = strlen(string);
          if(j+m < n){
            j+=m;
            printf("%s",string);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              printf("%c",string[cntr]);
              cntr++;
            }
            j+=cntr;
            break;
          }
          if(string) free(string);
        }
        printf("\n");
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    for(int i = 0; i < f; i++){
      int j = 0, status;
      file_parse = fopen(files[i], "r");
      if(file_parse!=NULL){
        char *string = malloc(100);
        int j = 0;
        while(j < n){
          char *string = malloc(100);
          getline(&string, &length, file_parse);
          int m = strlen(string);
          if(j+m < n){
            j+=m;
            fprintf(ff,"%s",string);
          }
          else{
            int cntr = 0;
            while(j + cntr < n){
              fprintf(ff,"%c",string[cntr]);
              cntr++;
            }
            j+=cntr;
            break;
          }
          if(string) free(string);
        }
        fprintf(ff,"\n");
        fclose(file_parse);
      }    
    }
    fclose(ff);
  }
}
void head_nq(char **parsed_command_array, char **files, int n, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    for(int i = 0; i < f; i++){
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        char *string = malloc(100);
        while(j < n){
          readstring(string_for_empty_commands);
          printf("%s",string_for_empty_commands);
          j++;
        }
        if(string) free(string);
      }
      else{
        int j = 0, status;
        file_parse = fopen(files[i], "r");
        char *string = malloc(100);
        while(j < n){
          status = getline(&string, &length, file_parse);
          if(status != -1) printf("%s", string);
          j++;
        }
        if(string) free(string);
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    for(int i = 0; i < f; i++){
      int j = 0, status;
      file_parse = fopen(files[i], "r");
      if(file_parse!=NULL){
        char *string = malloc(100);
        while(j < n){
          status = getline(&string, &length, file_parse);
          if(status != -1)
            fprintf(ff,"%s",string);
          j++;
        }
        fclose(file_parse);
        if(string) free(string);
      }    
    }
    fclose(ff);
  }
}
void simple_head(char **parsed_command_array, char **files, int f, int flag, char *red){
  FILE *file_parse;
  size_t length = 0;
  if(flag == -1){
    for(int i = 0; i < f; i++){
      if(f > 1){
        if(strcmp(files[i],"-") == 0) 
          printf("==> standard input <==\n");
        else printf("==> %s <==\n",files[i]);
      } 
      if(strcmp(files[i],"-") == 0){
        int j = 0;
        while(j < 10){
          readstring(string_for_empty_commands);
          printf("%s",string_for_empty_commands);
          j++;
        }
      }
      else{
        int j = 0, status;
        file_parse = fopen(files[i], "r");
        char *string = malloc(100);
        while(j < 10){
          status = getline(&string, &length, file_parse);
          if(status != -1) printf("%s",string);
          j++;
        }
        if(string) free(string);
      }
    }
  }
  else if(flag == 1 || flag == 2){
    FILE *ff;
    ff = fopen(red,"a+");
    for(int i = 0; i < f; i++){
      if(f > 1){
        fprintf(ff,"==> %s <==\n",files[i]);
      } 
      int j = 0, status;
      file_parse = fopen(files[i], "r");
      if(file_parse!=NULL){
        char *string = malloc(100);
        while(j < 10){
          status = getline(&string, &length, file_parse);
          if(status != -1)
            fprintf(ff,"%s",string);
          j++;
        }
        fclose(file_parse);
        if(string) free(string);
      }    
    }
    fclose(ff);
  }
  else if(flag == 3){
    FILE *ff;
    ff = fopen(red,"r");
    if(ff != NULL){
      char *string = malloc(100);
      size_t length = 0;
      int count = 0;
      while(count < 10 && getline(&string, &length,ff) != -1){
        printf("%s",string);
        count++;
      }
      fclose(ff);
    }
    else{
      printf("bash: %s: No such file or directory\n",red);
    }
  }
  else if(flag == 4){
    char **lines = malloc(64*sizeof(char*));
    int ind = 0, count = 0;
    int status;
    do{
      printf("     > ");
      char *string = malloc(100);
      size_t length = 0;
      status = getline(&string,&length,stdin);
      lines[ind] = string;
      ind++;
      count++;
    }while(status != -1 && count < 10);
    printf("\n");
    for(int i = 0; i < ind; i++)
      printf("%s",lines[i]);
  }
}
void head_help(){
  printf("Usage: head [OPTION]... [FILE]...\nPrint the first 10 lines of each FILE to standard output.\nWith more than one FILE, precede each with a header giving the file name.\n\nWith no FILE, or when FILE is -, read standard input.\n\nMandatory arguments to long options are mandatory for short options too.\n  -c, --bytes=[-]NUM       print the first NUM bytes of each file;\n  -n, --lines=[-]NUM       print the first NUM lines instead of the first 10;\n  -q, --quiet, --silent    never print headers giving file names\n  -v, --verbose            always print headers giving file names\n");
}
void parse_head_args(char **parsed_command_array){
  int index = 1, pindex = 0, findex = 0, nindex = 0, nnindex = 0, nsindex = 0;
  int buffer_size = 64;
  int redflag = -1;
  char *next_redirect = malloc(100);
  char **parameters = malloc(buffer_size * sizeof(char *));
  char **files = malloc(buffer_size * sizeof(char *));
  char **number_command = malloc(buffer_size * sizeof(char *));
  char **nr_number_command = malloc(buffer_size * sizeof(char *));
  char **nonsense = malloc(buffer_size * sizeof(char *));
  while(parsed_command_array[index] != NULL){
    if(head_simple_param_checker(parsed_command_array[index]) != -1){
      parameters[pindex] = parsed_command_array[index];
      index++;
      pindex++;
    }
    else if(file_checker(parsed_command_array[index]) == 1){
      files[findex] = parsed_command_array[index];
      findex++;
      index++; 
    }
    else if(index >= 2 && number_checker(parsed_command_array[index]) == 1){
      char combination[10] = "";
      strcat(combination,parsed_command_array[index-1]);
      strcat(combination,parsed_command_array[index]);
      number_command[nindex] = combination;
      parameters[pindex] = number_command[nindex];
      nindex++;
      index++;
      pindex++;
    }
    else if(head_param_numbers_checker(parsed_command_array[index]) == 1){
        nr_number_command[nnindex] = parsed_command_array[index];
        if(strcmp(parsed_command_array[index],"-n")!=0 && strcmp(parsed_command_array[index],"-c")!=0){
          parameters[pindex] = parsed_command_array[index];
          pindex++;
        }
        nnindex++;
        index++; 
    }
    else if(strcmp(parsed_command_array[index],">") == 0){
      if(parsed_command_array[index+1] != NULL){
        strcpy(next_redirect,parsed_command_array[index+1]);
        index+=2;
        redflag = 1;
      }
      else printf("bash: syntax error near unexpected token `newline'");
    }
    else if(strcmp(parsed_command_array[index],">>") == 0){
      if(parsed_command_array[index+1] != NULL){
        strcpy(next_redirect,parsed_command_array[index+1]);
        index+=2;
        redflag = 2;
      }
      else printf("bash: syntax error near unexpected token `newline'");
    }
    else if(strcmp(parsed_command_array[index],"<") == 0){
      if(parsed_command_array[index+1] != NULL){
        strcpy(next_redirect,parsed_command_array[index+1]);
        index+=2;
        redflag = 3;
      }
      else printf("bash: syntax error near unexpected token `newline'");
    }
    else if(strcmp(parsed_command_array[index],"<<") == 0){
      if(parsed_command_array[index+1] != NULL){
        strcpy(next_redirect,parsed_command_array[index+1]);
        index+=2;
        redflag = 4;
      }
      else printf("bash: syntax error near unexpected token `newline'");
    }
    else{
      nonsense[nsindex] = parsed_command_array[index];
      nsindex++;
      index++;
    }
  }

  char *param_merge = malloc(20);
  strcpy(param_merge,"");
  char *comm = "cnqv";
  char **number_array = malloc(64*sizeof(char*));
  int aindex = 0;
  
  for(int i = 0; i < pindex; i++){
    char *p;
    p = parameters[i];
    while(parameters[i]){
      if(number_checker(p)==1){
        number_array[aindex] = p;
        aindex++;
        break;
      }
      p++;
    }
    char *search = strchr(parameters[i],'c');
    if(search!=NULL){
      char c = search[0];
      strncat(param_merge,&c,1);
    }
    char *search1 = strchr(parameters[i],'n');
    if(search1!=NULL){
      char n = search1[0];
      strncat(param_merge,&n,1);
    }
    char *search2 = strchr(parameters[i],'q');
    if(search2!=NULL){
      char q = search2[0];
      strncat(param_merge,&q,1);
    }
    char *search3 = strchr(parameters[i],'v');
    if(search3!=NULL){
      char v = search3[0];
      strncat(param_merge,&v,1);
    }
    
  }
  int *number_ints = malloc(64);
  for(int i = 0; i < aindex; i++){
    number_ints[i] = atoi(number_array[i]);
  }


  if((pindex == 0 && findex != 0 && nsindex ==0)||(pindex == 0 && findex == 0 && nsindex ==0 && redflag == 3) || (pindex == 0 && findex == 0 && nsindex ==0 && redflag == 4)){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    simple_head(parsed_command_array,files,findex,redflag,next_redirect);
  }
  else if(pindex == 0 && findex == 0 && nsindex == 0){
    if(redflag == -1){
      int j = 0;
      char *string = malloc(100);
      size_t length = 0;
      while(j < 10){
        getline(&string,&length,stdin);
        printf("%s",string);
        j++;
        free(string);
        string = malloc(100);
      }
    }
    else if(redflag == 1){
      FILE *f;
      f = fopen(next_redirect,"w"); 
      int j = 0;
      char *string = malloc(100);
      size_t length = 0;
      while(j < 10){
        getline(&string,&length,stdin);
        fprintf(f,"%s",string);
        j++;
        free(string);
        string = malloc(100);
      }
      fclose(f);
    }
    else if(redflag == 2){
      FILE *f;
      f = fopen(next_redirect,"a+"); 
      int j = 0;
      char *string = malloc(100);
      size_t length = 0;
      while(j < 10){
        getline(&string,&length,stdin);
        fprintf(f,"%s",string);
        j++;
        free(string);
        string = malloc(100);
      }
      fclose(f);
    }
    else if(redflag == 4){
      char **lines = malloc(64*sizeof(char*));
      int ind = 0;
      int status;
      do{
        printf("     > ");
        char *string = malloc(100);
        size_t length = 0;
        status = getline(&string,&length,stdin);
        lines[ind] = string;
        ind++;
      }while(status != -1 && ind <= 10);
      printf("\n");
      for(int i = 0; i < ind; i++)
          printf("%s",lines[i]);
    } 
  }
  else if(nsindex != 0){
    if(nsindex == 1 && strcmp(nonsense[0],"--help") == 0){
      head_help();
    }
    else{
      printf("head: invalid option -- '%s'\nTry 'head --help' for more information.\n",nonsense[0]);
      exit(0);
    } 
  }
      
  else if(strcmp(param_merge,"c")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_c(parsed_command_array,files,number_ints[0],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"nc")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_c(parsed_command_array,files,number_ints[1],findex,redflag,next_redirect);
  }
  else if((strcmp(param_merge,"cq")==0||strcmp(param_merge,"qc")==0||strcmp(param_merge,"vqc")==0||strcmp(param_merge,"vcq")==0||strcmp(param_merge,"cvq")==0)){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_cq(parsed_command_array,files,number_ints[0],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"n")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_n(parsed_command_array,files,number_ints[0],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"cn")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_n(parsed_command_array,files,number_ints[1],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"nq")==0||strcmp(param_merge,"qn")==0||strcmp(param_merge,"vqn")==0||strcmp(param_merge,"vnq")==0||strcmp(param_merge,"nvq")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_nq(parsed_command_array,files,number_ints[0],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"q")==0||strcmp(param_merge,"vq")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_q(parsed_command_array,files,findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"v")==0||strcmp(param_merge,"qv")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_v(parsed_command_array,files,findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"cv")==0||strcmp(param_merge,"vc")==0||strcmp(param_merge,"cqv")==0||strcmp(param_merge,"qcv")==0||strcmp(param_merge,"qvc")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_cv(parsed_command_array,files,number_ints[0],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"nv")==0||strcmp(param_merge,"vn")==0||strcmp(param_merge,"qnv")==0||strcmp(param_merge,"nqv")==0||strcmp(param_merge,"qvn")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_nv(parsed_command_array,files,number_ints[0],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"cnq")==0||strcmp(param_merge,"qcn")==0||strcmp(param_merge,"cvnq")==0||strcmp(param_merge,"vcnq")==0||strcmp(param_merge,"vcqn")==0||strcmp(param_merge,"cnvq")==0||strcmp(param_merge,"cvqn")==0||strcmp(param_merge,"vqcn")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_nq(parsed_command_array,files,number_ints[1],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"ncq")==0||strcmp(param_merge,"nqc")==0||strcmp(param_merge,"qnc")==0||strcmp(param_merge,"vncq")==0||strcmp(param_merge,"ncvq")==0||strcmp(param_merge,"nvcq")==0||strcmp(param_merge,"nvqc")==0||strcmp(param_merge,"vnqc")==0||strcmp(param_merge,"vqnc")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_cq(parsed_command_array,files,number_ints[1],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"cnv")==0||strcmp(param_merge,"vcn")==0||strcmp(param_merge,"cvn")==0||strcmp(param_merge,"cnqv")==0||strcmp(param_merge,"cqvn")==0||strcmp(param_merge,"cqnv")==0||strcmp(param_merge,"cqnv")==0||strcmp(param_merge,"qvcn")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_nv(parsed_command_array,files,number_ints[1],findex,redflag,next_redirect);
  }
  else if(strcmp(param_merge,"ncv")==0||strcmp(param_merge,"nvc")==0||strcmp(param_merge,"vnc")==0||strcmp(param_merge,"nqcv")==0||strcmp(param_merge,"ncqv")==0||strcmp(param_merge,"qncv")==0||strcmp(param_merge,"qnvc")==0||strcmp(param_merge,"qvnc")==0||strcmp(param_merge,"nqvc")==0){
    FILE *f;
    if(redflag == 1){
      f = fopen(next_redirect,"w");
      fclose(f);
    }
    head_cv(parsed_command_array,files,number_ints[1],findex,redflag,next_redirect);
  }
}

/**IMPLEMENT ENV**/

char env_output[100][10000];
int env_cnt = 0;

int is_instance(char *string){
  char env_instances[60][100] = {"SHELL","SESSION_MANAGER","QT_ACCESSIBILITY","COLORTERM","XDG_CONFIG_DIRS","XDG_MENU_PREFIX","GNOME_DESKTOP_SESSION_ID","LC_ADDRESS","GNOME_SHELL_SESSION_MODE","LC_NAME","SSH_AUTH_SOCK","XMODIFIERS","DESKTOP_SESSION","LC_MONETARY","SSH_AGENT_PID","GTK_MODULES","PWD","LOGNAME","XDG_SESSION_DESKTOP","XDG_SESSION_TYPE","GPG_AGENT_INFO","XAUTHORITY","GJS_DEBUG_TOPICS","WINDOWPATH","HOME","USERNAME","IM_CONFIG_PHASE","LC_PAPER","LANG","LS_COLORS","XDG_CURRENT_DESKTOP","VTE_VERSION","GNOME_TERMINAL_SCREEN","INVOCATION_ID","MANAGERPID","GJS_DEBUG_OUTPUT","LESSCLOSE","XDG_SESSION_CLASS","TERM","LC_IDENTIFICATION","LESSOPEN","USER","GNOME_TERMINAL_SERVICE","DISPLAY","SHLVL","LC_TELEPHONE","QT_IM_MODULE","LC_MEASUREMENT","XDG_RUNTIME_DIR","LC_TIME","JOURNAL_STREAM","XDG_DATA_DIRS","PATH","GDMSESSION","DBUS_SESSION_BUS_ADDRESS","LC_NUMERIC","OLDPWD","_"};
  for(int i = 0; i < 58; i++)
    if(strcmp(string,env_instances[i]) == 0){
      return i;
    }
  return -1;
}

void parse_env_args(char **parsed_command_array, int arg, char **argv, char *envp[]){
    int index = 1, pindex = 0, nindex = 0;
    int buffer_size = 64;
    int cmd;
    int deleted[58] = {1};
    char env_instances[60][100] = {"SHELL","SESSION_MANAGER","QT_ACCESSIBILITY","COLORTERM","XDG_CONFIG_DIRS","XDG_MENU_PREFIX","GNOME_DESKTOP_SESSION_ID","LC_ADDRESS","GNOME_SHELL_SESSION_MODE","LC_NAME","SSH_AUTH_SOCK","XMODIFIERS","DESKTOP_SESSION","LC_MONETARY","SSH_AGENT_PID","GTK_MODULES","PWD","LOGNAME","XDG_SESSION_DESKTOP","XDG_SESSION_TYPE","GPG_AGENT_INFO","XAUTHORITY","GJS_DEBUG_TOPICS","WINDOWPATH","HOME","USERNAME","IM_CONFIG_PHASE","LC_PAPER","LANG","LS_COLORS","XDG_CURRENT_DESKTOP","VTE_VERSION","GNOME_TERMINAL_SCREEN","INVOCATION_ID","MANAGERPID","GJS_DEBUG_OUTPUT","LESSCLOSE","XDG_SESSION_CLASS","TERM","LC_IDENTIFICATION","LESSOPEN","USER","GNOME_TERMINAL_SERVICE","DISPLAY","SHLVL","LC_TELEPHONE","QT_IM_MODULE","LC_MEASUREMENT","XDG_RUNTIME_DIR","LC_TIME","JOURNAL_STREAM","XDG_DATA_DIRS","PATH","GDMSESSION","DBUS_SESSION_BUS_ADDRESS","LC_NUMERIC","OLDPWD","_"};
    int simple_env = -1;
    char **catarg = malloc(buffer_size*sizeof(char*));
    int cind = 0;
    char **headarg = malloc(buffer_size*sizeof(char*));
    int hind = 0;

    int ind = 0;
    int flag = -1;
    char *next_redirect;
    while(parsed_command_array[ind]!=NULL){
      if(strcmp(parsed_command_array[ind],">") == 0){
        if(parsed_command_array[ind+1] != NULL){
          strcpy(next_redirect,parsed_command_array[ind+1]);
          ind+=2;
          flag = 1;
        }
        else printf("bash: syntax error near unexpected token `newline'");
      }
      else if(strcmp(parsed_command_array[ind],">>") == 0){
        if(parsed_command_array[ind+1] != NULL){
          strcpy(next_redirect,parsed_command_array[ind+1]);
          ind+=2;
          flag = 2;
        }
        else printf("bash: syntax error near unexpected token `newline'");
      }
      else if(strcmp(parsed_command_array[ind],"<") == 0){
        if(parsed_command_array[ind+1] != NULL){
          strcpy(next_redirect,parsed_command_array[ind+1]);
          ind+=2;
          flag = 3;
        }
        else printf("bash: syntax error near unexpected token `newline'");
      }
      else if(strcmp(parsed_command_array[ind],"<<") == 0){
        if(parsed_command_array[ind+1] != NULL){
          strcpy(next_redirect,parsed_command_array[ind+1]);
          ind+=2;
          flag = 4;
        }
        else printf("bash: syntax error near unexpected token `newline'");
      }
      ind++;
    }
    if(strcmp(parsed_command_array[0],"env") == 0 && parsed_command_array[1] == NULL){
      if(flag == -1 || flag == 3 || flag == 4){
        for(int i = 0; envp[i]; i++)
          printf("%s\n",envp[i]);
      }
      else if(flag == 1){
        FILE *f;
        f = fopen(next_redirect,"r");
        if(f != NULL){
          for(int i = 0; envp[i]; i++)
            fprintf(f,"%s\n",envp[i]);
          fclose(f);
        }
        else printf("File does not exist");
      }
      else if(flag == 2){
        FILE *f;
        f = fopen(next_redirect,"a+");
        for(int i = 0; envp[i]; i++)
          fprintf(f,"%s\n",envp[i]);
        fclose(f);
      }
    }
    else{
    while(parsed_command_array[index] != NULL){
      if(strcmp(parsed_command_array[index],"exit")==0){
        break;
      }
      if(strcmp(parsed_command_array[index],"-u")==0){
        if(parsed_command_array[index+1] != NULL){
          if(is_instance(parsed_command_array[index+1])!=-1){
            deleted[is_instance(parsed_command_array[index])] = -1;
            if(parsed_command_array[index+2] != NULL){
              if(strcmp(parsed_command_array[index+2],"cat")==0){
                index += 2;
                catarg[cind] = parsed_command_array[index];
                cind++;
                while(strcmp(parsed_command_array[index],"-u") != 0 && parsed_command_array[index+1] != NULL){
                  catarg[cind] = parsed_command_array[index+1];
                  cind++;
                  index++;
                }
                index++;
                cmd = 1;
              }
              else if(strcmp(parsed_command_array[index+2],"head")==0){
                index += 2;
                headarg[hind] = parsed_command_array[index];
                hind++;
                while(strcmp(parsed_command_array[index],"-u") != 0 && parsed_command_array[index+1] != NULL){
                  headarg[hind] = parsed_command_array[index+1];
                  hind++;
                  index++;
                }
                index++;
                cmd = 2;
              }
              else{
                printf("env: ‘%s’: No such file or directory\n",parsed_command_array[index+2]);
                index++;
              }
            }
            else{
              if(flag == -1 || flag == 3 || flag == 4){
                for(int i = 0; envp[i]; i++)
                  if(deleted[i]!=1)
                    printf("%s\n",envp[i]);
              }
              else if(flag == 1){
                FILE *f;
                f = fopen(next_redirect,"r");
                if(f != NULL){
                  for(int i = 0; envp[i]; i++)
                    if(deleted[i]!=1)
                      fprintf(f,"%s\n",envp[i]);
                  fclose(f);
                }
                else printf("File does not exist");
              }
              else if(flag == 2){
                FILE *f;
                f = fopen(next_redirect,"a+");
                for(int i = 0; envp[i]; i++)
                  if(deleted[i]!=1)
                    fprintf(f,"%s\n",envp[i]);
                fclose(f);
              }
            }
          }
          else{
            if(flag == -1 || flag == 3 || flag == 4){
              for(int i = 0; envp[i]; i++)
                printf("%s\n",envp[i]);
            }
            else if(flag == 1){
              FILE *f;
              f = fopen(next_redirect,"r");
              if(f != NULL){
                for(int i = 0; envp[i]; i++)
                  fprintf(f,"%s\n",envp[i]);
                fclose(f);
              }
              else printf("File does not exist");
            }
            else if(flag == 2){
              FILE *f;
              f = fopen(next_redirect,"a+");
              for(int i = 0; envp[i]; i++)
                 fprintf(f,"%s\n",envp[i]);
              fclose(f);
            }
          }
        }
        else{
          printf("env: option requires an argument -- 'u'\nTry 'env --help' for more information.\n");
        }
        index++;
      }   
      else{
        printf("env: ‘%s’: No such file or directory\n",parsed_command_array[index]);
        index++;
      }
      index++;
    }
  }
    if(cind != 0){
      pid_t pid;
      pid = fork();
      if(pid == 0){
        parse_cat_args(catarg);
        exit(0);
      }
      else if(pid < 0)
        perror("Error forking\n");
      else wait(NULL);
    }
    else if(hind != 0){
      pid_t pid;
      pid = fork();
      if(pid == 0){
        parse_head_args(headarg);
        exit(0);
      }
      else if(pid < 0)
        perror("Error forking\n");
      else wait(NULL);
    }
}

int main(int arg, char **argv, char *envp[]){
  char *command = malloc(100);
  char **arguments_parsed, **arguments_piped;
  arguments_parsed = malloc(64*sizeof(char*));
  arguments_piped = malloc(64*sizeof(char*)); 

  do{
    command = readline("> ");
    char *buff = malloc(100);
    strcpy(buff,command);
    if(strlen(command)!=0)add_history(command);
    char *com = malloc(100);
    strcpy(com,command);
    char *com1 = malloc(100);
    strcpy(com1,command);
    char *com2 = malloc(100);
    strcpy(com2,command);
    char *com3 = malloc(100);
    strcpy(com3,command);
    char *com4 = malloc(100);
    strcpy(com4,command);

    arguments_parsed = parse(com);
    
    if(strcmp(arguments_parsed[0],"exit")==0){
      break;
    }
    if(is_pipe(com1)==0){
      
      if(strcmp(arguments_parsed[0], "help") == 0) {
        universal_help();
      }
      else if(strcmp(arguments_parsed[0],"cat") == 0) {
        pid_t pid;
        pid = fork();
        if(pid == 0){
          parse_cat_args(arguments_parsed);
          exit(0);
        }
        else if(pid < 0)
          perror("Error forking\n");
        else wait(NULL);
      }
      else if(strcmp(arguments_parsed[0], "head") == 0) {
        pid_t pid;
        pid = fork();
        if(pid == 0){
          parse_head_args(arguments_parsed);
          exit(0);
        }
        else if(pid < 0)
          perror("Error forking\n");
        else wait(NULL);
      }
      else if(strcmp(arguments_parsed[0], "env") == 0){
        pid_t pid;
        pid = fork();
        if(pid == 0){
          parse_env_args(arguments_parsed,arg,argv,envp);    
          exit(0); 
        }
        else if(pid < 0)
          perror("Error forking\n");
        else wait(NULL);
      }
      else execute(arguments_parsed);
    }
    else if(is_pipe(com2) == 1){
      arguments_piped = parse_pipe(com3); 
      execute_pipes(com4,arguments_piped,arg,argv);
    }
  }while(1);
}
