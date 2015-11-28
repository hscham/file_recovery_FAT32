#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

char *device, *target, *dest;
int err;
const char *lfn_err_msg[10] = { 
    "Target name should not start with dot '.'",
    "Target name should not contain more than 1 dot '.'",
    "Target name should only contain upper case alphabets, digits, or special symbols $ % ’ ‘ - { } ~ ! # ( ) & ∧",
    "Target name should be in length between 1 to 8",
    "File extension should be in length between 1 to 3",
    };

void printu_exit(char *argv[]);
short is_long_fn(char *input);
void print_lfn_errmsg(int err);
int parse_opt(int argc, char *argv[]);
void list_tdir(void);
void recover_tpath(void);

void printu_exit(char *argv[]){
    printf("Usage: %s -d [device filename] [other arguments]\n", argv[0]);
    printf("-l target           List the target directory\n");
    printf("-r target -o dest   Recover the target pathname\n");
    exit(1);
}

short is_lfn(char *input){
    char input_t[strlen(input)];
    strcpy(input_t, input);
    char *name_e, *name, *ext, *tmp;
    if (strlen(input_t) == 1 && input_t[0] == '/'){
        name = input_t;
        return 0;
    }
    name_e = strtok(input, "/");
    while (tmp = strtok(NULL, "/"))
        name_e = tmp;
    //Check input_t does not start with '.'
    if (name_e[0] == '.') return 1;
    //Check input_t contains no illegal characters
    int i;
    short dotflag = 0;
    for (i = 0; i < strlen(name_e); i++){
        char c = name_e[i];
        if ((c == 33) || (c > 34 && c < 42) || c == 45 || (c > 64 && c < 91) || c == 94 || c ==95 || c == 123 || c == 125 || c == 126) ;
        else if (c == 46 && !dotflag) dotflag = 1; 
        else if (c == 46) return 2; 
        else return 3;
    }
    //Check input_t name (without extension) length
    name = strtok(name_e, ".");
    if (strlen(name) < 1 || strlen(name) > 8) return 4;
    if (ext = strtok(NULL, ".")){
        //Check extension length
        if (strlen(ext) < 1 || strlen(ext) > 3) return 5;
    }
    return 0;
}

void print_lfn_errmsg(int err){
    printf("Long filenames not supported. %s\n", lfn_err_msg[err-1]);
}

int parse_opt(int argc, char *argv[]){
    int opt = 0, state = 0;
    while((opt = getopt(argc, argv, "d:l:r:o:")) != -1){
        if (state == 0){
            if (opt != 'd') printu_exit(argv);
            device = optarg;
            state = 1;
        } else if (state == 1){
            if (opt == 'l') state = 2;
            else if (opt == 'r') state = 3;
            else printu_exit(argv);
            target = optarg;
            if (err = is_lfn(target)){
                print_lfn_errmsg(err);
                exit(1);
            }
        } else if (state == 3){
            if (opt != 'o') printu_exit(argv);
            dest = optarg;
            state = 4;
        } else
            printu_exit(argv);
    } 
    if (state != 2 && state != 4) printu_exit(argv);
    return (state - 2);
}

void list_tdir(void){
    printf("Task: to list target directory %s from device %s\n", target, device);
}

void recover_tpath(void){
    printf("Task: to recover destination path %s in target directory %s from device %s\n", dest, target, device);
}

int main(int argc, char *argv[]){
    int mode = parse_opt(argc, argv);

    if (mode) recover_tpath();
    else list_tdir();
    return 0;
}
