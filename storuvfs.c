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
void                swrite(void * buffer, size_t sizeofelements, size_t num, FILE *f);
void                convertToNet(superblock_entry_t * sb);
void                convertToNetDE(directory_entry_t * de);
void                convertToHostDE(directory_entry_t * de);
unsigned int        findFileOnDisk(diskimage_t * image, char * filename);
unsigned long int   read_fat_entry(diskimage_t * image, unsigned long int entry);
void                write_file_to_image(diskimage_t * image, char * filename, FILE * src_file);
void                read_fat(diskimage_t * image, int * FAT);
void                write_fat(diskimage_t * t, int * FAT);

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
 * Safe write
 * Runs fwrite with input params and handles error
 */
void swrite(void * buffer, size_t sizeofelements, size_t num, FILE *f)
{
    if ( fwrite(buffer, sizeofelements, num, f) != num )
    {
        fprintf(stderr, "Write failed.\n");
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
 * Convert directory entry to network byte order
 */
void convertToNetDE(directory_entry_t * de)
{
    de->start_block = htonl(de->start_block);
    de->num_blocks = htonl(de->num_blocks);
    de->file_size = htonl(de->file_size);
}

/*
 * Locates filename on image.
 * Returns location of first block or 0xffffffff if end of file or 0 if not found.
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
 * Writes src file to the specified image under filename
 */
void write_file_to_image(diskimage_t * image, char * filename, FILE * src_file)
{

    // find where to write de
    unsigned long int de_write_byte = 0;
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

            if(de.status == DIR_ENTRY_AVAILABLE)
            {
                de_write_byte = (image->sb->dir_start + i) * image->sb->block_size + j * SIZE_DIR_ENTRY;
                break;
            }
        }
        if(de_write_byte != 0)
            break;
    }

    // read in FAT
    int FAT[image->sb->fat_blocks * image->sb->block_size / SIZE_FAT_ENTRY];
    read_fat(image, FAT);

    if(de_write_byte == 0)
    {
        fprintf(stderr, "No room for directory entry.\n");
        exit(1);
    }

    // for reading and writing file
    unsigned char write_buffer[image->sb->block_size + 1];

    // begin prepping de
    directory_entry_t de;
    de.status = DIR_ENTRY_NORMALFILE;
    sseek(image->f, image->sb->fat_start * image->sb->block_size, SEEK_SET);
    de.start_block = next_free_block(FAT, image->sb->num_blocks);

    // strncpy(&de._padding, "0xff0xff0xff0xff0xff0xff", 6);

    de.num_blocks = 0;
    strcpy(de.filename, filename);

    int bytes_read;
    unsigned int write_block = de.start_block;
    unsigned int next_block;

    sseek(src_file, 0L, SEEK_SET);

    while( (bytes_read = fread(&write_buffer, 1, image->sb->block_size, src_file)) > 0)
    {
        write_buffer[bytes_read] = '\0';

        FAT[write_block] = ntohl(FAT_RESERVED);
        next_block = next_free_block(FAT, image->sb->num_blocks);
        FAT[write_block] = ntohl(next_block);
        
        if(next_block == 0)
        {
            fprintf(stderr, "Not enough room for file.\n");
            exit(1);
        }

        //printf("%s", write_buffer);
        //printf("Saving %x to %x\n", next_block, write_block);
        
        sseek(image->f, write_block * image->sb->block_size, SEEK_SET);
        swrite(&write_buffer, sizeof(char), bytes_read, image->f);

        write_block = next_block;
        de.num_blocks++;
    }

    // end file and write FAT back to disk
    FAT[write_block] = FAT_LASTBLOCK;
    write_fat(image, FAT);

    // finish up and write de
    de.file_size = (de.num_blocks - 1) * image->sb->block_size + bytes_read;
    pack_current_datetime(de.create_time);
    pack_current_datetime(de.modify_time);

    sseek(image->f, de_write_byte, SEEK_SET);
    convertToHostDE(&de);
    swrite(&de, sizeof(de), 1, image->f);
}

void convertToHostDE(directory_entry_t * de)
{
    de->start_block = ntohl(de->start_block);
    de->num_blocks = ntohl(de->num_blocks);
    de->file_size = ntohl(de->file_size);
}

void read_fat(diskimage_t * image, int * FAT)
{
    sseek(image->f, image->sb->fat_start * image->sb->block_size, SEEK_SET);
    sread(FAT, sizeof(int), image->sb->fat_blocks * image->sb->block_size / SIZE_FAT_ENTRY, image->f);
}

void write_fat(diskimage_t * image, int * FAT)
{
    sseek(image->f, image->sb->fat_start * image->sb->block_size, SEEK_SET);
    swrite(FAT, sizeof(int), image->sb->fat_blocks * image->sb->block_size / SIZE_FAT_ENTRY, image->f);
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

    if( (image.f = fopen(imagename, "rb+")) == NULL )
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

    // check file doesn't already exist
    if(findFileOnDisk(&image, filename) != 0)
    {
        fprintf(stderr, "File already on specified image.\n");
        exit(1);
    }

    write_file_to_image(&image, filename, src_file);

    return 0; 
}
