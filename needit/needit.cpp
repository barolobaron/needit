/////////////////////////////////////////////////////////////////////////////
// NEEDIT: New Executable Editor										   //
//                                                                         //
// Version history:                                                        //
//                                                                         //
// v. 0.2 (28 Nov 17) - options -a (add import) and -f (fix module); also  //
//                removes the (superseded) prehistorical code              //
//                                                                         //
// v. 0.1 (some time in Oct17) - option -r to redirect single ordinals     //
//                between any two imported modules                         //
//                                                                         //
// (prehistory) - duplicates all modules in the import table and redirects //
//                all unresolved ordinals/imported names to the duplicate  //
//                entry                                                    //
/////////////////////////////////////////////////////////////////////////////

// TODO: we are still missing (or haven't tested) the following error checks:
// - existence of files opened for reading
// - success of opening files for writing
// - various failures when accessing memory mapped files (SEH)
// - well-formedness of the executable

// NeEdit.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "needit.h"

char	*szDllExt = ".dll";

int main(int argc, char *argv[]) {
	// splash message
	splash();

	if (argc < 4)
		return failwith(NEEDIT_ERR_PARAM_MISSING);

	if (argv[3][0] != '-' || argv[3][1] == '\0' || argv[3][2] != '\0')
		return failwith(NEEDIT_ERR_PARAM_FORMAT);

	/* decide what to do */
	unsigned short oldMod, newMod, funOrd;
	switch (argv[3][1]) {
	case 'a':	/* add new imported modules */
		return addImports(argv[1], argv[2], argv + 4, argc - 4);

	case 'f':	/* fix missing dependencies in module */
		if (argc < 7)
			return failwith(NEEDIT_ERR_PARAM_MISSING);

		oldMod = atoi(argv[5]);
		newMod = atoi(argv[6]);
		return fixDeps(argv[1], argv[2], argv[4], oldMod, newMod);

	case 'r':	/* function redirect by ordinal */
		if (argc < 7)
			return failwith(NEEDIT_ERR_PARAM_MISSING);

		oldMod = atoi(argv[4]);
		newMod = atoi(argv[5]);
		funOrd = atoi(argv[6]);

		return redirect(argv[1], argv[2], oldMod, newMod, funOrd);

	default:
		return failwith(NEEDIT_ERR_SWITCH);
	}
	
}

void splash() {
	std::wcout << L"Towerlabsoft NeedIt: New Executable editor v." 
		<< NEEDIT_VER_MAJOR << L"." << NEEDIT_VER_MINOR << std::endl;
	std::wcout << L"(c) 2017 Wilmer Ricciotti. All rights reserved." << std::endl;
	std::wcout << std::endl;
}

int failwith(wchar_t *err) {
	std::wcout << err << std::endl << std::endl;
	print_help();
	return 1;
}

void print_help()
{
	std::wcout << L"needit <input.exe> <output.exe> -a <mod name>*" << std::endl;
	std::wcout << L"needit <input.exe> <output.exe> -f <module.dll> <old mod num> <new mod num>" << std::endl;
	std::wcout << L"needit <input.exe> <output.exe> -r <old mod num> <new mod num> <function ordinal>" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"Options:" << std::endl;
	std::wcout << std::endl;
	std::wcout << L"  -a        Add imported modules to the executable." << std::endl;
	std::wcout << L"            Each <mod name> is the name of a new imported module." << std::endl;
	std::wcout << std::endl;
	std::wcout << L"  -f        Fix unresolved imports." << std::endl;
	std::wcout << L"            Tries to resolve all the used imports with module number" << std::endl;
	std::wcout << L"            <old mod num> within the library file <module.dll>. Unresolved" << std::endl;
	std::wcout << L"            imports will be moved to the module number <new mod num>." << std::endl;
	std::wcout << std::endl;
	std::wcout << L"  -r        Redirect imported function." << std::endl;
	std::wcout << L"            The imported function <ordinal> of module <old mod num> will be" << std::endl;
	std::wcout << L"            redirected to <new mod num> (same ordinal)." << std::endl;
	std::wcout << std::endl;
}

int redirect(
	char *szInFile, 
	char *szOutFile, 
	unsigned short oldMod,
	unsigned short newMod,
	unsigned short funOrd)
{
	HANDLE hInFile, hmapInFile, hOutFile, hmapOutFile;
	char *pMapInFile, *pMapOutFile;
	PIMAGE_WIN31_HEADER pNE;
	int mzMRT, mzINT;
	LARGE_INTEGER cbInFile;
	WORD offSegtab;
	WORD cSeg;

	if ((hInFile = CreateFileA(szInFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
		== INVALID_HANDLE_VALUE)
		return NEEDIT_NOTFOUND;
	if (!GetFileSizeEx(hInFile, &cbInFile))
		return NEEDIT_FILE_ERROR;
	if ((hmapInFile = CreateFileMapping(hInFile, NULL, PAGE_READONLY, 0, 0, 0)) == NULL
		|| (pMapInFile = (char *)MapViewOfFile(hmapInFile, FILE_MAP_READ, 0, 0, 0)) == NULL)
		return NEEDIT_MAPPING_ERROR;

	DWORD offNE = *((DWORD *)(pMapInFile + 0x3C));
	pNE = (PIMAGE_WIN31_HEADER)(pMapInFile + offNE);

	mzINT = pNE->ne_imptab + offNE;
	mzMRT = pNE->ne_modtab + offNE;

	// CREATE OUTPUT FILE
	LARGE_INTEGER cbOutFile;
	cbOutFile.QuadPart = cbInFile.QuadPart;
	if ((hOutFile = CreateFileA(szOutFile, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL))
		== INVALID_HANDLE_VALUE)
		return NEEDIT_FILE_ERROR;
	if ((hmapOutFile = CreateFileMapping(hOutFile, NULL, PAGE_READWRITE, cbOutFile.HighPart, cbOutFile.LowPart, NULL)) == NULL
		|| (pMapOutFile = (char *)MapViewOfFile(hmapOutFile, FILE_MAP_WRITE, 0, 0, 0)) == NULL)
		return NEEDIT_MAPPING_ERROR;
	PIMAGE_WIN31_HEADER pOutNE = (PIMAGE_WIN31_HEADER)(pMapOutFile + offNE);

	// DO COPY
	// until INT (excluded)
	char *pIn = pMapInFile, *pOut = pMapOutFile;
	char *limit = pMapInFile + cbInFile.QuadPart;
	while (pIn < limit)
		*(pOut++) = *(pIn++);

	// finally, we fixup the output exe
	// NE hdr fixup
	pOutNE->ne_expver = 0x30A;

	// segment relocations fixup
	// this phase is specific to the -r switch

	offSegtab = pOutNE->ne_segtab;
	cSeg = pOutNE->ne_cseg;
	ne_segment_table_entry_s *segentry =
		(ne_segment_table_entry_s *)(pMapOutFile + offNE + offSegtab);
	for (; cSeg > 0; cSeg--, segentry++) {
		BYTE *segment_fixup = (BYTE *)
			(pMapOutFile + (segentry->seg_data_offset << 4) + segentry->seg_data_length);
		WORD cFixupEntries = *((WORD *)segment_fixup);
		relocation_entry_s *fixup = (relocation_entry_s *)(segment_fixup + 2);
		for (; cFixupEntries > 0; cFixupEntries--, fixup++) {
			WORD module = fixup->target1;
			int cbImportName = 0;
			std::string importName;
			if (fixup->relocation_type == 1		// imported ordinal
				&& fixup->target1 == oldMod		// module reference
				&& fixup->target2 == funOrd)	// imported ordinal
			{
				fixup->target1 = newMod;
				ppRedirect(oldMod, newMod, NULL, funOrd,
					pOutNE->ne_cseg - cSeg + 1, *((WORD *)segment_fixup) - cFixupEntries + 1);
			}
		}
	}

	std::cout << std::endl;

	// XXX: error checks missing
	UnmapViewOfFile(pMapInFile);
	CloseHandle(hmapInFile);
	CloseHandle(hInFile);
	UnmapViewOfFile(pMapOutFile);
	CloseHandle(hmapOutFile);
	CloseHandle(hOutFile);
	return 0;
}

int addImports(
	char *szFpInFile, 
	char *szFpOutFile,
	char *newImports[],
	int cImports)
{
	HANDLE hInFile, hmapInFile, hOutFile, hmapOutFile;
	char *pMapInFile, *pMapOutFile;
	PIMAGE_WIN31_HEADER pNE;
	int mzMRT, mzINT;
	int kMRT, kINT, kGLA;
	LARGE_INTEGER cbInFile;

	if ((hInFile = CreateFileA(szFpInFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
		== INVALID_HANDLE_VALUE)
		return NEEDIT_NOTFOUND;
	if (!GetFileSizeEx(hInFile,&cbInFile))
		return NEEDIT_FILE_ERROR;
	if ((hmapInFile = CreateFileMapping(hInFile, NULL, PAGE_READONLY, 0, 0, 0)) == NULL
		|| (pMapInFile = (char *) MapViewOfFile(hmapInFile, FILE_MAP_READ, 0, 0, 0)) == NULL)
		return NEEDIT_MAPPING_ERROR;

	DWORD offNE = *((DWORD *)(pMapInFile + 0x3C));
	pNE = (PIMAGE_WIN31_HEADER) (pMapInFile + offNE);

	// determine extension constants
	// kMRT, kINT, kGLA

	// kMRT adds as many words as the size of newImports
	// kINT depends on the new import names
	kINT = 0;
	for (int ctmp = 0; ctmp < cImports; kINT += strlen(newImports[ctmp++]) + 1); // add 1 for pascal-style length
	kMRT = cImports * 2;
	mzINT = pNE->ne_imptab + offNE;
	mzMRT = pNE->ne_modtab + offNE;

	// kGLA is the adjustment needed to align Seg1 to 16
	// first, find offSeg1
	WORD offSegtab = pNE->ne_segtab;
	WORD cSeg = pNE->ne_cseg;
	WORD mzOffSeg1 = 0xFFFF;
	WORD tmpoffS;
	while (cSeg > 0) {
		tmpoffS = *((WORD *)(pMapInFile + offNE + offSegtab + (--cSeg) * 8));
		if (tmpoffS < mzOffSeg1)
			mzOffSeg1 = tmpoffS;
	}
	mzOffSeg1 <<= 4;
	// finally adjust the alignment
	kGLA = (- (mzOffSeg1 + kMRT + kINT)) & 0xF;

	// CREATE OUTPUT FILE
	LARGE_INTEGER cbOutFile;
	cbOutFile.QuadPart = cbInFile.QuadPart + kMRT + kINT + kGLA;
	if ((hOutFile = CreateFileA(szFpOutFile, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL))
		== INVALID_HANDLE_VALUE)
		return NEEDIT_FILE_ERROR;
	if ((hmapOutFile = CreateFileMapping(hOutFile, NULL, PAGE_READWRITE, cbOutFile.HighPart, cbOutFile.LowPart, NULL)) == NULL
		|| (pMapOutFile = (char *)MapViewOfFile(hmapOutFile, FILE_MAP_WRITE, 0, 0, 0)) == NULL)
		return NEEDIT_MAPPING_ERROR;
	PIMAGE_WIN31_HEADER pOutNE = (PIMAGE_WIN31_HEADER)(pMapOutFile + offNE);

	// DO COPY
	// until INT (excluded)
	char *pIn = pMapInFile, *pOut = pMapOutFile;
	char *limit = pMapInFile + mzINT;
	while (pIn < limit)
		*(pOut++) = *(pIn++);
	// extend MRT and copy until ET (excluded)
	pOut += kMRT;
	limit = pMapInFile + pNE->ne_enttab + offNE;
	while (pIn < limit)
		*(pOut++) = *(pIn++);
	// extend INT and copy until Seg1 (excluded)
	pOut += kINT;
	limit = pMapInFile + mzOffSeg1;
	while (pIn < limit)
		*(pOut++) = *(pIn++);
	// zero-extend GLA:
	for (int i = 0; i < kGLA; i++)
		*(pOut++) = 0;
	// copy rest of the file
	limit = pMapInFile + cbInFile.QuadPart;
	while (pIn < limit)
		*(pOut++) = *(pIn++);

	// finally, we fixup the output exe
	// NE hdr fixup
	WORD *pextMRT = (WORD *)(pMapOutFile + offNE + pOutNE->ne_imptab); // points to the start of the extended MRT area
	char *pextINT = pMapOutFile + offNE + pOutNE->ne_enttab + kMRT; // points to the start of the extended INT area
	WORD intOffExt = pOutNE->ne_enttab - pOutNE->ne_imptab; // offset to extended INT area first entry
	pOutNE->ne_enttab += kMRT + kINT;
	pOutNE->ne_cmod += cImports;
	pOutNE->ne_imptab += kMRT;
	pOutNE->ne_nrestab += kMRT + kINT;
	pOutNE->fastload_offset += (kMRT + kINT + kGLA) >> 4;
	pOutNE->ne_expver = 0x30A;
	// segment table fixup
	ne_segment_table_entry_s *pSegEntry = (ne_segment_table_entry_s *)(pMapOutFile + offNE + pOutNE->ne_segtab);
	for (int i = 0; i < pOutNE->ne_cseg; ++i)
		pSegEntry[i].seg_data_offset += (kMRT + kINT + kGLA) >> 4;
	// resource table fixup
	// first word is the shift, we assume 4 here
	resource_typeinfo_s *pResType = (resource_typeinfo_s *) (pMapOutFile + offNE + pOutNE->ne_rsrctab + 2);
	resource_nameinfo_s *pResName = (resource_nameinfo_s *) (pResType + 1);
	while (pResType->type_id) {
		int nres = pResType->count;
		int i;
		for (i = 0; i < nres; ++i) {
			pResName[i].offset += (kMRT + kINT + kGLA) >> 4;
		}
		pResType = (resource_typeinfo_s *)&pResName[i];
		pResName = (resource_nameinfo_s *)(pResType + 1);
	}
	// modref table extension
	// import name table extension
	char **pszImpName = newImports;
	++pextINT; // first character is the length, so we skip it
	while (*pszImpName) {
		char i;
		for (i = 0; (*pszImpName)[i] != 0; i++) {
			pextINT[i] = (*pszImpName)[i];
		}
		pextINT[-1] = i; // store size
		*pextMRT = intOffExt;
		// goto next
		intOffExt += i + 1;
		pextINT = pextINT + i + 1; 
		++pszImpName;
		++pextMRT;
	}

	// XXX: error checks missing
	UnmapViewOfFile(pMapInFile);
	CloseHandle(hmapInFile);
	CloseHandle(hInFile);
	UnmapViewOfFile(pMapOutFile);
	CloseHandle(hmapOutFile);
	CloseHandle(hOutFile);
	return 0;
}

int fixDeps(
	char *szFpInFile, 
	char *szFpOutFile,
	char *szFpDllFile,
	unsigned short oldMod, 
	unsigned short newMod)
{
	HANDLE hInFile, hmapInFile, hOutFile, hmapOutFile;
	char *pMapInFile, *pMapOutFile;
	PIMAGE_WIN31_HEADER pNE;
	int mzMRT, mzINT;
	LARGE_INTEGER cbInFile;

	if ((hInFile = CreateFileA(szFpInFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
		== INVALID_HANDLE_VALUE)
		return NEEDIT_NOTFOUND;
	if (!GetFileSizeEx(hInFile, &cbInFile))
		return NEEDIT_FILE_ERROR;
	if ((hmapInFile = CreateFileMapping(hInFile, NULL, PAGE_READONLY, 0, 0, 0)) == NULL
		|| (pMapInFile = (char *)MapViewOfFile(hmapInFile, FILE_MAP_READ, 0, 0, 0)) == NULL)
		return NEEDIT_MAPPING_ERROR;

	DWORD offNE = *((DWORD *)(pMapInFile + 0x3C));
	pNE = (PIMAGE_WIN31_HEADER)(pMapInFile + offNE);

	WORD cmod = pNE->ne_cmod;
	if (oldMod > cmod || newMod > cmod) {
		// FIXME: needs correct error code and close files
		UnmapViewOfFile(pMapInFile);
		CloseHandle(hmapInFile);
		CloseHandle(hInFile);
		return -1; // failure
	}

	mzINT = pNE->ne_imptab + offNE;
	mzMRT = pNE->ne_modtab + offNE;

	// CREATE OUTPUT FILE
	LARGE_INTEGER cbOutFile;
	cbOutFile.QuadPart = cbInFile.QuadPart;
	if ((hOutFile = CreateFileA(szFpOutFile, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL))
		== INVALID_HANDLE_VALUE)
		return NEEDIT_FILE_ERROR;
	if ((hmapOutFile = CreateFileMapping(hOutFile, NULL, PAGE_READWRITE, cbOutFile.HighPart, cbOutFile.LowPart, NULL)) == NULL
		|| (pMapOutFile = (char *)MapViewOfFile(hmapOutFile, FILE_MAP_WRITE, 0, 0, 0)) == NULL)
		return NEEDIT_MAPPING_ERROR;
	PIMAGE_WIN31_HEADER pOutNE = (PIMAGE_WIN31_HEADER)(pMapOutFile + offNE);

	// DO COPY
	// until INT (excluded)
	char *pIn = pMapInFile, *pOut = pMapOutFile;
	char *limit = pMapInFile + cbInFile.QuadPart;
	while (pIn < limit)
		*(pOut++) = *(pIn++);

	// segment relocations fixup

	// Create ordinal/exported name sets for the specified import
	WORD *modtab = (WORD *)(pMapInFile + offNE + pNE->ne_modtab);
	char *intab = (char *)(pMapInFile + offNE + pNE->ne_imptab);
	std::set<WORD> ordSet = std::set<WORD>();
	std::set<std::string> nameSet = std::set<std::string>(); 
	std::unordered_map<WORD, std::string> assocOrd 
		= std::unordered_map<WORD, std::string>(500);
	// make sets
	getExportedNames(szFpDllFile, &ordSet, &nameSet);
	// this was only needed to collect the names of functions that were not known in the old dll?!
	// getAssocOrd(szNewFile, assocOrd + i);

	/*
	 * for each segment:
	 *	find segstart and seglength:
	 *	obtain segfixups = segstart + seglength
	 *	obtain WORD cFixupEntries = file[segfixups]
	 *	for each Entry fixup = ((Entry *) (segfixups + 1) + i)
	 *	 by cases on the second byte of fixup (say, fixup->type)
	 *	 case 1 (imported ordinal):
	 *	  if (fixup->modref == oldImp && fixup->ordinal not in ordinals(fixup->modref))
	 *	   fixup->modref = newMod;
	 *	  break;
	 *	 case 2 (imported name):
	 *	  if (fixup->modref == oldImp && name(fixup->idxINT) not in names(fixup->modref))
	 *	   fixup->modref = newMod;
	 *    break;
	 *  next fixup;
	 * next segment;
	 */
	WORD offSegtab = pOutNE->ne_segtab;
	WORD cSeg = pOutNE->ne_cseg;
	ne_segment_table_entry_s *segentry =
		(ne_segment_table_entry_s *)(pMapOutFile + offNE + offSegtab);
	for (; cSeg > 0; cSeg--, segentry++) {
		BYTE *segment_fixup = (BYTE *)
			(pMapOutFile + (segentry->seg_data_offset << 4) + segentry->seg_data_length);
		WORD cFixupEntries = *((WORD *)segment_fixup);
		relocation_entry_s *fixup = (relocation_entry_s *)(segment_fixup + 2);
		for (; cFixupEntries > 0; cFixupEntries--, fixup++) {
			WORD wModule = fixup->target1;
			int cbImportName = 0;
			std::string importName;
			switch (fixup->relocation_type) {
			case 1: /* imported ordinal */
					/* fixup->target1 == module, fixup->target2 == ordinal */
				if (wModule == oldMod && ordSet.find(fixup->target2) == ordSet.end()) { /* if ordinal not in ordinals(module) */
					fixup->target1 = newMod;
					ppRedirect(oldMod, newMod, NULL, fixup->target2,
						pOutNE->ne_cseg - cSeg + 1, *((WORD *)segment_fixup) - cFixupEntries + 1);
				}
				break;

			case 2: /* imported name */
					/* fixup->target1 == module, fixup->target2 == offINT */
				cbImportName = intab[fixup->target2];
				importName = std::string(intab + fixup->target2 + 1, cbImportName);
				if (wModule == oldMod && nameSet.find(importName) == nameSet.end()) { /* name(idxINT) not in names(module) */
					fixup->target1 = newMod;
					ppRedirect(oldMod, newMod, importName.c_str(), 0,
						pOutNE->ne_cseg - cSeg + 1, *((WORD *)segment_fixup) - cFixupEntries + 1);
				}
				break;

			default:
				break;
			}
		}
	}

	std::cout << std::endl;

	// XXX: error checks missing
	UnmapViewOfFile(pMapInFile);
	CloseHandle(hmapInFile);
	CloseHandle(hInFile);
	UnmapViewOfFile(pMapOutFile);
	CloseHandle(hmapOutFile);
	CloseHandle(hOutFile);
	return 0;
}

void ppRedirect(
	WORD oldMod, 
	WORD newMod, 
	const char *szImpName, 
	WORD ordinal, 
	WORD wSeg, 
	WORD wFixup) {
	if (ordinal && szImpName)
		std::cout << "Redirecting " << szImpName << "@" << ordinal << ": "
			<< oldMod << " -> " << newMod;
	else if (ordinal)
		std::cout << "Redirecting " << "unnamed" << ordinal << "@" << ordinal << ": "
			<< oldMod << " -> " << newMod;
	else if (szImpName)
		std::cout << "Redirecting " << szImpName << ": " << oldMod << " -> " << newMod;
	// else: no ordinal and no name? can't happen, nothing to do.
	std::cout << " (segment " << wSeg << " fixup " << wFixup << ")" << std::endl;
}

int getExportedNames(
	char *szInFile, 
	std::set<WORD> *ordSet, 
	std::set<std::string> *nameSet)
{
	HANDLE hInFile, hmapInFile;
	BYTE *pMapInFile;
	PIMAGE_WIN31_HEADER pNE;
	int mzMRT, mzINT, mzET, mzRNT, mzNNT;
	LARGE_INTEGER cbInFile;

	if ((hInFile = CreateFileA(szInFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL))
		== INVALID_HANDLE_VALUE)
		return NEEDIT_NOTFOUND;
	if (!GetFileSizeEx(hInFile, &cbInFile))
		return NEEDIT_FILE_ERROR;
	if ((hmapInFile = CreateFileMapping(hInFile, NULL, PAGE_READONLY, 0, 0, 0)) == NULL
		|| (pMapInFile = (BYTE *)MapViewOfFile(hmapInFile, FILE_MAP_READ, 0, 0, 0)) == NULL)
		return NEEDIT_MAPPING_ERROR;
	DWORD offNE = *((DWORD *)(pMapInFile + 0x3C));
	pNE = (PIMAGE_WIN31_HEADER)(pMapInFile + offNE);

	mzINT = pNE->ne_imptab + offNE;
	mzET = pNE->ne_enttab + offNE;
	mzMRT = pNE->ne_modtab + offNE;
	mzRNT = pNE->ne_restab + offNE;
	mzNNT = pNE->ne_nrestab;

	WORD cbEnttab = pNE->ne_cbenttab;

	BYTE *pEntries = pMapInFile + mzET;

	// phase 1: collect exported ordinals
	// =====================================
	// while the entry table is not finished
	WORD curOrd = 1;
	while (*pEntries) {
		BYTE count = *pEntries; // #entries in this bundle
		// unused entries
		if (!pEntries[1]) {
			// we skip this bundle
			curOrd += count;
			pEntries += 2; // skip bundle header
			continue; // next bundle
		}
		int cbEntry = (pEntries[1] == 0xFF) ? 6 : 3;
		pEntries += 2; // skip bundle header
		for (; count > 0; count--, curOrd++, pEntries += cbEntry) {
			// if it is an exported entry
			if ((*pEntries) & 0x1)
				ordSet->insert(curOrd);
		}
	}

	BYTE *pResNames = pMapInFile + mzRNT;
	BYTE *pNResNames = pMapInFile + mzNNT;

	// phase 2: collect exported names
	// ================================
	// skip default descriptive strings
	pResNames += (*pResNames) + 3;
	pNResNames += (*pNResNames) + 3;

	// while the res name table is not finished
	while (*pResNames) {
		char cbName = *pResNames;
		WORD thisOrd = *((WORD *)(pResNames + cbName + 1));
		if (ordSet->find(thisOrd) != ordSet->end()) {
			nameSet->insert(std::string((char *)(pResNames + 1), cbName));
		}
		pResNames += cbName + 3;
	}
	// similarly for the nonres name table
	while (*pNResNames) {
		char cbName = *pNResNames;
		WORD thisOrd = *((WORD *)(pNResNames + cbName + 1));
		if (ordSet->find(thisOrd) != ordSet->end()) {
			nameSet->insert(std::string((char *)(pNResNames + 1), cbName));
		}
		pNResNames += cbName + 3;
	}

	// XXX: error checks missing
	UnmapViewOfFile(pMapInFile);
	CloseHandle(hmapInFile);
	CloseHandle(hInFile);
	return 0; // all is well
}