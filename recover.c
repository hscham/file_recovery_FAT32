#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

struct BootEntry
{
    unsigned char BS_jmpBoot[3];
    unsigned char BS_OEMName[8];
    unsigned short BPB_BytsPerSec __attribute__((packed));
    unsigned char BPB_SecPerClus;
    unsigned short BPB_RsvdSecCnt __attribute__((packed));
    unsigned char BPB_NumFATs;
    unsigned short BPB_RootEntCnt __attribute__((packed));
    unsigned short BPB_TotSec16 __attribute__((packed));
    unsigned char BPB_Media;
    unsigned short BPB_FATSz16 __attribute__((packed)); 
    unsigned short BPB_SecPerTrk __attribute__((packed)); 
    unsigned short BPB_NumHeads __attribute__((packed)); 
    unsigned long BPB_HiddSec __attribute__((packed)); 
    unsigned long BPB_TotSec32 __attribute__((packed)); 
    unsigned long BPB_FATSz32 __attribute__((packed));
    unsigned short BPB_ExtFlags __attribute__((packed));
    unsigned short BPB_FSVer __attribute__((packed));
    unsigned long BPB_RootClus __attribute__((packed));
    unsigned short BPB_FSInfo __attribute__((packed));
    unsigned short BPB_BkBootSec __attribute__((packed));
    unsigned char BPB_Reserved[12];
    unsigned char BS_DrvNum;
    unsigned char BS_Reserved1;
    unsigned char BS_BootSig;
    unsigned long BS_VolID __attribute__((packed));
    unsigned char BS_VolLab[11];
    unsigned char BS_FilSysType[8];
};  

struct DirEntry
{
    unsigned char DIR_Name[11];
    unsigned char DIR_Attr;
    unsigned char DIR_NTRes;
    unsigned char DIR_CrtTimeTenth;
    unsigned short DIR_CrtTime __attribute__((packed));
    unsigned short DIR_CrtDate __attribute__((packed));
    unsigned short DIR_LstAccDate __attribute__((packed));
    unsigned short DIR_FstClusHI __attribute__((packed));
    unsigned short DIR_WrtTime __attribute__((packed));
    unsigned short DIR_WrtDate __attribute__((packed));
    unsigned short DIR_FstClusLO __attribute__((packed));
    unsigned long DIR_FileSize __attribute__((packed));
};

char *device, *target, *dest;
int fd, err;
struct BootEntry boot_entry;
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
void init_boot_entry(void);
int read_sec(int sec_num, unsigned char *buf, int num_sec);

void printu_exit(char *argv[]){
    printf("Usage: %s -d [device filename] [other arguments]\n", argv[0]);
    printf("-l target           List the target directory\n");
    printf("-r target -o dest   Recover the target pathname\n");
    exit(1);
}

short is_lfn(char *input){
    char target[strlen(input)];
    strcpy(target, input);
    char *name_e, *name, *ext, *tmp;
    if (strlen(target) == 1 && target[0] == '/'){
        name = target;
        return 0;
    }
    name_e = strtok(input, "/");
    while (tmp = strtok(NULL, "/"))
        name_e = tmp;
    //Check target does not start with '.'
    if (name_e[0] == '.') return 1;
    //Check target contains no illegal characters
    int i;
    short dotflag = 0;
    for (i = 0; i < strlen(name_e); i++){
        char c = name_e[i];
        if ((c == 33) || (c > 34 && c < 42) || c == 45 || (c > 64 && c < 91) || c == 94 || c ==95 || c == 123 || c == 125 || c == 126) ;
        else if (c == 46 && !dotflag) dotflag = 1; 
        else if (c == 46) return 2; 
        else return 3;
    }
    //Check target name (without extension) length
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

void init_boot_entry() {
    char tmp[100];
    lseek(fd, 0, SEEK_SET);
    if(read(fd, tmp, 100) == -1)
	perror("read");
    memcpy(&boot_entry, tmp, sizeof(struct BootEntry));
    /*
      printf("%d\n", boot_entry.BPB_BytsPerSec);
      printf("%d\n", boot_entry.BPB_SecPerClus);
      printf("%d\n", boot_entry.BPB_RsvdSecCnt);
      printf("%d\n", boot_entry.BPB_NumFATs);
      printf("Tot16 %d\n", boot_entry.BPB_TotSec16);
      printf("%d\n", boot_entry.BPB_TotSec32);
      printf("FAT size 32 %d\n", boot_entry.BPB_FATSz32);
      printf("%d\n", boot_entry.BPB_RootClus);
      printf("size is %d\n", sizeof(struct BootEntry));
    */
}

int read_sec(int sec_num, unsigned char *buf, int num_sec) {
    int len;
    lseek(fd, sec_num*boot_entry.BPB_BytsPerSec, SEEK_SET);
    if ((len = read(fd, buf, num_sec*boot_entry.BPB_BytsPerSec)) == -1)
	perror("read");
    return len;
}

void list_tdir(void){
    int data_sec = boot_entry.BPB_RsvdSecCnt + boot_entry.BPB_NumFATs * boot_entry.BPB_FATSz32, i = 0;
    int root_dir_sec = data_sec + (boot_entry.BPB_RootClus-2) * boot_entry.BPB_BytsPerSec;
    unsigned char tmp[boot_entry.BPB_SecPerClus * boot_entry.BPB_BytsPerSec];
    struct DirEntry dir_entry;

    read_sec(root_dir_sec, tmp, boot_entry.BPB_SecPerClus);
    memcpy(&dir_entry, tmp, sizeof(struct DirEntry));

    while ((char)dir_entry.DIR_Name[0] != 0) {
	int j = 0;
	printf("%d, ", ++i);
	
	/* print filename */
	if (dir_entry.DIR_Name[0] == (char)0xE5) {
	    putchar('?');
	    j = 1;
	}

	for (; j<8; j++) {
	    if (dir_entry.DIR_Name[j] == ' ')
		continue;
	    else
		putchar(dir_entry.DIR_Name[j]);
	}

	if (dir_entry.DIR_Attr == 16)
	    putchar('/');
	else {
	    putchar('.');
	    for (; j<11; j++) {
		if (dir_entry.DIR_Name[j] == ' ')
		    continue;
		else
		    putchar(dir_entry.DIR_Name[j]);
	    }
	}

        /* print file size */
        printf(", %ld", dir_entry.DIR_FileSize);
        /* print starting cluster */
        printf(", %d\n", dir_entry.DIR_FstClusLO);
	
	memcpy(&dir_entry, tmp + i*sizeof(struct DirEntry), sizeof(struct DirEntry));
    }
}

void recover_tpath(void){
    printf("Task: to recover destination path %s in target directory %s from device %s\n", dest, target, device);
}

int main(int argc, char *argv[]){
    int mode = parse_opt(argc, argv);
    if((fd = open(device, O_RDWR)) == -1)
      perror("open");

    /* read boot sector */
    init_boot_entry();

    if (mode) recover_tpath();
    else list_tdir();

    close(fd);
    return 0;
}
