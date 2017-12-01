#include "stdafx.h"

static IMAGE_OS2_HEADER nehdr_orig, nehdr_new;

int initFiles(
	const char* szIn, 
	const char* szOut,
	FILE **fin,
	FILE **fout) 
{
	if (*fin = fopen(szIn, "rb")) {
		if (*fout = fopen(szOut, "w+b"))
			return 0;
		else {
			fclose(*fin);
			return -1;
		}
	}
}

// NE EDIT copy
int need_copy(
	FILE* fin,
	FILE* fout,
	long cbytes)
{
	char buf[1024];

	long loops = cbytes / 1024;
	int remainder = cbytes % 1024;

	// copy chunks of 1kb at a time
	for (long i = 0; i < loops; i++) {
		if (fread(buf, 1, 1024, fin) < 1024
			|| fwrite(buf, 1, 1024, fout) < 1024)
			return -1;	// fail
	}
	// copy the remainder (if any)
	if (remainder) {
		if (fread(buf, 1, remainder, fin) < remainder
			|| fwrite(buf, 1, remainder, fout) < remainder)
			return -1;
	}

	return 0;
}

// NE EDIT copy-pad = copy and pad with zeroes in the target
int need_copy_pad(
	FILE* fin,
	FILE* fout,
	long cbytes_copy,
	long cbytes_pad)
{
	char zero = 0;

	if (need_copy(fin, fout, cbytes_copy))
		return -1;
	
	while (cbytes_pad-- > 0) {
		if (!fwrite(&zero, 1, 1, fout))
			return -1;
	}

	return 0;
}

// process DOS hdr, return 0 iff success
int processMZ(
	FILE *fin,
	FILE *fout)
{
	char buf[0x400];

	return (fread(buf, 1, 0x400, fin) < 0x400			// zero for successful read
		|| *((short int *)buf) != *((short int *) "MZ")	// zero for correct magic number
		|| *((int *)buf[0x3C]) != 0x00000400			// zero for expected NE hdr offset
		|| fwrite(buf, 1, 0x400, fout) < 0x400);		// zero for successful write
}

int processNE(
	FILE *fin,
	FILE *fout)
{

	if (fread(&nehdr_orig, sizeof(nehdr_orig), 1, fin) < sizeof(nehdr_orig)
		|| nehdr_orig.ne_magic != IMAGE_OS2_SIGNATURE) {
		// abort
		return -1;
	}

	// initialize the new hdr with old info
	nehdr_new = nehdr_orig;

	int imp_fixup = 2; // FIXME: add size of new imported name
    // compute the sector-aligned fixup
	int gang_fixup = log_displace(nehdr_new.ne_align, imp_fixup);

	++nehdr_new.ne_cmod;				// create space to import a new module
	nehdr_new.ne_imptab += 2;			// shift import table by the size of a new modtab entry
	nehdr_new.ne_enttab += imp_fixup;	// shift entry table
	nehdr_new.ne_nrestab += imp_fixup;	// shift non resident table
	nehdr_new.fastload_offset += gang_fixup;

	return (fwrite(&nehdr_new, sizeof(nehdr_new), 1, fout) < sizeof(nehdr_new));
}

// converts a displacement in bytes into one in logical sectors
inline int log_displace(
	int csecbits,		// log of sector size
	int cdisplbytes)	// #bytes to displace
{
	return (cdisplbytes % (1 << csecbits))	// is there a remainder?
		? (cdisplbytes >> csecbits + 1)		// if so, 
		: (cdisplbytes >> csecbits);
}

void finalize(
	FILE *fin,
	FILE *fout)
{
	// TODO
}