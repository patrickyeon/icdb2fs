#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define HEADER_SKIP_BYTES 0x50
#define LISTING_SKIP_BYTES 24
#define LISTING_TRAILING_BYTES 256 - LISTING_SKIP_BYTES - 5

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
    // TODO update LISTING_TRAILING_BYTES whenever this struct changes
    uint8_t unhandled[LISTING_SKIP_BYTES];
    uint32_t char_count;
    char filename; // don't know a max size, but it's NULL-padded...
    uint8_t trailer[LISTING_TRAILING_BYTES];
};
#pragma pack()

int main(int argc, char **argv)
{
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
    int i;
    for(i = 0; i < header->num_listings; i++)
    {
        if(pread(fd, templist, sizeof(struct listing),
                 header->offset_listings + (i * sizeof(struct listing))) !=
                 sizeof(struct listing))
        {
            fprintf(stderr, "Ran out of file before finishing listings\n\n");
            return(-1);
        }
        fwrite(&(templist->filename), templist->char_count, 1, stdout);
        printf("\n");
        //FIXME not error-checking there
    }

    return(0);
}
