#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

char *device, *target, *dest;

void printu_exit(char *argv[]);
int parse_opt(int argc, char *argv[]);
void list_tdir(void);
void recover_tpath(void);
short not_long_fn(char *input);
short vaild_fn(int mode);

void printu_exit(char *argv[]){
    printf("Usage: %s -d [device filename] [other arguments]\n", argv[0]);
    printf("-l target           List the target directory\n");
    printf("-r target -o dest   Recover the target pathname\n");
    exit(0);
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

short not_long_fn(char *input, short is_dir){
    char *target;
    strcpy(target, input);
    char *path, *name, *ext;
    path = strtok(target, '.');
    if (ext = strtok(NULL, '.')){
        if (strlen(ext) < 1 || strlen(ext) > 3) return 0;
    }
    if (strlen(targ) 
}

short valid_fn(int mode){
    int len
}

void list_tdir(void){
    printf("Task: to list target directory %s from device %s\n", target, device);
}

void recover_tpath(void){
    printf("Task: to recover destination path %s in target directory %s from device %s\n", dest, target, device);
}

int main(int argc, char *argv[]){
    int mode = parse_opt(argc, argv);
    valid_fn(mode);

    if (mode) recover_tpath();
    else list_tdir();
    return 0;
}
