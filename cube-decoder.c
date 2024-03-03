#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "blowfish.h"

typedef unsigned int DWORD;

// Helper function for converting an array of four BYTEs into a 32-bit integer
DWORD byte_to_dword(BYTE *b)
{
    return (b[0]) | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

// Helper function for calculating size of a file
long int fsize(FILE *f) {
    long int original, size;
    original = ftell(f);
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, original, SEEK_SET);
    return size;
}

// Helper function for checking for string suffix
int ends_with(char *subject, char *ending) {
	int subjectlength = strlen(subject);
	int endlength = strlen(ending);
	if (endlength > subjectlength) {
		return 0;
	}
	return 0 == strncmp(subject + subjectlength - endlength, ending, endlength);
}

// Helper function to determine file extension from a given file name
char *get_file_extension(char path[])
{
    char *result;
    int i, n;

    assert(path != NULL);
    n = strlen(path);
    i = n - 1;
    while ((i > 0) && (path[i] != '.') && (path[i] != '/') && (path[i] != '\\')) {
        i--;
    }
    if ((i > 0) && (i < n - 1) && (path[i] == '.') && (path[i - 1] != '/') && (path[i - 1] != '\\')) {
        result = path + i;
    }
    else {
        result = path + n;
    }
    return result;
}

// Safe strncpy with null termination
size_t strlcpy(char* dst, const char* src, size_t bufsize)
{
    size_t srclen = strlen(src);
    size_t result = srclen; /* Result is always the length of the src string */
    if (bufsize>0)
    {
        if (srclen >= bufsize)
            srclen = bufsize - 1;
        if (srclen>0)
            memcpy(dst, src, srclen);
        dst[srclen] = '\0';
    }
    return result;
}

int main(int argc, char **argv) {
    char cubepro_key[] = "221BBakerMycroft";    // Key used to decrypt .cubepro / .cube3 / .cube files
    char cubex_key[] = "kWd$qG*25Xmgf-Sg";      // Key used to decrypt .cubex files

    char *userkey;
    BLOWFISH_KEY key;

    char *infilename, *outfilename, *infilenameext;
    long int outfilesize, infilesize, readcount, writecount, offset, i;

    /* TODO:
    Implement legacy mode for compatibility with CodeX:
    argv[0] [CUBEPRO | CUBEX] [ENCODE | DECODE | RECODE] inputfile outputfile
    */

    userkey = NULL;

    // "Parse" arguments
    if (argc >= 2) {
        infilename = argv[1];
        infilenameext = get_file_extension(infilename);

        // Determine which decryption key to use
        if (strcmp(infilenameext, ".cubepro") == 0 || strcmp(infilenameext, ".cube3") == 0 || strcmp(infilenameext, ".cube") == 0) {
            userkey = cubepro_key;
        }
        else if (strcmp(infilenameext, ".cubex") == 0) {
            userkey = cubex_key;
        }

        if (argc == 2) {
            outfilename = malloc(strlen(infilename));
            strlcpy(outfilename, infilename, strlen(infilename) - strlen(infilenameext) + 1);
            strcat(outfilename, ".bfb");
        }
        else {
            outfilename = argv[2];
        }
    } 
    
    if (userkey == NULL)  {
        printf("Usage:\n"
               "    %s inputfile [outputfile]\n"
               "    outputfile defaults to inputfile with the file extension changed to .bfb\n"
               "    inputfile file extension must be .cubex or .cube or .cube3 or .cubepro\n", argv[0]);
        return 1;
    }

    // Load input file
    FILE *infile = fopen(infilename, "rb");
    if (infile == NULL) {
        perror("Unable to open input file");
        return 2;
    }
    infilesize = fsize(infile);
    if (infilesize < 0) {
        perror("Unable to determine size of input file");
        return 3;
    }
    BYTE *data = malloc(infilesize);
    if (data == NULL) {
        perror("Unable to allocate memory for input file");
        return 4;
    }
    readcount = fread(data, 1, infilesize, infile);
    if (readcount != infilesize) {
        printf("Unable to read the whole input file: %zd != %zd\n", readcount, infilesize);
        return 5;
    }
    fclose(infile);

    // Check to see if this is the "new" format .cubepro file
    // this newer format is what appears to be a simple uncompressed file archive which allows 
    // multiple files to be bundled together including an XML file with build information, several 
    // preview images, a checksum file of some sort, and and the actual encrypted gcode file we
    // want to find the offset of the archive file  with a .cubepro file in it which can then be 
    // processed by the decryption routine

    // New format has a header
    //  - DWORD = number of files in the archive
    //  - DWORD = size of the archive
    //  - WORD  = x0108 - size of the header chunk that precedes each file

    // Let's check the second DWORD and compare against the archive's file size and if this matches
    // assume we're dealing with a newer format file

    DWORD storedfile_filesize = 0;

    if (byte_to_dword(&data[4]) == infilesize) {
        char storedfile_filename[260] = "";
        int gcodefile_found = 0;

        i = 10;

        do {
            // Each file in the archive is preceded by chunk of data containing size and filename
            //  - DWORD - the size of the stored file
            //  - char[260] - the original name of the stored file (null terminated/padded)

            storedfile_filesize = byte_to_dword(&data[i]);
            i += 4;
            strncpy(storedfile_filename, (char*) &data[i], 260);
            i += storedfile_filesize + 260;
            
            // If the storedfile_'s file name ends in .cubepro then we've found what we're looking for...
            gcodefile_found = ends_with(storedfile_filename, ".cubepro") || ends_with(storedfile_filename, ".cube3");

        } while (!gcodefile_found && i <= infilesize);

        if (gcodefile_found) {
            offset = i - storedfile_filesize;
            infilesize = storedfile_filesize;
        }
        else {
            return 8;
        }
    }
    else {
        offset = 0;
    }
    
    // Decrypt data
    blowfish_key_setup((BYTE*) userkey, &key, strlen(userkey));
    for (i = offset; i < offset + infilesize; i += 8) {
        blowfish_decrypt(&data[i], &data[i], &key, 1);
    }

    // Remove padding
    BYTE pad = data[offset + infilesize - 1];
    if (pad > 8) {
        printf("Decoding error: Invalid padding. Make sure that this is a valid encoded file.\n");
    }
    outfilesize = infilesize - pad;

    // Save data to output file
    FILE *outfile = fopen(outfilename, "wb+");
    if (outfile == NULL) {
        perror("Unable to open output file");
        return 6;
    }
    writecount = fwrite(&data[offset], 1, outfilesize, outfile);
    if (writecount != outfilesize) {
        printf("Unable to write the whole output file: %zd != %zd\n", writecount, outfilesize);
        return 7;
    }
    fclose(outfile);

    return 0;
}