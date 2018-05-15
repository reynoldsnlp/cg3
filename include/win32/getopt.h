#ifndef GETOPT_H
#define GETOPT_H

#ifdef __cplusplus
	extern "C" {
#endif

#include <stdio.h>
#include <string.h>

int getopt(int argc, char** argv, const char* opts);

extern int opterr;
extern int optind;
extern int optopt;
extern char *optarg;

#ifdef __cplusplus
	}
#endif

#endif
