#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

#define HEADER_SKIP_BYTES 72
#define LISTING_LEN 256
#define LISTING_NAME_LEN 192

#pragma pack(1) // just going to hope this is doing what I think.
struct dbhead
{
    uint32_t version[2]; // of what? I don't know, but it looks pretty static
    uint8_t unhandled[HEADER_SKIP_BYTES];
    uint32_t num_listings;
    uint32_t offset_listings;
    uint32_t trailing_byte;
};

struct listing
{
    uint32_t list_len; // may only be a uint_8. Looks like it's the number of
                       // entries in this section if it's the first listing, 
                       // 0x01 otherwise?
    uint32_t head_only[2]; // unknown, but 0x0 when not the head?
    uint32_t not_head; // unknown, 0x0 when head?
    uint32_t self_ref; // seems to be an offset that points back to here
    uint32_t gap; // maybe always zero?
    uint32_t char_count;
    char filename[LISTING_NAME_LEN]; // NULL-padded, guessing at max size
    uint32_t foo[6]; // Don't know what's in here yet
    uint32_t data_size; // just a guess right now
    uint32_t data_offset; // offset into icdb.dat this file starts at
    uint32_t last_flag; // looks like it's always 1?
};
#pragma pack()

uint32_t check_listing(struct listing *listing, struct dbhead *dbheader,
                       int index);

int main(int argc, char **argv)
{
    assert(sizeof(struct listing) == LISTING_LEN);
    if(argc < 1)
    {
        fprintf(stderr, "\nUsage: %s <db file>\n -- Nothing fancy yet\n\n",
                argv[0]);
        return(-1);
    }

    // All the fun that goes along with opening files, malloc'ing memory
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

        printf("%d %5x %5x [%5x]", check_listing(templist, header, i),
               templist->data_offset,
               templist->data_offset + templist->data_size + 15, // 15=header-1
               templist->data_size);
        fwrite(&(templist->filename), templist->char_count, 1, stdout);
        printf("\n");
        //FIXME not error-checking there
    }

    return(0);
}

uint32_t check_listing(struct listing *listing, struct dbhead *dbheader,
                       int index)
{
    // Check various assumptions about what the entries mean
    uint32_t warn = 0;
    // is the assumption about the first value correct?
    if(index != 0 && listing->list_len != 1)
    {
        warn |= 1;
    }
    // are the head-only bytes really head-only?
    if(index != 0 && (listing->head_only[0] || listing->head_only[1]))
    {
        warn |= (1 << 1);
    }
    // what about the non-head entry?
    if(index == 0 && listing->not_head)
    {
        warn |= (1 << 3);
    }
    // does the self-ref just point to itself?
    if(listing->self_ref != dbheader->offset_listings + LISTING_LEN * index
                            + (void *)(&(listing->self_ref))
                            - (void *)listing)
    {
        warn |= (1 << 4);
    }
    // empty int?
    if(listing->gap != 0)
    {
        warn |= (1 << 5);
    }
    // last int is always 1?
    if(listing->last_flag != 1)
    {
        warn |= (1 << 6);
    }
    return(warn);
}
