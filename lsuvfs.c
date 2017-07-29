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
} __attribute__ ((packed));


typedef struct datetime datetime_t;
struct datetime {
    short year;
    short month;
    short day;
    short hour;
    short minute;
    short second;
    char * smonth[3];
};

/************************* FUNCTION PROTOTYPES ****************************/

char *month_to_string(short m);
void unpack_datetime(unsigned char *time, short *year, short *month, short *day, short *hour, short *minute, short *second);
void sseek(FILE * f, int len, int origin);
void sread(void * buffer, size_t sizeofelements, size_t num, FILE *f);
void convertToNet(superblock_entry_t * sb);
void readRootDirectory(diskimage_t * image, FILE * f);
void printDirectoryEntry(directory_entry_t de, datetime_t dt);
void convertToNetDE(directory_entry_t * de);
void convertToNetDT(datetime_t * dt);

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
 * Reads and prints out each entry in the root directory
 */
void readRootDirectory(diskimage_t * image, FILE * f)
{
    int i, j;
    // for each block of root
    for(i = 0; i < image->sb->dir_blocks; i++)
    {
        // for each directory entry of block
        for(j = 0; j < image->sb->block_size/SIZE_DIR_ENTRY; j++)
        {
            sseek(f, ((image->sb->dir_start + i) * image->sb->block_size + j * SIZE_DIR_ENTRY), SEEK_SET);
            directory_entry_t de;
            datetime_t dt;
            sread(&de, sizeof(directory_entry_t), 1, f);

            if(de.status == DIR_ENTRY_AVAILABLE)
                continue;

            convertToNetDE(&de);

            unpack_datetime(de.modify_time, &dt.year, &dt.month, &dt.day, &dt.hour, &dt.minute, &dt.second);

            printDirectoryEntry(de, dt);
        }
    }
}

void printDirectoryEntry(directory_entry_t de, datetime_t dt)
{
    if(de.status == DIR_ENTRY_DIRECTORY)
        printf("d%7d ", de.file_size);
    else   
        printf("%8d ", de.file_size);
    printf("%02d-%s-%02d %02d:%02d:%02d %s\n", dt.year, month_to_string(dt.month), dt.day, dt.hour, dt.minute,
        dt.second, de.filename);
}

void convertToNetDE(directory_entry_t * de)
{
    de->start_block = htonl(de->start_block);
    de->num_blocks = htonl(de->num_blocks);
    de->file_size = htonl(de->file_size);
}

void convertToNetDT(datetime_t * dt)
{
    //dt->year = htons(dt->year);
    // dt->month = htons(dt->month);
    // dt->day = htons(dt->day);
    // dt->hour = htons(dt->hour);
    // dt->minute = htons(dt->minute);
    // dt->second = htons(dt->second);
}

/******************** MAIN ************************/

int main(int argc, char *argv[]) {
    superblock_entry_t sb;
    int  i;
    char *imagename = NULL;
    FILE *f;

    diskimage_t image;
    image.sb = &sb;

/******************* ZASTRE ***********************/

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL)
    {
        fprintf(stderr, "usage: lsuvfs --image <imagename>\n");
        exit(1);
    }

/******************** END Z *********************/

    if( (f = fopen(imagename, "rb+")) == NULL)
    {
        fprintf(stderr, "Specified image could not be opened.\n");
        exit(1);
    }

    image.imagename = imagename;
    sseek(f, 0L, SEEK_SET);

    // read in super block
    sread(image.sb, sizeof(superblock_entry_t), 1, f);

    // validate image
    if(strncmp(image.sb->magic, FILE_SYSTEM_ID, 8) != 0)
    {
        fprintf(stderr, "Image did not match expected format.\n");
        exit(1);
    }

    convertToNet(image.sb);

    readRootDirectory(&image, f);

    if(f)
        fclose(f);

    return 0; 
}
