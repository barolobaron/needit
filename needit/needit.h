#pragma once

#define	NEEDIT_NOTFOUND			(-1)
#define NEEDIT_MAPPING_ERROR	(-2)
#define NEEDIT_FILE_ERROR		(-3)
#define NEEDIT_INVALID_ARGUMENT	(-4)

#define NEEDIT_VER_MAJOR		0
#define NEEDIT_VER_MINOR		20

#define NEEDIT_ERR_PARAM_MISSING	(L"Required parameter missing.")
#define NEEDIT_ERR_PARAM_FORMAT		(L"Invalid parameter format.")
#define NEEDIT_ERR_NOT_FOUND		(L"File not found.")
#define NEEDIT_ERR_PARAM_VALUE		(L"Invalid parameter value.")
#define NEEDIT_ERR_SWITCH			(L"Invalid switch.")

int addImports(char *szFpInFile, char *szFpOutFile, char *newImports[], int cImports);
int fixDeps(char *inFile, char *outFile, char *dllFile, unsigned short oldMod, unsigned short newMod);
int redirect(char *inFile, char *outFile, unsigned short oldMod, unsigned short newMod, unsigned short funOrd);
void splash();
int failwith(wchar_t *err);
void print_help();
void ppRedirect(WORD oldMod, WORD newMod, const char *szImpName, WORD ordinal, WORD wSeg, WORD wFixup);
int getExportedNames(char *szInFile, std::set<WORD> *ordSet, std::set<std::string> *nameSet);