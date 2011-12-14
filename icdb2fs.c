#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#define HEADER_SKIP_BYTES 72
#define LISTING_LEN 256
#define LISTING_NAME_LEN 192
#define ICDB_SEP '\\'
#define HOST_SEP '/'
#define DEF_OUT_DIR "."
#define DEF_OUT_DIR_LEN 2

#define EBADENTRY 1
#define EFILE     2
#define EMEM      3

#pragma pack(1)
typedef struct dbhead
{
    uint32_t version[2]; // of what? I don't know, but it looks pretty static
    uint8_t unhandled[HEADER_SKIP_BYTES];
    uint32_t num_listings; // total number of files in this db
    uint32_t offset_listings; // offset into db of first file listing
    uint32_t trailing_byte;
} dbhead;

typedef struct listing
{
    uint32_t list_len; // may only be a uint_8. Looks like it's the number of
                       // entries in this section if it's the first listing, 
                       // 0x01 otherwise?
    uint32_t head_only[2]; // 0x0 when not the table head? When it is the table
                           // head, head_only[0] may be bytelength of the table,
                           // head_only[1] I've only seen 0x6410
    // TODO I no longer understand what I meant by the following comment
    uint32_t back_step; // backwards offset to start of prev. listing, from 
                        // start of this listing. Includes the NULL-padding, is
                        // offset of next batch of listings for the first
                        // listing in a batch, or 0x0 if there is no next batch
    uint32_t self_ref; // seems to be an offset that points back to here, from
                       // the beginning of the db file
    uint32_t gap; // maybe always zero?
    uint32_t char_count;
    char filename[LISTING_NAME_LEN]; // NULL-padded, guessing at max size
    uint8_t guid[24]; // see unscramble_guid to see how it's made to match the
                      // GUID form that I found in \sids.
    uint32_t data_size; // just a guess right now
    uint32_t data_offset; // offset into icdb.dat this file starts at
                          // File has a header there too, sizeof(uint8_t) * 16
    uint32_t last_flag; // looks like it's always 1?
} listing;
#pragma pack()

int check_listing(listing *entry, dbhead *dbheader, int index);
void unscramble_guid(uint8_t *guid);
FILE *open_icdb(char *filename, dbhead *header);
int get_listing_offset(FILE *db, int offset, listing *ret);
void human_size(int bytes, char *buff);
void print_contents(FILE *db, dbhead *header);
void usage();
int extract(FILE *db, dbhead *header, char *outdir);
int get_listings(FILE *db, dbhead *header, int start, int end, listing **ret);

int main(int argc, char **argv)
{
    assert(sizeof(listing) == LISTING_LEN);
    if(argc < 2 || argv[1][1] != '\0')
    {
        usage(argv);
    }

    FILE *db;
    dbhead header;

    if((db = open_icdb(argv[2], &header)) == NULL)
    {
        return(-1);
    }

    if(argv[1][0] == 't')
    {
        print_contents(db, &header);
    }

    else if(argv[1][0] == 'x')
    {
        char *outdir;
        if(argc > 3)
        {
            outdir = strdup(argv[3]);
        }
        else
        {
            outdir = malloc(DEF_OUT_DIR_LEN * sizeof(char));
            strncpy(outdir, DEF_OUT_DIR, DEF_OUT_DIR_LEN);
        }

        if(extract(db, &header, outdir) != 0)
        {
            printf("Extract error\n");
        }
        free(outdir);
    }

    return(0);
}

void usage(char **argv)
{
    fprintf(stderr, "\nUsage: %s t | x dbfile [path]\n", argv[0]);
    fprintf(stderr, "t -- list files in dbfile\n");
    fprintf(stderr, "x -- extract files to path (default ./)\n");
    exit(-1);
}

void print_contents(FILE *db, dbhead *header)
{
    printf("\n%d files\n", header->num_listings);
    listing *entries = NULL;
    if(get_listings(db, header, 0, header->num_listings, &entries) != 0)
    {
        printf("Haha nope.\n");
        return;
    }
    int i;
    for(i = 0; i < header->num_listings; i++)
    {
        char hsize[6];
        human_size((int) (entries[i].data_size), hsize);
        printf("%5s %s\n", hsize, entries[i].filename);
    }
    free(entries);
}

void human_size(int bytes, char *buff)
{
    //buff better have 6 chars for me.
    char unit = '\0';
    if(bytes >= 1024 * 1024 * 2)
    {
        // so long as using 32bit ints, max file size is 4GB. I think it's fair
        // to not worry about eg. 1.2G and instead put eg. 1230M
        bytes = bytes / (1024 * 1024);
        unit = 'M';
    }
    else if(bytes >= 1024 * 2)
    {
        bytes = bytes / 1024;
        unit = 'K';
    }
    snprintf(buff, 6, "%d%c", bytes, unit);
    return;
}

int extract(FILE *db, dbhead *header, char *outdir)
{
    printf("Copying %d files to %s\n", header->num_listings, outdir);
    int prefixlen = strlen(outdir);
    char *tempfilename = malloc((prefixlen + LISTING_NAME_LEN) * sizeof(char));
    if(tempfilename == NULL)
    {
        return(-1); // but seriously, how likely?
    }
    strcpy(tempfilename, outdir);

    listing *entries = NULL;
    int err;
    if((err = get_listings(db, header, 0, header->num_listings, &entries)) != 0)
    {
        return(err);
    }
    int i;
    for(i = 0; i < header->num_listings; i++)
    {
        strcpy(tempfilename + prefixlen, entries[i].filename);
        // that clobbers the prefix's terminating \0 on purpose
        
        // change the directory seperators if local system requires it
        if(ICDB_SEP != HOST_SEP)
        {
            char *c;
            while((c = strchr(tempfilename, ICDB_SEP)) != NULL)
            {
                *c = HOST_SEP;
            }
        }

        // create the directory if necessary
        char *predir = strdup(tempfilename);
        if(predir == NULL)
        {
            printf("Mem error\n");
            continue;
        }
        // TODO will I see trouble if *predir = "\0"?
        // can that even happen?
        char *c = predir + 1;
        while((c = strchr(c, HOST_SEP)) != NULL)
        {
            c++;
            char *path = strndup(predir, c - predir);
            if(path == NULL)
            {
                printf("Mem error\n");
            }
            struct stat trash;
            errno = 0;
            if(stat(path, &trash) != 0 && (errno | ENOENT))
            {
                if(mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) != 0)
                {
                    printf("Error creating dir: %s\n", path);
                    continue;
                }
            }
            free(path);
        }
        free(predir);

        FILE *outfile = fopen(tempfilename, "w");
        // TODO maybe tweak the flags there when you are a little more sure
        // that you're not about to break things on the fs
        // Also, maybe check about clobbering already-present files?
        if(outfile == NULL)
        {
            printf("Error on output file: %s\n", tempfilename);
            continue;
        }
        void *buff = malloc(1024); //arbitrary buffer size
        //TODO not sure that a void * is the proper thing there

        if(fseek(db, entries[i].data_offset + 0x10, SEEK_SET) != 0)
        {
            printf("Seek error: %s\n", tempfilename);
            continue;
        }
        int bytesleft;
        for(bytesleft = entries[i].data_size; bytesleft > 1024; bytesleft -= 1024)
        {
            if(fread(buff, 1, 1024, db) != 1024 ||
               fwrite(buff, 1, 1024, outfile) != 1024)
            {
                bytesleft = 0;
                printf("Copy error on %s\n", tempfilename);
                break;
            }
        }
        if(fread(buff, 1, bytesleft, db) != bytesleft ||
           fwrite(buff, 1, bytesleft, outfile) != bytesleft)
        {
            printf("Copy error on %s\n", tempfilename);
        }

        fclose(outfile);
    }
    free(tempfilename);
    return(0);
}

FILE *open_icdb(char *filename, dbhead *header)
{
    // return the FILE* (!= NULL) and set *header pointing at the header if
    // successful, returns NULL to signal error.

    FILE *icdb = fopen(filename, "r");
    if(icdb == NULL)
    {
        fprintf(stderr, "\nError trying to open file\n\n");
        return(NULL);
    }
    if(fread(header, 1, sizeof(dbhead), icdb) != sizeof(dbhead))
    {
        fprintf(stderr, "\nFile too small to get a whole header\n\n");
        fclose(icdb);
        return(NULL);
    }
    // Finally, have a damn header
    return(icdb);
}

int check_listing(listing *entry, dbhead *dbheader, int index)
{
    // Check various assumptions about what the entries mean
    uint32_t warn = 0;
    // is the assumption about the first value correct?
    if(index != 0 && entry->list_len != 1)
    {
        warn |= 1 << 1;
    }
    // are the head-only bytes really head-only?
    if(index != 0 && (entry->head_only[0] || entry->head_only[1]))
    {
        warn |= (1 << 2);
    }
    // does the self-ref just point to itself?
    /* offset calculation is wrong when they get broken into chunks
    if(entry->self_ref != dbheader->offset_listings + LISTING_LEN * index
                            + (void *)(&(entry->self_ref))
                            - (void *)entry)
    {
        warn |= (1 << 4);
    }*/
    // empty int?
    if(entry->gap != 0)
    {
        warn |= (1 << 5);
    }
    // last int is always 1?
    if(entry->last_flag != 1)
    {
        warn |= (1 << 6);
    }
    // filename is zero-padded, and nothing follows it?
    char *c;
    for(c = entry->filename + entry->char_count;
        c < entry->filename + LISTING_NAME_LEN; c++)
    {
        if(*c != 0){
            warn |= (1 << 7);
            break;
        }
    }
    // filename starts with directory seperator?
    if(entry->filename[0] != ICDB_SEP)
    {
        warn |= (1 << 8);
    }
    return(warn);
}

void unscramble_guid(uint8_t *guid)
{
    // I feel like I'm missing something obvious re: byte-ordering here, but 
    // I've not been able to figure out what, so brute-force!
    // This is also likely better re-done as a pure function.
    uint8_t scratch[24];
    int i;
    for(i = 0; i < 8; i++)
    {
        scratch[i] = guid[i];
    }
    for(i = 8; i < 16; i++)
    {
        scratch[i] = guid[i + 8];
    }
    for(i = 16; i < 24; i++)
    {
        scratch[i] = guid[i - 8];
    }
    for(i = 0; i < 24; i++)
    {
        guid[i] = (uint8_t)(scratch[i] << 4) | (scratch[i] >> 4);
    }
    return;
}

int get_listings(FILE *db, dbhead *header, int start, int end, listing **ret)
{
    // ret := array of listings, start <= id < end.
    // it should not be malloc'd
    assert(db != NULL);
    assert(header != NULL);
    assert(ret != NULL);
    assert(start >= 0 && start < end && end <= header->num_listings);

    listing *writeto = malloc((end - start) * sizeof(listing));
    *ret = writeto;

    int headid = 0;
    listing head;
    head.list_len = 0;
    head.back_step = header->offset_listings;
    do
    {
        headid += head.list_len;
        if(get_listing_offset(db, head.back_step, &head) < sizeof(listing))
        {
            return(-EFILE);
        }
    }
    while(start > headid + head.list_len);
    // start is now in the batch of listings starting at head

    int curid = start;
    while(end - curid > 0)
    {
        int stop = (end <= curid + head.list_len) ? end : curid + head.list_len;
        if(fseek(db, (curid - headid - 1) * sizeof(listing), SEEK_CUR) != 0 ||
           fread(writeto, sizeof(listing), stop - headid, db) < (stop - headid))
        {
            return(-EFILE);
        }
        writeto += (stop - headid);
        curid = stop;
        headid += head.list_len;
        // TODO don't like special-casing this.
        if(head.back_step == 0)
        {
            // if we're at the last batch, but not done, something's broken
            if(end - curid > 0)
            {
                return(-EBADENTRY);
            }
            else
            {
                // work is done
                break;
            }
        }
        if(get_listing_offset(db, head.back_step, &head) < sizeof(listing))
        {
            return(-EFILE);
        }
    }
    return(0);
}

int get_listing_offset(FILE *db, int offset, listing *ret)
{
    // get the listing directly from an offset, does no checking on the listing
    // I know it's not much now, but if I choose to make it smarter later, it's
    // all in one spot.
    // FIXME error handling
    fseek(db, offset, SEEK_SET);
    return(fread(ret, 1, sizeof(listing), db));
}
