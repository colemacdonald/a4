#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "disk.h"

/************************ STRUCT *******************************/

typedef struct diskimage diskimage_t;
struct diskimage {
    char * imagename;
    superblock_entry_t * sb;
    FILE * f;
} __attribute__ ((packed));

/************************* FUNCTION PROTOTYPES ****************************/

void                pack_current_datetime(unsigned char *entry);
int                 next_free_block(int *FAT, int max_blocks);
void                sseek(FILE * f, int len, int origin);
void                sread(void * buffer, size_t sizeofelements, size_t num, FILE *f);
void                convertToNet(superblock_entry_t * sb);
unsigned int        findFileOnDisk(diskimage_t * image, char * filename);
void                catFile(diskimage_t * image, unsigned int start_block);
unsigned long int   read_fat_entry(diskimage_t * image, unsigned long int entry);
void                convertToNetDE(directory_entry_t * de);

/************************* FUNCTION IMPLEMENTATIONS *************************/

/*
 * Based on http://bit.ly/2vniWNb
 */
void pack_current_datetime(unsigned char *entry) {
    assert(entry);

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    unsigned short year   = tm.tm_year + 1900;
    unsigned char  month  = (unsigned char)(tm.tm_mon + 1);
    unsigned char  day    = (unsigned char)(tm.tm_mday);
    unsigned char  hour   = (unsigned char)(tm.tm_hour);
    unsigned char  minute = (unsigned char)(tm.tm_min);
    unsigned char  second = (unsigned char)(tm.tm_sec);

    year = htons(year);

    memcpy(entry, &year, 2);
    entry[2] = month;
    entry[3] = day;
    entry[4] = hour;
    entry[5] = minute;
    entry[6] = second; 
}

/*
 * Returns next available block
 */
int next_free_block(int *FAT, int max_blocks) {
    assert(FAT != NULL);

    int i;

    for (i = 0; i < max_blocks; i++) {
        if (FAT[i] == FAT_AVAILABLE) {
            return i;
        }
    }

    return -1;
}

/*************************** MAIN ***************************/

int main(int argc, char *argv[]) {
    superblock_entry_t sb;
    int  i;
    char *imagename  = NULL;
    char *filename   = NULL;
    char *sourcename = NULL;
    FILE * src_file;

    diskimage_t image;
    image.sb = &sb;

/******************* ZASTRE ***********************/

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "--file") == 0 && i+1 < argc) {
            filename = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "--source") == 0 && i+1 < argc) {
            sourcename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL || filename == NULL || sourcename == NULL) {
        fprintf(stderr, "usage: storuvfs --image <imagename> " \
            "--file <filename in image> " \
            "--source <filename on host>\n");
        exit(1);
    }

/********************* END Z **********************/

    if( (image.f = fopen(imagename, "wb+")) == NULL )
    {
        fprintf(stderr, "Specified image could not be opened.\n");
        exit(1);
    }

    if( (src_file = fopen(sourcename, "rb+")) == NULL )
    {
        fprintf(stderr, "Specified source file could not be found.\n");
    }

    image.imagename = imagename;
    sseek(image.f, 0L, SEEK_SET);
    // read in super block
    sread(image.sb, sizeof(superblock_entry_t), 1, image.f);
    // validate image
    if(strncmp(image.sb->magic, FILE_SYSTEM_ID, 8) != 0)
    {
        fprintf(stderr, "Image did not match expected format.\n");
        exit(1);
    }

    convertToNet(image.sb);


    return 0; 
}
