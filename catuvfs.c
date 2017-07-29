#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include "disk.h"

/************************ STRUCT *******************************/

typedef struct diskimage diskimage_t;
struct diskimage {
    char * imagename;
    superblock_entry_t * sb;
    FILE * f;
} __attribute__ ((packed));

/************************* FUNCTION PROTOTYPES ****************************/

char *              month_to_string(short m);
void                unpack_datetime(unsigned char *time, short *year, short *month, short *day, short *hour, short *minute, short *second);
void                sseek(FILE * f, int len, int origin);
void                sread(void * buffer, size_t sizeofelements, size_t num, FILE *f);
void                convertToNet(superblock_entry_t * sb);
unsigned int        findFileOnDisk(diskimage_t * image, char * filename);
void                catFile(diskimage_t * image, unsigned int start_block);
unsigned long int   read_fat_entry(diskimage_t * image, unsigned long int entry);
void                convertToNetDE(directory_entry_t * de);

/************************* FUNCTION IMPLEMENTATIONS *************************/

char *month_to_string(short m) {
    switch(m) {
    case 1: return "Jan";
    case 2: return "Feb";
    case 3: return "Mar";
    case 4: return "Apr";
    case 5: return "May";
    case 6: return "Jun";
    case 7: return "Jul";
    case 8: return "Aug";
    case 9: return "Sep";
    case 10: return "Oct";
    case 11: return "Nov";
    case 12: return "Dec";
    default: return "?!?";
    }
}

void unpack_datetime(unsigned char *time, short *year, short *month, 
    short *day, short *hour, short *minute, short *second)
{
    assert(time != NULL);

    memcpy(year, time, 2);
    *year = htons(*year);

    *month = (unsigned short)(time[2]);
    *day = (unsigned short)(time[3]);
    *hour = (unsigned short)(time[4]);
    *minute = (unsigned short)(time[5]);
    *second = (unsigned short)(time[6]);
}

/*
 * Safe seek
 * Runs fseek with input params and handles errors
 */
void sseek(FILE * f, int len, int origin)
{
    if(0 != fseek(f, len, origin))
    {
        fprintf(stderr, "Seek failed.\n");
        exit(1);
    }
}
/*
 * Safe read
 * Runs fread with input params and handles error
 */
void sread(void * buffer, size_t sizeofelements, size_t num, FILE *f)
{
    if( fread(buffer, sizeofelements, num, f) != num )
    {
        fprintf(stderr, "Read failed.\n");
        exit(1);
    }
}

/*
 * Converts superblock to network byte order
 */
void convertToNet(superblock_entry_t * sb)
{
    sb->block_size = htons(sb->block_size);
    sb->num_blocks = htonl(sb->num_blocks);
    sb->fat_start = htonl(sb->fat_start);
    sb->fat_blocks = htonl(sb->fat_blocks);
    sb->dir_start = htonl(sb->dir_start);
    sb->dir_blocks = htonl(sb->dir_blocks);
}

/*
 * Locates filename on image.
 * Returns location of first block or 0xffffffff if end of file.
 */
unsigned int findFileOnDisk(diskimage_t * image, char * filename)
{
    int i, j;
    // for each block of root
    for(i = 0; i < image->sb->dir_blocks; i++)
    {
        // for each directory entry of block
        for(j = 0; j < image->sb->block_size/SIZE_DIR_ENTRY; j++)
        {
            sseek(image->f, ((image->sb->dir_start + i) * image->sb->block_size + j * SIZE_DIR_ENTRY), SEEK_SET);
            directory_entry_t de;
            sread(&de, sizeof(directory_entry_t), 1, image->f);

            if(de.status == DIR_ENTRY_AVAILABLE || strcmp(de.filename, filename) != 0)
                continue;

            convertToNetDE(&de);
            return de.start_block;
        }
    }
    return 0;
}

/*
 * Prints to stdout the file in image that starts at start_block
 */
void catFile(diskimage_t * image, unsigned int start_block)
{
    char block_data[image->sb->block_size];

    sseek(image->f, start_block * image->sb->block_size, SEEK_SET);
    sread(&block_data, sizeof(char), image->sb->block_size, image->f);

    printf("%s", block_data);

    unsigned long int block = read_fat_entry(image, start_block);
    for(;block != FAT_LASTBLOCK;)
    {
        // read next block from FAT

        sseek(image->f, block * image->sb->block_size, SEEK_SET);
        sread(&block_data, sizeof(char), image->sb->block_size, image->f);
        printf("%s", block_data);
        //NextBlockStart = FatStart + CurrentBlockStart * 4 (each FAT entry is 4 bytes)
        block = read_fat_entry(image, block);
    }
}

/*
 * Input: diskimage, current block
 * Returns: next block of file
 */
unsigned long int read_fat_entry(diskimage_t * image, unsigned long int entry)
{
    sseek(image->f, image->sb->fat_start * image->sb->block_size + entry * SIZE_FAT_ENTRY, SEEK_SET);
    unsigned long int ret;
    sread(&ret, sizeof(unsigned long int), 1, image->f);
    ret = htonl(ret);
    return ret;
}

/*
 * Convert directory entry to network byte order
 */
void convertToNetDE(directory_entry_t * de)
{
    de->start_block = htonl(de->start_block);
    de->num_blocks = htonl(de->num_blocks);
    de->file_size = htonl(de->file_size);
}

/******************** MAIN ************************/

int main(int argc, char *argv[]) {
    int  i;
    superblock_entry_t sb;
    char *imagename = NULL;
    char *filename  = NULL;

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
        }
    }

    if (imagename == NULL || filename == NULL) {
        fprintf(stderr, "usage: catuvfs --image <imagename> " \
            "--file <filename in image>");
        exit(1);
    }

/******************** END Z *********************/

    if( (image.f = fopen(imagename, "rb+")) == NULL)
    {
        fprintf(stderr, "Specified image could not be opened.\n");
        exit(1);
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

    unsigned long int start_block;

    if((start_block = findFileOnDisk(&image, filename)) == 0)
    {
        fprintf(stderr, "File not found on specified image.\n");
        exit(1);
    }

    catFile(&image, start_block);

    if(image.f)
        fclose(image.f);

    return 0; 
}
