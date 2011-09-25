#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define HEADER_SKIP_BYTES 0x50
#define LISTING_SKIP_BYTES 16
#define LISTING_LEN 256
#define LISTING_NAME_LEN 192

#pragma pack(1) // just going to hope this is doing what I think.
struct dbhead
{
    uint8_t unhandled[HEADER_SKIP_BYTES];
    uint32_t num_listings;
    uint32_t offset_listings;
    uint32_t trailing_byte;
};

struct listing
{
    uint8_t unhandled[LISTING_SKIP_BYTES];
    uint32_t self_ref; // seems to be an offset that points back to here
    uint32_t gap;
    uint32_t char_count;
    char filename[LISTING_NAME_LEN]; // NULL-padded, guessing at max size
    uint32_t foo[6]; // Don't know what's in here yet
    uint32_t data_size; // just a guess right now
    uint32_t data_offset; // offset into icdb.dat this file starts at
    uint32_t last_flag; // looks like it's always 1?
};
#pragma pack()

int main(int argc, char **argv)
{
    assert(sizeof(struct listing) == LISTING_LEN);
    if(argc < 1)
    {
        fprintf(stderr, "\nUsage: %s <db file>\n -- Nothing fancy yet\n\n",
                argv[0]);
        return(-1);
    }

    char *dbfile = argv[1];
    int fd = open(dbfile, O_RDONLY);
    if(fd == -1)
    {
        fprintf(stderr, "\nError trying to open file\n\n");
        return(-1);
    }
    struct dbhead *header = malloc(sizeof(struct dbhead));
    if(header == NULL)
    {
        fprintf(stderr, "\nMemory error\n\n");
        return(-1);
    }
    if(read(fd, header, sizeof(struct dbhead)) != sizeof(struct dbhead))
    {
        fprintf(stderr, "\nFile too small to get a whole header\n\n");
        return(-1);
    }
    // Finally, have a damn header

    printf("\n%d file listings found, starting at offset %x\n",
           header->num_listings, header->offset_listings);

    struct listing *templist = malloc(sizeof(struct listing));
    if(templist == NULL)
    {
        fprintf(stderr, "Memory error\n\n");
        return(-1);
    }
    int i, offset;
    for(i = 0, offset = header->offset_listings; i < header->num_listings;
        i++, offset += sizeof(struct listing))
    {
        if(pread(fd, templist, sizeof(struct listing), offset) !=
           sizeof(struct listing))
        {
            fprintf(stderr, "Ran out of file before finishing listings\n\n");
            return(-1);
        }
        int warn = ' ';
        if(templist->self_ref != offset + (void *)(&(templist->self_ref)) - 
                (void *)templist)
        {
            warn = '!';
        }
        printf("%c %5x %5x [%5x]", warn, templist->data_offset,
               templist->data_offset + templist->data_size, templist->data_size);
        fwrite(&(templist->filename), templist->char_count, 1, stdout);
        printf("\n");
        //FIXME not error-checking there
    }

    return(0);
}
