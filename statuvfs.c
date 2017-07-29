#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include "disk.h"

/************************ IMAGE STRUCT *******************************/

typedef struct diskimage diskimage_t;
struct diskimage {
    char * imagename;
    superblock_entry_t * sb;
    unsigned int free_blocks;
    unsigned int resv_blocks;
    unsigned int alloc_blocks;
} __attribute__ ((packed));

/************************* FUNCTION PROTOTYPES ****************************/

void sseek(FILE * f, int len, int origin);
void sread(void * buffer, size_t sizeofelements, size_t num, FILE *f);
void convertToNet(superblock_entry_t * sb);
void print_image(diskimage_t image);
void read_FAT(diskimage_t * image, FILE * f);

/************************* FUNCTION IMPLEMENTATIONS *************************/

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
 * Print image as per assignment spec
 */
void print_image(diskimage_t image)
{

    printf("%s (%s)\n", image.sb->magic, image.imagename);
    printf("\n-------------------------------------------------\n");
    printf("  Bsz   Bcnt  FATst FATcnt  DIRst DIRcnt\n");
    printf("%5d  %5d  %5d  %5d  %5d  %5d\n", image.sb->block_size, image.sb->num_blocks, image.sb->fat_start,
        image.sb->fat_blocks, image.sb->dir_start, image.sb->dir_blocks);
    printf("\n-------------------------------------------------\n");
    printf(" Free   Resv  Alloc\n");
    printf("%5d  %5d  %5d\n", image.free_blocks, image.resv_blocks, image.alloc_blocks);
}

/*
 * Read FAT table information into image
 */
void read_FAT(diskimage_t * image, FILE * f)
{
    int i,j;
    // for each block of the FAT table
    for(i = 0; i < image->sb->fat_blocks; i++)
    {
        //sseek(f, (image->sb->fat_start + i) * image->sb->block_size, SEEK_SET);
        // for each FAT entry in block
        for(j = 0;
            j < image->sb->block_size/SIZE_FAT_ENTRY && 
            i*image->sb->block_size/SIZE_FAT_ENTRY + j < image->sb->num_blocks; 
            j++)
        {
            //ensure looking at FAT entry
            sseek(f, (image->sb->fat_start + i) * image->sb->block_size + j * SIZE_FAT_ENTRY, SEEK_SET);

            unsigned long int status;

            sread(&status, sizeof(unsigned long int), 1, f);
            status = htonl(status);

            if(status == FAT_AVAILABLE)
                image->free_blocks++;
            else if(status == FAT_RESERVED)
                image->resv_blocks++;
            else
                image->alloc_blocks++;
        }
    }
}

/************************* MAIN ****************************/

int main(int argc, char *argv[]) {
    superblock_entry_t sb;
    int  i;
    char *imagename = NULL;
    FILE  *f;
    //int   *fat_data;

    diskimage_t image;
    image.sb = &sb;
    image.free_blocks = 0;
    image.resv_blocks = 0;
    image.alloc_blocks = 0;

/******************* ZASTRE ***********************/

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i+1 < argc) {
            imagename = argv[i+1];
            i++;
        }
    }

    if (imagename == NULL)
    {
        fprintf(stderr, "usage: statuvfs --image <imagename>\n");
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

    read_FAT(&image, f);

    print_image(image);

    if(f)
        fclose(f);

    return 0; 
}
