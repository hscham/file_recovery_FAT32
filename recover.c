#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#define INPUT_MAX 1024
#define FN_LEN 11

struct BootEntry
{
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

struct DirEntry
{
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

char *device, *target, *p_target, *dest, *tg_list_ptr[INPUT_MAX / 2], tg_list[FN_LEN + 1][INPUT_MAX / 2];
int fd, err, fat_sec[2], data_sec, clus_size, tg_height = 0; //tg_height of root = 0; +1 for each subdirectory layer
short is_recover;
struct BootEntry boot_entry;
const char *lfn_err_msg[10] = { 
    "Target name should not start with dot '.'",
    "Target name should not contain more than 1 dot '.'",
    "Target name should only contain upper case alphabets, digits, or special symbols $ % ’ ‘ - { } ~ ! # ( ) & ∧",
    "Target name should be in length between 1 to 8",
    "File extension should be in length between 1 to 3",
    };

void printu_exit(char *argv[]);
void parse_target(void);
short is_lfn(void);
void print_lfn_errmsg(int err);
int parse_opt(int argc, char *argv[]);
void process_dirname(void);
void list_tdir(void);
void recover_tpath(void);
void init_boot_entry(void);
int read_sec(int sec_num, unsigned char *buf, int num_sec);
int write_sec(int sec_num, unsigned char *buf, int num_sec);
unsigned short find_clus(void);

void print_dir_tree(void){
    int i;
    printf("tg_height = %d\n", tg_height);
    for (i = 0; i < tg_height; i++)
        printf("i = %d: %s\n", i, tg_list_ptr[i]);
}

void printu_exit(char *argv[]){
    printf("Usage: %s -d [device filename] [other arguments]\n", argv[0]);
    printf("-l target           List the target directory\n");
    printf("-r target -o dest   Recover the target pathname\n");
    exit(1);
}

void parse_target(void){
    p_target = malloc(sizeof(strlen(target)));
    strcpy(p_target, target);
    tg_list_ptr[tg_height] = strtok(p_target, "/");
    if (!tg_list_ptr[tg_height])
        return;
    tg_height++;
    while(tg_list_ptr[tg_height] = strtok(NULL, "/")){
        tg_height++;
    }
    print_dir_tree();
}

short is_lfn(void){
    if (!tg_height)
        return 0;
    char c, *name_e = tg_list_ptr[tg_height-1];
    printf("name_e: %s\n", name_e);
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
    if (strlen(name) < 1 || strlen(name) > 8) return 4;
    //Check extension length
    char *ext;
    if (ext = strtok(NULL, ".")){
        if (strlen(ext) < 1 || strlen(ext) > 3) return 5;
    }
    return 0;
}

void print_lfn_errmsg(int err){
    printf("Long filenames not supported. %s\n", lfn_err_msg[err-1]);
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

    for (i = 0; i < tg_height; i++)
        printf("processed dir name at %d: %s(end)\n", i, tg_list[i]);
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
	    if(tg_height)
		process_dirname();
        } else if (state == 3){
            if (opt != 'o') printu_exit(argv);
            dest = optarg;
            state = 4;
        } else
            printu_exit(argv);
    } 
    if (state != 2 && state != 4) printu_exit(argv);
    printf("leaving parse_opt, device = %s, target = %s\n", device, target);
    return (state - 2);
}

void init_boot_entry() {
    char tmp[100];
    lseek(fd, 0, SEEK_SET);
    if(read(fd, tmp, 100) == -1)
	perror("read");
    memcpy(&boot_entry, tmp, sizeof(struct BootEntry));
    fat_sec[0] = boot_entry.BPB_RsvdSecCnt;
    fat_sec[1] = boot_entry.BPB_RsvdSecCnt + boot_entry.BPB_FATSz32;
    data_sec = boot_entry.BPB_RsvdSecCnt + boot_entry.BPB_NumFATs * boot_entry.BPB_FATSz32;
    clus_size = boot_entry.BPB_SecPerClus * boot_entry.BPB_BytsPerSec;
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

int write_sec(int sec_num, unsigned char *buf, int num_sec) {
    int len;
    lseek(fd, sec_num*boot_entry.BPB_BytsPerSec, SEEK_SET);
    if ((len = write(fd, buf, num_sec*boot_entry.BPB_BytsPerSec)) == -1)
	perror("write");
    return len;
}

unsigned short find_clus(void) {
    unsigned short clus = boot_entry.BPB_RootClus, pre_clus;
    int i, j, found;
    unsigned char tmp[clus_size];
    struct DirEntry dir_entry;

    for (i=0; i<tg_height; i++) {
	char *tg = malloc(sizeof(char) * FN_LEN);
	printf("target is: %s\n", tg_list[i]);
	found = 0;

	memcpy(tg, tg_list[i], FN_LEN);
	if (is_recover && i == (tg_height-1))  /* if reach the filename */
	    *(tg) = (char)0xE5;

	while (clus != 0xFFFF) {
	    j = 0;
	    pre_clus = clus;
	    read_sec(data_sec + (clus-2) * boot_entry.BPB_SecPerClus, tmp, boot_entry.BPB_SecPerClus);
	    memcpy(&dir_entry, tmp, sizeof(struct DirEntry));
	    while((char)dir_entry.DIR_Name[0] != 0 && j < (boot_entry.BPB_BytsPerSec/32)) {
		//		printf("%llu\n", tmp + i*sizeof(struct DirEntry));
		if (!strncmp(tg, dir_entry.DIR_Name, FN_LEN)) { /* find the subdir */
		    if ((i != (tg_height-1) && dir_entry.DIR_Attr == 16) || i == (tg_height-1)) {
			found = 1;
			printf("Target found!!!\n");
			break;
		    }
		}
		memcpy(&dir_entry, tmp + ++j * sizeof(struct DirEntry), sizeof(struct DirEntry));
	    }

	    /* find next cluster if not yet found */
	    if (!found) {
		lseek(fd, fat_sec[0] * boot_entry.BPB_BytsPerSec + clus * 4, SEEK_SET);
		read(fd, &clus, 4);
	    } else {
		clus = dir_entry.DIR_FstClusLO;
		printf("Now the clus is %d\n", clus);
		break;
	    }
	}
    }

    if (is_recover) {
	if (found) {
	    // TODO: separate this part to recover
	    /* update DIR_Name[0] to original name */
	    dir_entry.DIR_Name[0] = (unsigned char)tg_list[tg_height-1][0];
	    memcpy(tmp + j * sizeof(struct DirEntry), &dir_entry, sizeof(struct DirEntry));
	    write_sec(data_sec + (pre_clus-2) * boot_entry.BPB_SecPerClus, tmp, boot_entry.BPB_SecPerClus);
	    // printf("pre_clus is %d, j is %d\n", pre_clus, j);
	    printf("Recover DIR_Name\n");
	}
	else
	    printf("%s:  error - file not found\n", target);
    }

    return clus;
}

void list_tdir(void){
    static int num = 0;
    //    int root_dir_sec = data_sec + (boot_entry.BPB_RootClus-2) * boot_entry.BPB_SecPerClus;
    unsigned char tmp[clus_size];
    unsigned short clus = find_clus();
    struct DirEntry dir_entry;

    while (clus != 0xFFFF) {
	int i = 0;
	read_sec(data_sec + (clus-2) * boot_entry.BPB_SecPerClus, tmp, boot_entry.BPB_SecPerClus);
	memcpy(&dir_entry, tmp, sizeof(struct DirEntry));
	while ((char)dir_entry.DIR_Name[0] != 0 && i < (boot_entry.BPB_BytsPerSec/32)) { /* number of 32-bit dir_entry per sec */
	    //	    printf("clus is %d, i is %d\n", clus, i);
	    //	    printf("%llu\n", tmp + i*sizeof(struct DirEntry));
	    int j = 0;
	    printf("%d, ", ++num);

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
	
	    memcpy(&dir_entry, tmp + ++i * sizeof(struct DirEntry), sizeof(struct DirEntry));
	}

	/* find next cluster */
	lseek(fd, fat_sec[0] * boot_entry.BPB_BytsPerSec + clus * 4, SEEK_SET);
	read(fd, &clus, 4);
	printf("next clus %d\n", clus);
	//	read_sec(data_sec+(clus-2), tmp, boot_entry.BPB_SecPerClus);
    }
}

void recover_tpath(void){
    printf("Task: to recover destination path %s in target directory %s from device %s\n", dest, target, device);
    printf("target: %s\n", target);

    unsigned short clus = find_clus();
    unsigned long FAT[boot_entry.BPB_FATSz32 * boot_entry.BPB_BytsPerSec];

    /* read FAT */
    read_sec(fat_sec[0], (unsigned char *)FAT, boot_entry.BPB_FATSz32);
    printf("FAT number now is: %lx\n", (unsigned long)FAT[clus]);

    if (FAT[clus] == 0) {
	unsigned char content[clus_size];

	/* recover FAT */
	FAT[clus] = 0xfffffff;
	write_sec(fat_sec[0], (unsigned char *)FAT, boot_entry.BPB_FATSz32);
	write_sec(fat_sec[1], (unsigned char *)FAT, boot_entry.BPB_FATSz32);
	printf("Recover FAT entry\n");

	/* cpy content */
	read_sec(data_sec + (clus-2) * boot_entry.BPB_SecPerClus, content, boot_entry.BPB_SecPerClus);
	printf("%s\n", content);

	/* copy content to output.txt */
	int output_fd;
	if ((output_fd = open(dest, O_CREAT|O_WRONLY|O_TRUNC, 0640)) == -1)
	    perror("open");

	if (write(output_fd, content, clus_size) == -1 )
	    perror("write");

	close(output_fd);
	
	printf("%s:  recovered\n", target);
    } else 
	printf("%s:  error - fail to recover\n", target);
}

int main(int argc, char *argv[]){
    is_recover = parse_opt(argc, argv);
    if((fd = open(device, O_RDWR)) == -1)
        perror("open");

    /* read boot sector */
    init_boot_entry();

    if (is_recover) recover_tpath();
    else list_tdir();

    close(fd);
    return 0;
}
