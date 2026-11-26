// Burn - hard disk image module (CHD via libchdr, or raw dumps)

#include "burnint.h"
#include "burn_hdd.h"
#include "chd.h"
#include <map>
#include <vector>
#include <chrono>
#include <new>
#include "burn_hdd_readahead.h"

#ifdef __LIBRETRO__
#include <streams/file_stream_transforms.h>
#define hddFseek	fseek
#define hddFtell	ftell
#else
#ifdef _MSC_VER
#define hddFseek	_fseeki64
#define hddFtell	_ftelli64
#else
#define hddFseek	fseeko
#define hddFtell	ftello
#endif
#endif

#ifdef _UNICODE
#define HDD_NARROW_FMT	_T("%hs")
#else
#define HDD_NARROW_FMT	_T("%s")
#endif

#define HDD_CACHE_HUNKS		8
#define HDD_DIFF_MAGIC		"FBNHDIFF"
#define HDD_DIFF_VERSION	1

struct BurnHDD {
	FILE* pFile;
	chd_file* pChd;					// NULL for raw dumps

	UINT32 nSectorSize;
	UINT32 nSectorCount;
	INT32 nCylinders, nHeads, nSectors;	// 0 when unknown

	// CHD hunk cache (direct mapped)
	UINT32 nHunkBytes;
	UINT32 nTotalHunks;
	UINT8* pCache;
	UINT8* pResident = NULL;
	INT32 nCacheTag[HDD_CACHE_HUNKS];
	// Statistics and predictor are enabled only by the Y2 driver.
	bool bReadStats = false;
	BurnHDDReadStats readStats = {};
	UINT32 lastHunk = ~0U;
	unsigned sequentialHunks = 0;
	TCHAR szImagePath[MAX_PATH];
#if defined(__linux__)
	FILE *pAheadFile = NULL;
	chd_file *pAheadChd = NULL;
	HddReadAhead::Worker *pAhead = NULL;
#endif

	std::map<UINT32, std::vector<UINT8> > Diff;
	bool bDiffDirty;
	TCHAR szDiffPath[MAX_PATH];
};

static void Put32(UINT8* p, UINT32 v)
{
	p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24;
}

static UINT32 Get32(const UINT8* p)
{
	return p[0] | (p[1] << 8) | (p[2] << 16) | ((UINT32)p[3] << 24);
}

// the driver gives the disk name without extension
static FILE* OpenInDir(const TCHAR* szDir, const char* szName, const TCHAR* szExt, TCHAR* szPathOut)
{
	if (szDir == NULL || szDir[0] == 0) return NULL;

	_stprintf(szPathOut, _T("%s%s/") HDD_NARROW_FMT _T("%s"), szAppHDDPath, szDir, szName, szExt);
	return _tfopen(szPathOut, _T("rb"));
}

static INT32 OpenChd(BurnHDD* pDisk)
{
	if (chd_open_file(pDisk->pFile, CHD_OPEN_READ, NULL, &pDisk->pChd) != CHDERR_NONE) {
		pDisk->pChd = NULL;
		return 1;
	}

	const chd_header* pHeader = chd_get_header(pDisk->pChd);
	if (pHeader == NULL || pHeader->hunkbytes == 0) return 1;

	char szMeta[256];
	UINT32 nMetaLen = 0;
	INT32 nCyls = 0, nHeads = 0, nSecs = 0, nBps = 0;

	if (chd_get_metadata(pDisk->pChd, HARD_DISK_METADATA_TAG, 0, szMeta, sizeof(szMeta) - 1, &nMetaLen, NULL, NULL) == CHDERR_NONE) {
		szMeta[nMetaLen < sizeof(szMeta) ? nMetaLen : sizeof(szMeta) - 1] = 0;
		if (sscanf(szMeta, HARD_DISK_METADATA_FORMAT, &nCyls, &nHeads, &nSecs, &nBps) != 4 || nBps <= 0 || pHeader->hunkbytes % nBps) {
			bprintf(PRINT_ERROR, _T("HDD: bad CHD geometry\n"));
			return 1;
		}
	} else {
		// raw CHD (chdman createraw, e.g. NAND dumps): its unit is the sector, no geometry
		if (pHeader->version < 5 || pHeader->unitbytes == 0 || pHeader->hunkbytes % pHeader->unitbytes) {
			bprintf(PRINT_ERROR, _T("HDD: the CHD is neither a hard disk image nor a raw image\n"));
			return 1;
		}
		nBps = pHeader->unitbytes;
	}

	pDisk->nCylinders = nCyls;
	pDisk->nHeads = nHeads;
	pDisk->nSectors = nSecs;
	pDisk->nSectorSize = nBps;
	pDisk->nSectorCount = (UINT32)(pHeader->logicalbytes / nBps);
	pDisk->nHunkBytes = pHeader->hunkbytes;
	pDisk->nTotalHunks = pHeader->totalhunks;

	pDisk->pCache = (UINT8*)malloc((size_t)pDisk->nHunkBytes * HDD_CACHE_HUNKS);
	if (pDisk->pCache == NULL) return 1;
	for (INT32 i = 0; i < HDD_CACHE_HUNKS; i++) pDisk->nCacheTag[i] = -1;

	return 0;
}

static INT32 OpenRaw(BurnHDD* pDisk)
{
	hddFseek(pDisk->pFile, 0, SEEK_END);
	INT64 nSize = (INT64)hddFtell(pDisk->pFile);
	hddFseek(pDisk->pFile, 0, SEEK_SET);

	pDisk->nSectorSize = 512;
	pDisk->nSectorCount = (UINT32)(nSize / 512);
	return (pDisk->nSectorCount == 0);
}

static void LoadDiff(BurnHDD* pDisk)
{
	FILE* fp = _tfopen(pDisk->szDiffPath, _T("rb"));
	if (fp == NULL) return;

	UINT8 hdr[20];
	if (fread(hdr, sizeof(hdr), 1, fp) == 1 && memcmp(hdr, HDD_DIFF_MAGIC, 8) == 0 &&
		Get32(hdr + 8) == HDD_DIFF_VERSION && Get32(hdr + 12) == pDisk->nSectorSize) {
		UINT32 nEntries = Get32(hdr + 16);
		std::vector<UINT8> entry(4 + pDisk->nSectorSize);

		for (UINT32 i = 0; i < nEntries; i++) {
			if (fread(&entry[0], entry.size(), 1, fp) != 1) break;
			UINT32 nLba = Get32(&entry[0]);
			if (nLba >= pDisk->nSectorCount) continue;
			pDisk->Diff[nLba].assign(entry.begin() + 4, entry.end());
		}

		bprintf(PRINT_NORMAL, _T("HDD: %d modified sectors loaded from %s\n"), (INT32)pDisk->Diff.size(), pDisk->szDiffPath);
	} else {
		bprintf(PRINT_ERROR, _T("HDD: %s ignored (not made for this disk)\n"), pDisk->szDiffPath);
	}

	fclose(fp);
}

INT32 BurnHDDGetPath(INT32 nIndex, const TCHAR* pszExt, TCHAR* pszPath)
{
	char* szName = NULL;
	if (BurnDrvGetHDDName(&szName, nIndex, 0) || szName == NULL) return 1;

	FILE* fp = OpenInDir(BurnDrvGetText(DRV_NAME), szName, pszExt, pszPath);
	if (fp == NULL) fp = OpenInDir(BurnDrvGetText(DRV_PARENT), szName, pszExt, pszPath);
	if (fp == NULL) return 1;

	fclose(fp);
	return 0;
}

BurnHDD* BurnHDDOpen(INT32 nIndex)
{
	char* szName = NULL;
	if (BurnDrvGetHDDName(&szName, nIndex, 0) || szName == NULL) return NULL;

	static const TCHAR* szExts[] = { _T(".chd"), _T(".img") };

	TCHAR szPath[MAX_PATH];
	FILE* fp = NULL;
	for (INT32 i = 0; i < 2 && fp == NULL; i++) {
		fp = OpenInDir(BurnDrvGetText(DRV_NAME), szName, szExts[i], szPath);
		if (fp == NULL) fp = OpenInDir(BurnDrvGetText(DRV_PARENT), szName, szExts[i], szPath);
	}
	// a single disk may also be named after the romset (<romset>/<romset>.chd)
	for (INT32 i = 0; i < 2 && fp == NULL && nIndex == 0 && BurnDrvGetTextA(DRV_NAME); i++) {
		fp = OpenInDir(BurnDrvGetText(DRV_NAME), BurnDrvGetTextA(DRV_NAME), szExts[i], szPath);
	}
	if (fp == NULL) {
		bprintf(PRINT_ERROR, _T("HDD: ") HDD_NARROW_FMT _T(".chd not found\n"), szName);
		return NULL;
	}

	BurnHDD* pDisk = new BurnHDD;
	pDisk->pFile = fp;
	pDisk->pChd = NULL;
	pDisk->nCylinders = pDisk->nHeads = pDisk->nSectors = 0;
	pDisk->nHunkBytes = pDisk->nTotalHunks = 0;
	pDisk->pCache = NULL;
	pDisk->bDiffDirty = false;
	_tcscpy(pDisk->szImagePath, szPath);

	// a CHD is recognised by its signature, whatever the file extension
	char sig[8] = { 0 };
	bool bChd = (fread(sig, 8, 1, fp) == 1 && memcmp(sig, "MComprHD", 8) == 0);
	hddFseek(fp, 0, SEEK_SET);

	if (bChd ? OpenChd(pDisk) : OpenRaw(pDisk)) {
		bprintf(PRINT_ERROR, _T("HDD: can't use %s\n"), szPath);
		if (pDisk->pChd) chd_close(pDisk->pChd);
		fclose(fp);
		free(pDisk->pCache);
		delete pDisk;
		return NULL;
	}

	if (nIndex == 0) {
		_stprintf(pDisk->szDiffPath, _T("%s%s.diff"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME));
	} else {
		_stprintf(pDisk->szDiffPath, _T("%s%s_%d.diff"), szAppEEPROMPath, BurnDrvGetText(DRV_NAME), nIndex);
	}
	LoadDiff(pDisk);

	bprintf(PRINT_NORMAL, _T("HDD: %s, %s, %d sectors of %d bytes\n"), szPath, bChd ? _T("CHD") : _T("raw"),
		pDisk->nSectorCount, pDisk->nSectorSize);

	return pDisk;
}

INT32 BurnHDDFlush(BurnHDD* pDisk)
{
	if (pDisk == NULL) return 1;
	if (!pDisk->bDiffDirty) return 0;

	FILE* fp = _tfopen(pDisk->szDiffPath, _T("wb"));
	if (fp == NULL) {
		bprintf(PRINT_ERROR, _T("HDD: can't write %s\n"), pDisk->szDiffPath);
		return 1;
	}

	UINT8 hdr[20];
	memcpy(hdr, HDD_DIFF_MAGIC, 8);
	Put32(hdr + 8, HDD_DIFF_VERSION);
	Put32(hdr + 12, pDisk->nSectorSize);
	Put32(hdr + 16, (UINT32)pDisk->Diff.size());
	INT32 nRet = (fwrite(hdr, sizeof(hdr), 1, fp) != 1);

	UINT8 lba[4];
	for (std::map<UINT32, std::vector<UINT8> >::iterator it = pDisk->Diff.begin(); it != pDisk->Diff.end() && !nRet; ++it) {
		Put32(lba, it->first);
		nRet |= (fwrite(lba, 4, 1, fp) != 1);
		nRet |= (fwrite(&it->second[0], pDisk->nSectorSize, 1, fp) != 1);
	}

	fclose(fp);
	if (nRet == 0) pDisk->bDiffDirty = false;
	return nRet;
}

#if defined(__linux__)
static bool ReadAheadHunk(void *ctx,uint32_t hunk,uint8_t *dest)
{
	return chd_read(static_cast<chd_file*>(ctx),hunk,dest)==CHDERR_NONE;
}
#endif

bool BurnHDDConfigureReadAhead(BurnHDD* pDisk, bool enable)
{
	if(!pDisk)return false;
	pDisk->bReadStats=true;
	if(!enable)return false;
#if defined(__linux__)
	if(!pDisk->pChd || pDisk->pResident || !pDisk->Diff.empty())return false;
	if(pDisk->pAhead)return true;
	// Independent handle and libchdr decoder, not a shared seek cursor.
	pDisk->pAheadFile=_tfopen(pDisk->szImagePath,_T("rb"));
	if(!pDisk->pAheadFile)return false;
	const chd_error err=chd_open_file(pDisk->pAheadFile,CHD_OPEN_READ,NULL,&pDisk->pAheadChd);
	if(err!=CHDERR_NONE)pDisk->pAheadChd=NULL;
	if(err==CHDERR_NONE) {
		const chd_header *h=chd_get_header(pDisk->pAheadChd);
		const chd_header *orig=chd_get_header(pDisk->pChd);
		if(h && orig && h->hunkbytes==pDisk->nHunkBytes && h->totalhunks==pDisk->nTotalHunks &&
			h->logicalbytes==orig->logicalbytes && !memcmp(h->sha1,orig->sha1,sizeof(h->sha1))) {
			pDisk->pAhead=new(std::nothrow) HddReadAhead::Worker;
			if(pDisk->pAhead && pDisk->pAhead->Start(pDisk->nHunkBytes,ReadAheadHunk,pDisk->pAheadChd))return true;
			delete pDisk->pAhead;pDisk->pAhead=NULL;
		}
	}
	if(pDisk->pAheadChd)chd_close(pDisk->pAheadChd);
	pDisk->pAheadChd=NULL;
	fclose(pDisk->pAheadFile);pDisk->pAheadFile=NULL;
#endif
	return false;
}

UINT64 BurnHDDResidentSize(BurnHDD *pDisk)
{
	return pDisk && pDisk->pChd ? UINT64(pDisk->nHunkBytes)*pDisk->nTotalHunks : 0;
}

bool BurnHDDPreload(BurnHDD *pDisk,UINT64 budget)
{
	const UINT64 bytes=BurnHDDResidentSize(pDisk);
	if(!bytes || bytes>budget || bytes>SIZE_MAX)return false;
	if(pDisk->pResident)return true;
#if defined(__linux__)
	// Do not populate while a second decoder is actively speculating.
	if(pDisk->pAhead)return false;
#endif
	UINT8 *data=(UINT8*)malloc(size_t(bytes));
	if(!data)return false;
	for(UINT32 h=0;h<pDisk->nTotalHunks;h++){
		if(chd_read(pDisk->pChd,h,data+size_t(h)*pDisk->nHunkBytes)!=CHDERR_NONE){
			free(data);return false;
		}
	}
	// Publish only after every hunk was decoded successfully.
	pDisk->pResident=data;
	return true;
}

void BurnHDDDropResident(BurnHDD *pDisk)
{
	if(!pDisk)return;
	free(pDisk->pResident);pDisk->pResident=NULL;
}

bool BurnHDDIsResident(BurnHDD *pDisk)
{
	return pDisk && pDisk->pResident;
}

void BurnHDDGetReadStats(BurnHDD* pDisk,BurnHDDReadStats *stats)
{
	*stats={};
	if(!pDisk)return;
	*stats=pDisk->readStats;
#if defined(__linux__)
	if(pDisk->pAhead){
		const auto s=pDisk->pAhead->GetStats();
		stats->readAheadIssued=s.issued;stats->readAheadHits=s.hits;
		stats->readAheadWaited=s.waited;stats->readAheadDiscarded=s.discarded;
		stats->readAheadFailed=s.failed;
	}
#endif
}

void BurnHDDClose(BurnHDD* pDisk)
{
	if (pDisk == NULL) return;

	BurnHDDFlush(pDisk);
#if defined(__linux__)
	// Join the bounded speculative decoder before releasing either image.
	delete pDisk->pAhead;
	if(pDisk->pAheadChd)chd_close(pDisk->pAheadChd);
	if(pDisk->pAheadFile)fclose(pDisk->pAheadFile);
#endif

	if (pDisk->pChd) chd_close(pDisk->pChd);
	if (pDisk->pFile) fclose(pDisk->pFile);		// libchdr does not close a FILE* it was given
	free(pDisk->pCache);
	BurnHDDDropResident(pDisk);
	delete pDisk;
}

UINT32 BurnHDDGetSectorSize(BurnHDD* pDisk)
{
	return pDisk ? pDisk->nSectorSize : 0;
}

UINT64 BurnHDDGetSize(BurnHDD* pDisk)
{
	return pDisk ? (UINT64)pDisk->nSectorCount * pDisk->nSectorSize : 0;
}

UINT32 BurnHDDGetSectorCount(BurnHDD* pDisk)
{
	return pDisk ? pDisk->nSectorCount : 0;
}

INT32 BurnHDDGetGeometry(BurnHDD* pDisk, INT32* pnCylinders, INT32* pnHeads, INT32* pnSectors)
{
	if (pDisk == NULL || pDisk->nCylinders == 0) return 1;

	if (pnCylinders) *pnCylinders = pDisk->nCylinders;
	if (pnHeads) *pnHeads = pDisk->nHeads;
	if (pnSectors) *pnSectors = pDisk->nSectors;
	return 0;
}

static UINT8 *ReadImageHunk(BurnHDD *pDisk,UINT32 hunk)
{
	if(hunk>=pDisk->nTotalHunks)return NULL;
	if(pDisk->pResident){
		if(pDisk->bReadStats){pDisk->readStats.residentReads++;pDisk->readStats.residentBytes+=pDisk->nHunkBytes;}
		return pDisk->pResident+size_t(hunk)*pDisk->nHunkBytes;
	}
	const INT32 slot=hunk%HDD_CACHE_HUNKS;
	UINT8 *data=pDisk->pCache+(size_t)slot*pDisk->nHunkBytes;
	if(pDisk->nCacheTag[slot]!=(INT32)hunk){
		const auto start=pDisk->bReadStats?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};
		bool hit=false;
#if defined(__linux__)
		if(pDisk->pAhead)hit=pDisk->pAhead->Take(hunk,data);
#endif
		bool ok=hit || chd_read(pDisk->pChd,hunk,data)==CHDERR_NONE;
		if(pDisk->bReadStats){
			if(!hit)pDisk->readStats.demandHunks++;
			UINT64 ns=(UINT64)std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count();
			pDisk->readStats.demandNs+=ns;
			if(ns>pDisk->readStats.peakNs)pDisk->readStats.peakNs=ns;
		}
		if(!ok){pDisk->nCacheTag[slot]=-1;return NULL;}
		pDisk->nCacheTag[slot]=hunk;
	}
#if defined(__linux__)
	if(pDisk->pAhead && pDisk->lastHunk!=hunk){
		// Require three distinct sequential hunks before speculation.
		// Do not treat one short range crossing a hunk as a stream.
		if(pDisk->lastHunk!=~0U && hunk==pDisk->lastHunk+1){
			if(pDisk->sequentialHunks<3)pDisk->sequentialHunks++;
		}else pDisk->sequentialHunks=1;
		if(pDisk->sequentialHunks>=3 && hunk+1<pDisk->nTotalHunks &&
			pDisk->nCacheTag[(hunk+1)%HDD_CACHE_HUNKS]!=(INT32)(hunk+1))
			pDisk->pAhead->Offer(hunk+1);
		pDisk->lastHunk=hunk;
	}
#endif
	return data;
}

static INT32 ReadImageSector(BurnHDD* pDisk, UINT32 nLba, UINT8* pDest)
{
	if (pDisk->pChd == NULL) {
		if (hddFseek(pDisk->pFile, (INT64)nLba * pDisk->nSectorSize, SEEK_SET) != 0) return 1;
		return (fread(pDest, pDisk->nSectorSize, 1, pDisk->pFile) != 1);
	}
	const UINT64 offset=(UINT64)nLba*pDisk->nSectorSize;
	UINT8 *hunk=ReadImageHunk(pDisk,(UINT32)(offset/pDisk->nHunkBytes));
	if(!hunk)return 1;
	memcpy(pDest,hunk+(offset%pDisk->nHunkBytes),pDisk->nSectorSize);
	return 0;
}

INT32 BurnHDDRead(BurnHDD* pDisk, UINT32 nLba, UINT32 nCount, void* pDest)
{
	if (pDisk == NULL || pDest == NULL) return 1;

	UINT8* pDst = (UINT8*)pDest;
	INT32 nRet = 0;

	for (UINT32 i = 0; i < nCount; i++, nLba++, pDst += pDisk->nSectorSize) {
		std::map<UINT32, std::vector<UINT8> >::iterator it = pDisk->Diff.find(nLba);
		if (it != pDisk->Diff.end()) {
			memcpy(pDst, &it->second[0], pDisk->nSectorSize);
			continue;
		}

		if (nLba >= pDisk->nSectorCount || ReadImageSector(pDisk, nLba, pDst)) {
			memset(pDst, 0, pDisk->nSectorSize);
			nRet = 1;
		}
	}

	return nRet;
}

INT32 BurnHDDReadBytes(BurnHDD* pDisk, UINT64 nOffset, UINT32 nSize, void* pDest)
{
	if (pDisk == NULL || pDest == NULL) return 1;

	UINT8* pDst = (UINT8*)pDest;
	UINT64 nDiskSize = (UINT64)pDisk->nSectorCount * pDisk->nSectorSize;
	INT32 nRet = 0;

	if (nOffset >= nDiskSize) {
		memset(pDst, 0, nSize);
		return 1;
	}
	if (nSize > nDiskSize - nOffset) {
		UINT32 nInside = (UINT32)(nDiskSize - nOffset);
		memset(pDst + nInside, 0, nSize - nInside);
		nSize = nInside;
		nRet = 1;
	}

	// raw dump with nothing written: one read, whatever the alignment
	if (pDisk->pChd == NULL && pDisk->Diff.empty()) {
		if (hddFseek(pDisk->pFile, (INT64)nOffset, SEEK_SET) == 0 && fread(pDst, nSize, 1, pDisk->pFile) == 1) return nRet;
		memset(pDst, 0, nSize);
		return 1;
	}

	if(pDisk->pResident && pDisk->Diff.empty()){
		memcpy(pDst,pDisk->pResident+size_t(nOffset),nSize);
		if(pDisk->bReadStats){pDisk->readStats.residentReads++;pDisk->readStats.residentBytes+=nSize;}
		return nRet;
	}

	// Read immutable CHD byte ranges directly from the hunk cache, without
	// allocating a temporary sector or traversing the empty diff map.
	// A failed hunk falls back to the original sector/error handling.
	if (pDisk->bReadStats && pDisk->pChd && pDisk->Diff.empty()) {
		while(nSize) {
			const UINT32 pos=(UINT32)(nOffset%pDisk->nHunkBytes);
			UINT32 len=pDisk->nHunkBytes-pos;
			if(len>nSize)len=nSize;
			UINT8 *hunk=ReadImageHunk(pDisk,(UINT32)(nOffset/pDisk->nHunkBytes));
			if(!hunk)break;
			memcpy(pDst,hunk+pos,len);
			pDst+=len;nOffset+=len;nSize-=len;
		}
		if(!nSize)return nRet;
	}

	std::vector<UINT8> sector(pDisk->nSectorSize);

	while (nSize) {
		UINT32 nLba = (UINT32)(nOffset / pDisk->nSectorSize);
		UINT32 nPos = (UINT32)(nOffset % pDisk->nSectorSize);
		UINT32 nLen = pDisk->nSectorSize - nPos;
		if (nLen > nSize) nLen = nSize;

		if (nLen == pDisk->nSectorSize) {
			nRet |= BurnHDDRead(pDisk, nLba, 1, pDst);
		} else {
			nRet |= BurnHDDRead(pDisk, nLba, 1, &sector[0]);
			memcpy(pDst, &sector[nPos], nLen);
		}

		pDst += nLen;
		nOffset += nLen;
		nSize -= nLen;
	}

	return nRet;
}

INT32 BurnHDDWrite(BurnHDD* pDisk, UINT32 nLba, UINT32 nCount, const void* pSrc)
{
	if (pDisk == NULL || pSrc == NULL) return 1;

	const UINT8* pS = (const UINT8*)pSrc;
	INT32 nRet = 0;

	for (UINT32 i = 0; i < nCount; i++, nLba++, pS += pDisk->nSectorSize) {
		if (nLba >= pDisk->nSectorCount) {
			nRet = 1;
			continue;
		}
		pDisk->Diff[nLba].assign(pS, pS + pDisk->nSectorSize);
		pDisk->bDiffDirty = true;
	}

	return nRet;
}

void BurnHDDDiscardWrites(BurnHDD* pDisk)
{
	if (pDisk == NULL) return;

	pDisk->Diff.clear();
	pDisk->bDiffDirty = true;	// an empty diff file replaces the old one
	BurnHDDFlush(pDisk);
}

INT32 BurnHDDGetMetadata(BurnHDD* pDisk, UINT32 nTag, UINT32 nIndex, void* pDest, UINT32 nLen, UINT32* pnResultLen)
{
	if (pnResultLen) *pnResultLen = 0;
	if (pDisk == NULL || pDisk->pChd == NULL) return 1;

	UINT32 nResultLen = 0;
	if (chd_get_metadata(pDisk->pChd, nTag, nIndex, pDest, nLen, &nResultLen, NULL, NULL) != CHDERR_NONE) return 1;

	if (pnResultLen) *pnResultLen = nResultLen;
	return 0;
}
