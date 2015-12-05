#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INPUT_MAX 1024
#define FN_LEN 11

struct BootEntry {
    unsigned char BS_jmpBoot[3];
    unsigned char BS_OEMName[8];
    unsigned short BPB_BytsPerSec;
    unsigned char BPB_SecPerClus;
    unsigned short BPB_RsvdSecCnt;
    unsigned char BPB_NumFATs;
    unsigned short BPB_RootEntCnt;
    unsigned short BPB_TotSec16;
    unsigned char BPB_Media;
    unsigned short BPB_FATSz16;
    unsigned short BPB_SecPerTrk;
    unsigned short BPB_NumHeads;
    unsigned long BPB_HiddSec;
    unsigned long BPB_TotSec32;
    unsigned long BPB_FATSz32;
    unsigned short BPB_ExtFlags;
    unsigned short BPB_FSVer;
    unsigned long BPB_RootClus;
    unsigned short BPB_FSInfo;
    unsigned short BPB_BkBootSec;
    unsigned char BPB_Reserved[12];
    unsigned char BS_DrvNum;
    unsigned char BS_Reserved1;
    unsigned char BS_BootSig;
    unsigned long BS_VolID;
    unsigned char BS_VolLab[11];
    unsigned char BS_FilSysType[8];
}__attribute__((packed));

struct DirEntry {
    unsigned char DIR_Name[11];
    unsigned char DIR_Attr;
    unsigned char DIR_NTRes;
    unsigned char DIR_CrtTimeTenth;
    unsigned short DIR_CrtTime;
    unsigned short DIR_CrtDate;
    unsigned short DIR_LstAccDate;
    unsigned short DIR_FstClusHI;
    unsigned short DIR_WrtTime;
    unsigned short DIR_WrtDate;
    unsigned short DIR_FstClusLO;
    unsigned long DIR_FileSize;
}__attribute__((packed));

//Global variables for input (fixed)
char *device, *target, *dest; //userinput
char *p_target; //copy of target
char *tg_list_ptr[INPUT_MAX/2]; //strtok tokens pointed to p_target
char tg_list[FN_LEN + 1][INPUT_MAX / 2]; //8.3 ver of dir entry names
int tg_height = 0;
char is_recover;
//Global variables for boot sector and directory entry
unsigned long fat_sec[2], data_sec, clus_size;
int dir_entry_size = sizeof(struct DirEntry);
int boot_entry_size = sizeof(struct BootEntry);
struct BootEntry boot_entry;
struct DirEntry recover_dir_entry;
//Global variables for error msg
int fd;
char err;
const char *lfn_err_msg[10] = { 
    "Target name should not start with dot '.'",
    "Target name should not contain more than 1 dot '.'",
    "Target name should only contain upper case alphabets, digits, or special symbols $ % ’ ‘ - { } ~ ! # ( ) & ∧",
    "Target name should be in length between 1 to 8",
    "File extension should be in length between 1 to 3",
};

//Functions for debug
void print_dir_tree(void);
void print_boot_entry(void);
//Functions for error msg
void printu_exit(char *argv[]);
void print_lfn_errmsg(int err);
//Functions for parsing
int parse_opt(int argc, char* argv[]);
void parse_target(void);
char is_lfn(void);
void process_dirname(void);
//Functions for disk
void init_boot_entry(void);
unsigned short find_clus(void);
int read_sec(int sec_num, unsigned char *buf, int num_sec);
int write_sec(int sec_num, unsigned char *buf, int num_sec);
//Main functions
void list_tdir(void);
void recover_tpath(void);

void print_dir_tree(void){
    printf("--------------------\n");
    printf("| Directory tree:\n");
    int i;
    printf("| tg_height = %d\n", tg_height);
    for (i = 0; i < tg_height; i++)
        printf("| i = %d: %s\n", i, tg_list_ptr[i]);
    printf("--------------------\n");
}

void print_boot_entry(void){
    printf("--------------------\n");
    printf("| Boot entry:\n");
    printf("| Byte per sec: %d\n", boot_entry.BPB_BytsPerSec);
    printf("| Sec per clus: %c\n", boot_entry.BPB_SecPerClus);
    printf("| Reserved sec count: %d\n", boot_entry.BPB_RsvdSecCnt);
    printf("| Num of FATs: %c\n", boot_entry.BPB_NumFATs);
    printf("| Total sec(16): %d\n", boot_entry.BPB_TotSec16);
    printf("| Total sec(32): %ld\n", boot_entry.BPB_TotSec32);
    printf("| FAT size (32): %ld\n", boot_entry.BPB_FATSz32);
    printf("| Clus of root dir: %ld\n", boot_entry.BPB_RootClus);
    printf("| Size of boot entry: %d\n", sizeof(struct BootEntry));
    printf("--------------------\n");
}

void printu_exit(char *argv[]){
    printf("Usage: %s -d [device filename] [other arguments]\n", argv[0]);
    printf("-l target           List the target directory\n");
    printf("-r target -o dest   Recover the target pathname\n");
    exit(1);
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
            parse_target();
            if (err = is_lfn()){
                print_lfn_errmsg(err);
                exit(1);
            }
            if(tg_height) process_dirname();
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

void parse_target(void){
    p_target = malloc(sizeof(strlen(target)));
    strcpy(p_target, target);
    tg_list_ptr[tg_height] = strtok(p_target, "/");
    if (!tg_list_ptr[tg_height])
        return;
    tg_height++;
    while(tg_list_ptr[tg_height] = strtok(NULL, "/"))
        tg_height++;
}

char is_lfn(void){
    if (!tg_height)
        return 0;
    char c, *name_e = tg_list_ptr[tg_height-1];
    //Check lfn_target contains no illegal characters
    int i;
    short dotflag = 0;
    for (i = 0; i < strlen(name_e); i++){
        c = name_e[i];
        if ((c == 33) || (c > 34 && c < 42) || c == 45 || (c > 47 && c < 58) || (c > 64 && c < 91) || c == 94 || c ==95 || c == 123 || c == 125 || c == 126) ;
        else if (c == 46 && !dotflag) dotflag = 1; 
        else if (c == 46) return 2; 
        else return 3;
    }
    //Check lfn_target name (without extension) length
    char name[FN_LEN + 1];
    //char *name = malloc(sizeof(FN_LEN + 1));
    strcpy(name, name_e);
    char *noext, *ext;
    noext = strtok(name, ".");
    if (strlen(name) < 1 || strlen(name) > 8) return 4;
    //Check extension length
    if (ext = strtok(NULL, ".")){
        if (strlen(ext) < 1 || strlen(ext) > 3) return 5;
    }
    return 0;
}

void process_dirname(void){
    int i, j;

    for (i = 0; i < tg_height-1; i++){
        memcpy(tg_list[i], tg_list_ptr[i], strlen(tg_list_ptr[i]));
        for (j = strlen(tg_list_ptr[i]); j < FN_LEN; j++)
            tg_list[i][j] = ' ';
        tg_list[i][FN_LEN+1] = '\0';
    }

    char name_e[FN_LEN + 1];
    char *name, *ext;
    strcpy(name_e, tg_list_ptr[tg_height-1]);
    name = strtok(name_e, ".");
    memcpy(tg_list[tg_height-1], name, strlen(name));
    for (j = strlen(name); j < 8; j++)
        tg_list[tg_height-1][j] = ' ';
    if (ext = strtok(NULL, ".")){
        memcpy(tg_list[tg_height-1] + 8, ext, strlen(ext));
        for (j = 8 + strlen(ext); j < FN_LEN; j++)
            tg_list[tg_height-1][j] = ' ';
    } else {
        for (j = 8; j < FN_LEN; j++)
            tg_list[tg_height-1][j] = ' ';
    }
    
    tg_list[tg_height-1][FN_LEN] = '\0';
}

void init_boot_entry() {
    char tmp[boot_entry_size];
    lseek(fd, 0, SEEK_SET);
    if(read(fd, tmp, 100) == -1) perror("read");
    memcpy(&boot_entry, tmp, boot_entry_size);
    fat_sec[0] = boot_entry.BPB_RsvdSecCnt;
    fat_sec[1] = boot_entry.BPB_RsvdSecCnt + boot_entry.BPB_FATSz32;
    data_sec = boot_entry.BPB_RsvdSecCnt + boot_entry.BPB_NumFATs * boot_entry.BPB_FATSz32;
    clus_size = boot_entry.BPB_SecPerClus * boot_entry.BPB_BytsPerSec;
}

unsigned short find_clus(void){
    /* For each (sub)directory in the path (loop i through 0 to num of layers),
        search through all clusters of that (sub)dir until next (sub)dir is found.
        Find next (sub)dir by looping through all dir entries in that cluster.  */

    unsigned short clus = boot_entry.BPB_RootClus;
    int i, j;
    char found = 1;
    unsigned char tmp[clus_size];
    struct DirEntry dir_entry;

    for (i = 0; i < tg_height; i++){
        found = 0;

        while (clus < 0xFFF8){
            j = 0;
            read_sec(data_sec + (clus - 2) * boot_entry.BPB_SecPerClus, tmp, boot_entry.BPB_SecPerClus);
            memcpy(&dir_entry, tmp, dir_entry_size);

            while((char)dir_entry.DIR_Name[0] != 0 && j < (boot_entry.BPB_BytsPerSec / 32)){
                if ((i < tg_height - 1 || !is_recover) && (dir_entry.DIR_Attr % 32) >= 16){
                    if (!strncmp(tg_list[i], dir_entry.DIR_Name, FN_LEN)){
                        found = 1;
                        break;
                    }
                } else{
                    if (dir_entry.DIR_Name[0] == 0xE5 && !strncmp(&tg_list[i][1], &dir_entry.DIR_Name[1], FN_LEN-1)){
                        found = 1;
                        break;
                    }
                }
                memcpy(&dir_entry, tmp + ++j * dir_entry_size, dir_entry_size);
            }

            /* If next subdir is not found, read data of next clus of current dir
               O/w, go to clus of that entry */
            if (!found){
                lseek(fd, fat_sec[0] * boot_entry.BPB_BytsPerSec + clus * 4, SEEK_SET);
                read(fd, &clus, 4);
            } else {
                clus = dir_entry.DIR_FstClusLO;
                break;
            }
        }
    }

    memcpy(&recover_dir_entry, &dir_entry, dir_entry_size);
    if (clus == 0) clus = 1;
    return (is_recover && !found)? 0 : clus;
}

int read_sec(int sec_num, unsigned char *buf, int num_sec){
    int len;
    lseek(fd, sec_num * boot_entry.BPB_BytsPerSec, SEEK_SET);
    if ((len = read(fd, buf, num_sec * boot_entry.BPB_BytsPerSec)) == -1)
        perror("read");
    return len;
}

int write_sec(int sec_num, unsigned char *buf, int num_sec){
    int len;
    lseek(fd, sec_num * boot_entry.BPB_BytsPerSec, SEEK_SET);
    if ((len = write(fd, buf, num_sec * boot_entry.BPB_BytsPerSec)) == -1)
        perror("write");
    return len;
}

void list_tdir(void){
    static int num = 0;
    unsigned char tmp[clus_size];
    unsigned short tg_clus = find_clus();
    struct DirEntry dir_entry;

    while (tg_clus < 0xFFF8){
        int i = 0;
        read_sec(data_sec + (tg_clus-2) * boot_entry.BPB_SecPerClus, tmp, boot_entry.BPB_SecPerClus);
        memcpy(&dir_entry, tmp, dir_entry_size);
        while ((char)dir_entry.DIR_Name[0] != 0 && i < (boot_entry.BPB_BytsPerSec / dir_entry_size)) {
            if (dir_entry.DIR_Attr % 16 == 15){
                memcpy(&dir_entry, tmp + ++i * dir_entry_size, dir_entry_size);
                continue;
            }
            char rm_flag = 0;
            int j = 0;
            printf("%d, ", ++num);

            if (dir_entry.DIR_Name[0] == 0xE5){
                putchar('?');
                j = 1;
            }
            for (; j < 8; j++){
                if (dir_entry.DIR_Name[j] == ' ') continue;
                else putchar(dir_entry.DIR_Name[j]);
            }

            if ((dir_entry.DIR_Attr%32) >= 16) putchar('/');
            else if (dir_entry.DIR_Name[j] == ' ') ;
            else {
                putchar('.');
                for (; j < 11; j++){
                    if (dir_entry.DIR_Name[j] == ' ') continue;
                    else putchar(dir_entry.DIR_Name[j]);
                }
            }

            printf(", %ld", dir_entry.DIR_FileSize);
            printf(", %d\n", dir_entry.DIR_FstClusLO);

            memcpy(&dir_entry, tmp + ++i * dir_entry_size, dir_entry_size);
        }
        
        lseek(fd, fat_sec[0] * boot_entry.BPB_BytsPerSec + tg_clus * 4, SEEK_SET);
        read(fd, &tg_clus, 4);
    }
}

void recover_tpath(void){
    unsigned char tmp[clus_size];
    unsigned short tg_clus = find_clus();
    struct DirEntry dir_entry;
    int output_fd;

    if (!tg_clus){
        printf("%s:  error - file not found\n", target);
        return;
    } else if (tg_clus == 1){
        if ((output_fd = open(dest, O_CREAT|O_WRONLY|O_TRUNC, 0640)) == -1)
	    printf("%s: failed to open\n", target);
        close(output_fd);
        printf("%s:  recovered\n", target);
        return;
    }

    unsigned long FAT[boot_entry.BPB_FATSz32 * boot_entry.BPB_BytsPerSec];
    read_sec(fat_sec[0], (unsigned char *)FAT, boot_entry.BPB_FATSz32);
    if (FAT[tg_clus]){
        printf("%s:  error - fail to recover\n", target);
        return;
    }

    memcpy(&dir_entry, &recover_dir_entry, dir_entry_size);
    
    unsigned long file_size = dir_entry.DIR_FileSize;
    unsigned char content[file_size];
    read_sec(data_sec + (tg_clus-2) * boot_entry.BPB_SecPerClus, content, file_size);

    if ((output_fd = open(dest, O_CREAT|O_WRONLY|O_TRUNC, 0640)) == -1)
	printf("%s: failed to open\n", target);
    if (write(output_fd, content, file_size) == -1)
        perror("write");
    close(output_fd);
    printf("%s: recovered\n", target);
}

int main(int argc, char *argv[]){
    is_recover = parse_opt(argc, argv);

    if ((fd = open(device, O_RDONLY)) == -1)
        perror("open");
    init_boot_entry();

    if (is_recover) recover_tpath();
    else list_tdir();

    close(fd);
    return 0;
}
