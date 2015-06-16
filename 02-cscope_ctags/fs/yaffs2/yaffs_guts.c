/*
 * YAFFS: Yet Another Flash File System. A NAND-flash specific file system.
 *
 * Copyright (C) 2002-2007 Aleph One Ltd.
 *   for Toby Churchill Ltd and Brightstar Engineering
 *
 * Created by Charles Manning <charles@aleph1.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

const char *yaffs_guts_c_version =
    "$Id: yaffs_guts.c,v 1.101 2009-12-25 01:53:05 charles Exp $";

#include "yportenv.h"

#include "yaffsinterface.h"
#include "yaffs_guts.h"
#include "yaffs_tagsvalidity.h"
#include "yaffs_getblockinfo.h"

#include "yaffs_tagscompat.h"
#ifndef CONFIG_YAFFS_USE_OWN_SORT
#include "yaffs_qsort.h"
#endif
#include "yaffs_nand.h"

#include "yaffs_checkptrw.h"

#include "yaffs_nand.h"
#include "yaffs_packedtags2.h"


#define YAFFS_PASSIVE_GC_CHUNKS 2

#include "yaffs_ecc.h"


/* Robustification (if it ever comes about...) */
static void yaffs_RetireBlock(yaffs_Device *dev, int blockInNAND);
static void yaffs_HandleWriteChunkError(yaffs_Device *dev, int chunkInNAND,
		int erasedOk);
static void yaffs_HandleWriteChunkOk(yaffs_Device *dev, int chunkInNAND,
				const __u8 *data,
				const yaffs_ExtendedTags *tags);
static void yaffs_HandleUpdateChunk(yaffs_Device *dev, int chunkInNAND,
				const yaffs_ExtendedTags *tags);

/* Other local prototypes */
static void yaffs_UpdateParent(yaffs_Object *obj);
static int yaffs_UnlinkObject(yaffs_Object *obj);
static int yaffs_ObjectHasCachedWriteData(yaffs_Object *obj);

static void yaffs_HardlinkFixup(yaffs_Device *dev, yaffs_Object *hardList);

static int yaffs_WriteNewChunkWithTagsToNAND(yaffs_Device *dev,
					const __u8 *buffer,
					yaffs_ExtendedTags *tags,
					int useReserve);
static int yaffs_PutChunkIntoFile(yaffs_Object *in, int chunkInInode,
				int chunkInNAND, int inScan);

static yaffs_Object *yaffs_CreateNewObject(yaffs_Device *dev, int number,
					yaffs_ObjectType type);
static void yaffs_AddObjectToDirectory(yaffs_Object *directory,
				yaffs_Object *obj);
static int yaffs_UpdateObjectHeader(yaffs_Object *in, const YCHAR *name,
				int force, int isShrink, int shadows);
static void yaffs_RemoveObjectFromDirectory(yaffs_Object *obj);
static int yaffs_CheckStructures(void);
static int yaffs_DeleteWorker(yaffs_Object *in, yaffs_Tnode *tn, __u32 level,
			int chunkOffset, int *limit);
static int yaffs_DoGenericObjectDeletion(yaffs_Object *in);

static yaffs_BlockInfo *yaffs_GetBlockInfo(yaffs_Device *dev, int blockNo);


static int yaffs_CheckChunkErased(struct yaffs_DeviceStruct *dev,
				int chunkInNAND);

static int yaffs_UnlinkWorker(yaffs_Object *obj);

static int yaffs_TagsMatch(const yaffs_ExtendedTags *tags, int objectId,
			int chunkInObject);

static int yaffs_AllocateChunk(yaffs_Device *dev, int useReserve,
				yaffs_BlockInfo **blockUsedPtr);

static void yaffs_VerifyFreeChunks(yaffs_Device *dev);

static void yaffs_CheckObjectDetailsLoaded(yaffs_Object *in);

static void yaffs_VerifyDirectory(yaffs_Object *directory);
#ifdef YAFFS_PARANOID
static int yaffs_CheckFileSanity(yaffs_Object *in);
#else
#define yaffs_CheckFileSanity(in)
#endif

static void yaffs_InvalidateWholeChunkCache(yaffs_Object *in);
static void yaffs_InvalidateChunkCache(yaffs_Object *object, int chunkId);

static void yaffs_InvalidateCheckpoint(yaffs_Device *dev);

static int yaffs_FindChunkInFile(yaffs_Object *in, int chunkInInode,
				yaffs_ExtendedTags *tags);

static __u32 yaffs_GetChunkGroupBase(yaffs_Device *dev, yaffs_Tnode *tn,
		unsigned pos);
static yaffs_Tnode *yaffs_FindLevel0Tnode(yaffs_Device *dev,
					yaffs_FileStructure *fStruct,
					__u32 chunkId);

static void yaffs_SkipRestOfBlock(yaffs_Device *dev);
static int yaffs_VerifyChunkWritten(yaffs_Device *dev,
					int chunkInNAND,
					const __u8 *data,
					yaffs_ExtendedTags *tags);

/* Function to calculate chunk and offset */

static void yaffs_AddrToChunk(yaffs_Device *dev, loff_t addr, int *chunkOut,
		__u32 *offsetOut)
{
	int chunk;
	__u32 offset;

	chunk  = (__u32)(addr >> dev->chunkShift);

	if (dev->chunkDiv == 1) {
		/* easy power of 2 case */
		offset = (__u32)(addr & dev->chunkMask);
	} else {
		/* Non power-of-2 case */

		loff_t chunkBase;

		chunk /= dev->chunkDiv;

		chunkBase = ((loff_t)chunk) * dev->nDataBytesPerChunk;
		offset = (__u32)(addr - chunkBase);
	}

	*chunkOut = chunk;
	*offsetOut = offset;
}

/* Function to return the number of shifts for a power of 2 greater than or
 * equal to the given number
 * Note we don't try to cater for all possible numbers and this does not have to
 * be hellishly efficient.
 */

static __u32 ShiftsGE(__u32 x)
{
	int extraBits;
	int nShifts;

	nShifts = extraBits = 0;

	while (x > 1) {
		if (x & 1)
			extraBits++;
		x >>= 1;
		nShifts++;
	}

	if (extraBits)
		nShifts++;

	return nShifts;
}

/* Function to return the number of shifts to get a 1 in bit 0
 */

static __u32 Shifts(__u32 x)
{
	int nShifts;

	nShifts =  0;

	if (!x)
		return 0;

	while (!(x&1)) {
		x >>= 1;
		nShifts++;
	}

	return nShifts;
}



/*
 * Temporary buffer manipulations.
 */

static int yaffs_InitialiseTempBuffers(yaffs_Device *dev)
{
	int i;
	__u8 *buf = (__u8 *)1;

	memset(dev->tempBuffer, 0, sizeof(dev->tempBuffer));

	for (i = 0; buf && i < YAFFS_N_TEMP_BUFFERS; i++) {
		dev->tempBuffer[i].line = 0;	/* not in use */
		dev->tempBuffer[i].buffer = buf =
		    YMALLOC_DMA(dev->totalBytesPerChunk);
	}

	return buf ? YAFFS_OK : YAFFS_FAIL;
}

__u8 *yaffs_GetTempBuffer(yaffs_Device *dev, int lineNo)
{
	int i, j;

	dev->tempInUse++;
	if (dev->tempInUse > dev->maxTemp)
		dev->maxTemp = dev->tempInUse;

	for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++) {
		if (dev->tempBuffer[i].line == 0) {
			dev->tempBuffer[i].line = lineNo;
			if ((i + 1) > dev->maxTemp) {
				dev->maxTemp = i + 1;
				for (j = 0; j <= i; j++)
					dev->tempBuffer[j].maxLine =
					    dev->tempBuffer[j].line;
			}

			return dev->tempBuffer[i].buffer;
		}
	}

	T(YAFFS_TRACE_BUFFERS,
	  (TSTR("Out of temp buffers at line %d, other held by lines:"),
	   lineNo));
	for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++)
		T(YAFFS_TRACE_BUFFERS, (TSTR(" %d "), dev->tempBuffer[i].line));

	T(YAFFS_TRACE_BUFFERS, (TSTR(" " TENDSTR)));

	/*
	 * If we got here then we have to allocate an unmanaged one
	 * This is not good.
	 */

	dev->unmanagedTempAllocations++;
	return YMALLOC(dev->nDataBytesPerChunk);

}

void yaffs_ReleaseTempBuffer(yaffs_Device *dev, __u8 *buffer,
				    int lineNo)
{
	int i;

	dev->tempInUse--;

	for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++) {
		if (dev->tempBuffer[i].buffer == buffer) {
			dev->tempBuffer[i].line = 0;
			return;
		}
	}

	if (buffer) {
		/* assume it is an unmanaged one. */
		T(YAFFS_TRACE_BUFFERS,
		  (TSTR("Releasing unmanaged temp buffer in line %d" TENDSTR),
		   lineNo));
		YFREE(buffer);
		dev->unmanagedTempDeallocations++;
	}

}

/*
 * Determine if we have a managed buffer.
 */
int yaffs_IsManagedTempBuffer(yaffs_Device *dev, const __u8 *buffer)
{
	int i;

	for (i = 0; i < YAFFS_N_TEMP_BUFFERS; i++) {
		if (dev->tempBuffer[i].buffer == buffer)
			return 1;
	}

	for (i = 0; i < dev->nShortOpCaches; i++) {
		if (dev->srCache[i].data == buffer)
			return 1;
	}

	if (buffer == dev->checkpointBuffer)
		return 1;

	T(YAFFS_TRACE_ALWAYS,
		(TSTR("yaffs: unmaged buffer detected.\n" TENDSTR)));
	return 0;
}



/*
 * Chunk bitmap manipulations
 */

static Y_INLINE __u8 *yaffs_BlockBits(yaffs_Device *dev, int blk)
{
	if (blk < dev->internalStartBlock || blk > dev->internalEndBlock) {
		T(YAFFS_TRACE_ERROR,
			(TSTR("**>> yaffs: BlockBits block %d is not valid" TENDSTR),
			blk));
		YBUG();
	}
	return dev->chunkBits +
		(dev->chunkBitmapStride * (blk - dev->internalStartBlock));
}

static Y_INLINE void yaffs_VerifyChunkBitId(yaffs_Device *dev, int blk, int chunk)
{
	if (blk < dev->internalStartBlock || blk > dev->internalEndBlock ||
			chunk < 0 || chunk >= dev->nChunksPerBlock) {
		T(YAFFS_TRACE_ERROR,
		(TSTR("**>> yaffs: Chunk Id (%d:%d) invalid"TENDSTR),
			blk, chunk));
		YBUG();
	}
}

static Y_INLINE void yaffs_ClearChunkBits(yaffs_Device *dev, int blk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);

	memset(blkBits, 0, dev->chunkBitmapStride);
}

static Y_INLINE void yaffs_ClearChunkBit(yaffs_Device *dev, int blk, int chunk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);

	yaffs_VerifyChunkBitId(dev, blk, chunk);

	blkBits[chunk / 8] &= ~(1 << (chunk & 7));
}

static Y_INLINE void yaffs_SetChunkBit(yaffs_Device *dev, int blk, int chunk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);

	yaffs_VerifyChunkBitId(dev, blk, chunk);

	blkBits[chunk / 8] |= (1 << (chunk & 7));
}

static Y_INLINE int yaffs_CheckChunkBit(yaffs_Device *dev, int blk, int chunk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);
	yaffs_VerifyChunkBitId(dev, blk, chunk);

	return (blkBits[chunk / 8] & (1 << (chunk & 7))) ? 1 : 0;
}

static Y_INLINE int yaffs_StillSomeChunkBits(yaffs_Device *dev, int blk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);
	int i;
	for (i = 0; i < dev->chunkBitmapStride; i++) {
		if (*blkBits)
			return 1;
		blkBits++;
	}
	return 0;
}

static int yaffs_CountChunkBits(yaffs_Device *dev, int blk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);
	int i;
	int n = 0;
	for (i = 0; i < dev->chunkBitmapStride; i++) {
		__u8 x = *blkBits;
		while (x) {
			if (x & 1)
				n++;
			x >>= 1;
		}

		blkBits++;
	}
	return n;
}

/*
 * Verification code
 */

static int yaffs_SkipVerification(yaffs_Device *dev)
{
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY | YAFFS_TRACE_VERIFY_FULL));
}

static int yaffs_SkipFullVerification(yaffs_Device *dev)
{
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY_FULL));
}

static int yaffs_SkipNANDVerification(yaffs_Device *dev)
{
	return !(yaffs_traceMask & (YAFFS_TRACE_VERIFY_NAND));
}

static const char *blockStateName[] = {
"Unknown",
"Needs scanning",
"Scanning",
"Empty",
"Allocating",
"Full",
"Dirty",
"Checkpoint",
"Collecting",
"Dead"
};

static void yaffs_VerifyBlock(yaffs_Device *dev, yaffs_BlockInfo *bi, int n)
{
	int actuallyUsed;
	int inUse;

	if (yaffs_SkipVerification(dev))
		return;

	/* Report illegal runtime states */
	if (bi->blockState >= YAFFS_NUMBER_OF_BLOCK_STATES)
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has undefined state %d"TENDSTR), n, bi->blockState));

	switch (bi->blockState) {
	case YAFFS_BLOCK_STATE_UNKNOWN:
	case YAFFS_BLOCK_STATE_SCANNING:
	case YAFFS_BLOCK_STATE_NEEDS_SCANNING:
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has bad run-state %s"TENDSTR),
		n, blockStateName[bi->blockState]));
	}

	/* Check pages in use and soft deletions are legal */

	actuallyUsed = bi->pagesInUse - bi->softDeletions;

	if (bi->pagesInUse < 0 || bi->pagesInUse > dev->nChunksPerBlock ||
	   bi->softDeletions < 0 || bi->softDeletions > dev->nChunksPerBlock ||
	   actuallyUsed < 0 || actuallyUsed > dev->nChunksPerBlock)
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has illegal values pagesInUsed %d softDeletions %d"TENDSTR),
		n, bi->pagesInUse, bi->softDeletions));


	/* Check chunk bitmap legal */
	inUse = yaffs_CountChunkBits(dev, n);
	if (inUse != bi->pagesInUse)
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has inconsistent values pagesInUse %d counted chunk bits %d"TENDSTR),
			n, bi->pagesInUse, inUse));

	/* Check that the sequence number is valid.
	 * Ten million is legal, but is very unlikely
	 */
	if (dev->isYaffs2 &&
	   (bi->blockState == YAFFS_BLOCK_STATE_ALLOCATING || bi->blockState == YAFFS_BLOCK_STATE_FULL) &&
	   (bi->sequenceNumber < YAFFS_LOWEST_SEQUENCE_NUMBER || bi->sequenceNumber > 10000000))
		T(YAFFS_TRACE_VERIFY, (TSTR("Block %d has suspect sequence number of %d"TENDSTR),
		n, bi->sequenceNumber));
}

static void yaffs_VerifyCollectedBlock(yaffs_Device *dev, yaffs_BlockInfo *bi,
		int n)
{
	yaffs_VerifyBlock(dev, bi, n);

	/* After collection the block should be in the erased state */
	/* This will need to change if we do partial gc */

	if (bi->blockState != YAFFS_BLOCK_STATE_COLLECTING &&
			bi->blockState != YAFFS_BLOCK_STATE_EMPTY) {
		T(YAFFS_TRACE_ERROR, (TSTR("Block %d is in state %d after gc, should be erased"TENDSTR),
			n, bi->blockState));
	}
}

static void yaffs_VerifyBlocks(yaffs_Device *dev)
{
	int i;
	int nBlocksPerState[YAFFS_NUMBER_OF_BLOCK_STATES];
	int nIllegalBlockStates = 0;

	if (yaffs_SkipVerification(dev))
		return;

	memset(nBlocksPerState, 0, sizeof(nBlocksPerState));

	for (i = dev->internalStartBlock; i <= dev->internalEndBlock; i++) {
		yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, i);
		yaffs_VerifyBlock(dev, bi, i);

		if (bi->blockState < YAFFS_NUMBER_OF_BLOCK_STATES)
			nBlocksPerState[bi->blockState]++;
		else
			nIllegalBlockStates++;
	}

	T(YAFFS_TRACE_VERIFY, (TSTR(""TENDSTR)));
	T(YAFFS_TRACE_VERIFY, (TSTR("Block summary"TENDSTR)));

	T(YAFFS_TRACE_VERIFY, (TSTR("%d blocks have illegal states"TENDSTR), nIllegalBlockStates));
	if (nBlocksPerState[YAFFS_BLOCK_STATE_ALLOCATING] > 1)
		T(YAFFS_TRACE_VERIFY, (TSTR("Too many allocating blocks"TENDSTR)));

	for (i = 0; i < YAFFS_NUMBER_OF_BLOCK_STATES; i++)
		T(YAFFS_TRACE_VERIFY,
		  (TSTR("%s %d blocks"TENDSTR),
		  blockStateName[i], nBlocksPerState[i]));

	if (dev->blocksInCheckpoint != nBlocksPerState[YAFFS_BLOCK_STATE_CHECKPOINT])
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Checkpoint block count wrong dev %d count %d"TENDSTR),
		 dev->blocksInCheckpoint, nBlocksPerState[YAFFS_BLOCK_STATE_CHECKPOINT]));

	if (dev->nErasedBlocks != nBlocksPerState[YAFFS_BLOCK_STATE_EMPTY])
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Erased block count wrong dev %d count %d"TENDSTR),
		 dev->nErasedBlocks, nBlocksPerState[YAFFS_BLOCK_STATE_EMPTY]));

	if (nBlocksPerState[YAFFS_BLOCK_STATE_COLLECTING] > 1)
		T(YAFFS_TRACE_VERIFY,
		 (TSTR("Too many collecting blocks %d (max is 1)"TENDSTR),
		 nBlocksPerState[YAFFS_BLOCK_STATE_COLLECTING]));

	T(YAFFS_TRACE_VERIFY, (TSTR(""TENDSTR)));

}

/*
 * Verify the object header. oh must be valid, but obj and tags may be NULL in which
 * case those tests will not be performed.
 */
static void yaffs_VerifyObjectHeader(yaffs_Object *obj, yaffs_ObjectHeader *oh, yaffs_ExtendedTags *tags, int parentCheck)
{
	if (obj && yaffs_SkipVerification(obj->myDev))
		return;

	if (!(tags && obj && oh)) {
		T(YAFFS_TRACE_VERIFY,
				(TSTR("Verifying object header tags %p obj %p oh %p"TENDSTR),
				tags, obj, oh));
		return;
	}

	if (oh->type <= YAFFS_OBJECT_TYPE_UNKNOWN ||
			oh->type > YAFFS_OBJECT_TYPE_MAX)
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header type is illegal value 0x%x"TENDSTR),
			tags->objectId, oh->type));

	if (tags->objectId != obj->objectId)
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header mismatch objectId %d"TENDSTR),
			tags->objectId, obj->objectId));


	/*
	 * Check that the object's parent ids match if parentCheck requested.
	 *
	 * Tests do not apply to the root object.
	 */

	if (parentCheck && tags->objectId > 1 && !obj->parent)
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header mismatch parentId %d obj->parent is NULL"TENDSTR),
			tags->objectId, oh->parentObjectId));

	if (parentCheck && obj->parent &&
			oh->parentObjectId != obj->parent->objectId &&
			(oh->parentObjectId != YAFFS_OBJECTID_UNLINKED ||
			obj->parent->objectId != YAFFS_OBJECTID_DELETED))
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header mismatch parentId %d parentObjectId %d"TENDSTR),
			tags->objectId, oh->parentObjectId, obj->parent->objectId));

	if (tags->objectId > 1 && oh->name[0] == 0) /* Null name */
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header name is NULL"TENDSTR),
			obj->objectId));

	if (tags->objectId > 1 && ((__u8)(oh->name[0])) == 0xff) /* Trashed name */
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header name is 0xFF"TENDSTR),
			obj->objectId));
}



static int yaffs_VerifyTnodeWorker(yaffs_Object *obj, yaffs_Tnode *tn,
					__u32 level, int chunkOffset)
{
	int i;
	yaffs_Device *dev = obj->myDev;
	int ok = 1;

	if (tn) {
		if (level > 0) {

			for (i = 0; i < YAFFS_NTNODES_INTERNAL && ok; i++) {
				if (tn->internal[i]) {
					ok = yaffs_VerifyTnodeWorker(obj,
							tn->internal[i],
							level - 1,
							(chunkOffset<<YAFFS_TNODES_INTERNAL_BITS) + i);
				}
			}
		} else if (level == 0) {
			yaffs_ExtendedTags tags;
			__u32 objectId = obj->objectId;

			chunkOffset <<=  YAFFS_TNODES_LEVEL0_BITS;

			for (i = 0; i < YAFFS_NTNODES_LEVEL0; i++) {
				__u32 theChunk = yaffs_GetChunkGroupBase(dev, tn, i);

				if (theChunk > 0) {
					/* T(~0,(TSTR("verifying (%d:%d) %d"TENDSTR),tags.objectId,tags.chunkId,theChunk)); */
					yaffs_ReadChunkWithTagsFromNAND(dev, theChunk, NULL, &tags);
					if (tags.objectId != objectId || tags.chunkId != chunkOffset) {
						T(~0, (TSTR("Object %d chunkId %d NAND mismatch chunk %d tags (%d:%d)"TENDSTR),
							objectId, chunkOffset, theChunk,
							tags.objectId, tags.chunkId));
					}
				}
				chunkOffset++;
			}
		}
	}

	return ok;

}


static void yaffs_VerifyFile(yaffs_Object *obj)
{
	int requiredTallness;
	int actualTallness;
	__u32 lastChunk;
	__u32 x;
	__u32 i;
	yaffs_Device *dev;
	yaffs_ExtendedTags tags;
	yaffs_Tnode *tn;
	__u32 objectId;

	if (!obj)
		return;

	if (yaffs_SkipVerification(obj->myDev))
		return;

	dev = obj->myDev;
	objectId = obj->objectId;

	/* Check file size is consistent with tnode depth */
	lastChunk =  obj->variant.fileVariant.fileSize / dev->nDataBytesPerChunk + 1;
	x = lastChunk >> YAFFS_TNODES_LEVEL0_BITS;
	requiredTallness = 0;
	while (x > 0) {
		x >>= YAFFS_TNODES_INTERNAL_BITS;
		requiredTallness++;
	}

	actualTallness = obj->variant.fileVariant.topLevel;

	if (requiredTallness > actualTallness)
		T(YAFFS_TRACE_VERIFY,
		(TSTR("Obj %d had tnode tallness %d, needs to be %d"TENDSTR),
		 obj->objectId, actualTallness, requiredTallness));


	/* Check that the chunks in the tnode tree are all correct.
	 * We do this by scanning through the tnode tree and
	 * checking the tags for every chunk match.
	 */

	if (yaffs_SkipNANDVerification(dev))
		return;

	for (i = 1; i <= lastChunk; i++) {
		tn = yaffs_FindLevel0Tnode(dev, &obj->variant.fileVariant, i);

		if (tn) {
			__u32 theChunk = yaffs_GetChunkGroupBase(dev, tn, i);
			if (theChunk > 0) {
				/* T(~0,(TSTR("verifying (%d:%d) %d"TENDSTR),objectId,i,theChunk)); */
				yaffs_ReadChunkWithTagsFromNAND(dev, theChunk, NULL, &tags);
				if (tags.objectId != objectId || tags.chunkId != i) {
					T(~0, (TSTR("Object %d chunkId %d NAND mismatch chunk %d tags (%d:%d)"TENDSTR),
						objectId, i, theChunk,
						tags.objectId, tags.chunkId));
				}
			}
		}
	}
}


static void yaffs_VerifyHardLink(yaffs_Object *obj)
{
	if (obj && yaffs_SkipVerification(obj->myDev))
		return;

	/* Verify sane equivalent object */
}

static void yaffs_VerifySymlink(yaffs_Object *obj)
{
	if (obj && yaffs_SkipVerification(obj->myDev))
		return;

	/* Verify symlink string */
}

static void yaffs_VerifySpecial(yaffs_Object *obj)
{
	if (obj && yaffs_SkipVerification(obj->myDev))
		return;
}

static void yaffs_VerifyObject(yaffs_Object *obj)
{
	yaffs_Device *dev;

	__u32 chunkMin;
	__u32 chunkMax;

	__u32 chunkIdOk;
	__u32 chunkInRange;
	__u32 chunkShouldNotBeDeleted;
	__u32 chunkValid;

	if (!obj)
		return;

	if (obj->beingCreated)
		return;

	dev = obj->myDev;

	if (yaffs_SkipVerification(dev))
		return;

	/* Check sane object header chunk */

	chunkMin = dev->internalStartBlock * dev->nChunksPerBlock;
	chunkMax = (dev->internalEndBlock+1) * dev->nChunksPerBlock - 1;

	chunkInRange = (((unsigned)(obj->hdrChunk)) >= chunkMin && ((unsigned)(obj->hdrChunk)) <= chunkMax);
	chunkIdOk = chunkInRange || (obj->hdrChunk == 0);
	chunkValid = chunkInRange &&
			yaffs_CheckChunkBit(dev,
					obj->hdrChunk / dev->nChunksPerBlock,
					obj->hdrChunk % dev->nChunksPerBlock);
	chunkShouldNotBeDeleted = chunkInRange && !chunkValid;

	if (!obj->fake &&
			(!chunkIdOk || chunkShouldNotBeDeleted)) {
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d has chunkId %d %s %s"TENDSTR),
			obj->objectId, obj->hdrChunk,
			chunkIdOk ? "" : ",out of range",
			chunkShouldNotBeDeleted ? ",marked as deleted" : ""));
	}

	if (chunkValid && !yaffs_SkipNANDVerification(dev)) {
		yaffs_ExtendedTags tags;
		yaffs_ObjectHeader *oh;
		__u8 *buffer = yaffs_GetTempBuffer(dev, __LINE__);

		oh = (yaffs_ObjectHeader *)buffer;

		yaffs_ReadChunkWithTagsFromNAND(dev, obj->hdrChunk, buffer,
				&tags);

		yaffs_VerifyObjectHeader(obj, oh, &tags, 1);

		yaffs_ReleaseTempBuffer(dev, buffer, __LINE__);
	}

	/* Verify it has a parent */
	if (obj && !obj->fake &&
			(!obj->parent || obj->parent->myDev != dev)) {
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d has parent pointer %p which does not look like an object"TENDSTR),
			obj->objectId, obj->parent));
	}

	/* Verify parent is a directory */
	if (obj->parent && obj->parent->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d's parent is not a directory (type %d)"TENDSTR),
			obj->objectId, obj->parent->variantType));
	}

	switch (obj->variantType) {
	case YAFFS_OBJECT_TYPE_FILE:
		yaffs_VerifyFile(obj);
		break;
	case YAFFS_OBJECT_TYPE_SYMLINK:
		yaffs_VerifySymlink(obj);
		break;
	case YAFFS_OBJECT_TYPE_DIRECTORY:
		yaffs_VerifyDirectory(obj);
		break;
	case YAFFS_OBJECT_TYPE_HARDLINK:
		yaffs_VerifyHardLink(obj);
		break;
	case YAFFS_OBJECT_TYPE_SPECIAL:
		yaffs_VerifySpecial(obj);
		break;
	case YAFFS_OBJECT_TYPE_UNKNOWN:
	default:
		T(YAFFS_TRACE_VERIFY,
		(TSTR("Obj %d has illegaltype %d"TENDSTR),
		obj->objectId, obj->variantType));
		break;
	}
}

static void yaffs_VerifyObjects(yaffs_Device *dev)
{
	yaffs_Object *obj;
	int i;
	struct ylist_head *lh;

	if (yaffs_SkipVerification(dev))
		return;

	/* Iterate through the objects in each hash entry */

	for (i = 0; i <  YAFFS_NOBJECT_BUCKETS; i++) {
		ylist_for_each(lh, &dev->objectBucket[i].list) {
			if (lh) {
				obj = ylist_entry(lh, yaffs_Object, hashLink);
				yaffs_VerifyObject(obj);
			}
		}
	}
}


/*
 *  Simple hash function. Needs to have a reasonable spread
 */

static Y_INLINE int yaffs_HashFunction(int n)
{
	n = abs(n);
	return n % YAFFS_NOBJECT_BUCKETS;
}

/*
 * Access functions to useful fake objects.
 * Note that root might have a presence in NAND if permissions are set.
 */

yaffs_Object *yaffs_Root(yaffs_Device *dev)
{
	return dev->rootDir;
}

yaffs_Object *yaffs_LostNFound(yaffs_Device *dev)
{
	return dev->lostNFoundDir;
}


/*
 *  Erased NAND checking functions
 */

int yaffs_CheckFF(__u8 *buffer, int nBytes)
{
	/* Horrible, slow implementation */
	while (nBytes--) {
		if (*buffer != 0xFF)
			return 0;
		buffer++;
	}
	return 1;
}

static int yaffs_CheckChunkErased(struct yaffs_DeviceStruct *dev,
				int chunkInNAND)
{
	int retval = YAFFS_OK;
	__u8 *data = yaffs_GetTempBuffer(dev, __LINE__);
	yaffs_ExtendedTags tags;
	int result;

	result = yaffs_ReadChunkWithTagsFromNAND(dev, chunkInNAND, data, &tags);

	if (tags.eccResult > YAFFS_ECC_RESULT_NO_ERROR)
		retval = YAFFS_FAIL;

	if (!yaffs_CheckFF(data, dev->nDataBytesPerChunk) || tags.chunkUsed) {
		T(YAFFS_TRACE_NANDACCESS,
		  (TSTR("Chunk %d not erased" TENDSTR), chunkInNAND));
		retval = YAFFS_FAIL;
	}

	yaffs_ReleaseTempBuffer(dev, data, __LINE__);

	return retval;

}


static int yaffs_VerifyChunkWritten(yaffs_Device *dev,
					int chunkInNAND,
					const __u8 *data,
					yaffs_ExtendedTags *tags)
{
	int retval = YAFFS_OK;
	yaffs_ExtendedTags tempTags;
	__u8 *buffer = yaffs_GetTempBuffer(dev,__LINE__);
	int result;
	
	result = yaffs_ReadChunkWithTagsFromNAND(dev,chunkInNAND,buffer,&tempTags);
	if(memcmp(buffer,data,dev->nDataBytesPerChunk) ||
		tempTags.objectId != tags->objectId ||
		tempTags.chunkId  != tags->chunkId ||
		tempTags.byteCount != tags->byteCount)
		retval = YAFFS_FAIL;

	yaffs_ReleaseTempBuffer(dev, buffer, __LINE__);

	return retval;
}

static int yaffs_WriteNewChunkWithTagsToNAND(struct yaffs_DeviceStruct *dev,
					const __u8 *data,
					yaffs_ExtendedTags *tags,
					int useReserve)
{
	int attempts = 0;
	int writeOk = 0;
	int chunk;

	yaffs_InvalidateCheckpoint(dev);

	do {
		yaffs_BlockInfo *bi = 0;
		int erasedOk = 0;

		chunk = yaffs_AllocateChunk(dev, useReserve, &bi);
		if (chunk < 0) {
			/* no space */
			break;
		}

		/* First check this chunk is erased, if it needs
		 * checking.  The checking policy (unless forced
		 * always on) is as follows:
		 *
		 * Check the first page we try to write in a block.
		 * If the check passes then we don't need to check any
		 * more.	If the check fails, we check again...
		 * If the block has been erased, we don't need to check.
		 *
		 * However, if the block has been prioritised for gc,
		 * then we think there might be something odd about
		 * this block and stop using it.
		 *
		 * Rationale: We should only ever see chunks that have
		 * not been erased if there was a partially written
		 * chunk due to power loss.  This checking policy should
		 * catch that case with very few checks and thus save a
		 * lot of checks that are most likely not needed.
		 *
		 * Mods to the above
		 * If an erase check fails or the write fails we skip the 
		 * rest of the block.
		 */

		/* let's give it a try */
		attempts++;

#ifdef CONFIG_YAFFS_ALWAYS_CHECK_CHUNK_ERASED
		bi->skipErasedCheck = 0;
#endif
		if (!bi->skipErasedCheck) {
			erasedOk = yaffs_CheckChunkErased(dev, chunk);
			if (erasedOk != YAFFS_OK) {
				T(YAFFS_TRACE_ERROR,
				(TSTR("**>> yaffs chunk %d was not erased"
				TENDSTR), chunk));

				/* If not erased, delete this one,
				 * skip rest of block and
				 * try another chunk */
				 yaffs_DeleteChunk(dev,chunk,1,__LINE__);
				 yaffs_SkipRestOfBlock(dev);
				continue;
			}
		}

		writeOk = yaffs_WriteChunkWithTagsToNAND(dev, chunk,
				data, tags);

		if(!bi->skipErasedCheck)
			writeOk = yaffs_VerifyChunkWritten(dev, chunk, data, tags);

		if (writeOk != YAFFS_OK) {
			/* Clean up aborted write, skip to next block and
			 * try another chunk */
			yaffs_HandleWriteChunkError(dev, chunk, erasedOk);
			continue;
		}

		bi->skipErasedCheck = 1;

		/* Copy the data into the robustification buffer */
		yaffs_HandleWriteChunkOk(dev, chunk, data, tags);

	} while (writeOk != YAFFS_OK &&
		(yaffs_wr_attempts <= 0 || attempts <= yaffs_wr_attempts));

	if (!writeOk)
		chunk = -1;

	if (attempts > 1) {
		T(YAFFS_TRACE_ERROR,
			(TSTR("**>> yaffs write required %d attempts" TENDSTR),
			attempts));

		dev->nRetriedWrites += (attempts - 1);
	}

	return chunk;
}

/*
 * Block retiring for handling a broken block.
 */

static void yaffs_RetireBlock(yaffs_Device *dev, int blockInNAND)
{
	yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, blockInNAND);

	yaffs_InvalidateCheckpoint(dev);

	if (yaffs_MarkBlockBad(dev, blockInNAND) != YAFFS_OK) {
		if (yaffs_EraseBlockInNAND(dev, blockInNAND) != YAFFS_OK) {
			T(YAFFS_TRACE_ALWAYS, (TSTR(
				"yaffs: Failed to mark bad and erase block %d"
				TENDSTR), blockInNAND));
		} else {
			yaffs_ExtendedTags tags;
			int chunkId = blockInNAND * dev->nChunksPerBlock;

			__u8 *buffer = yaffs_GetTempBuffer(dev, __LINE__);

			memset(buffer, 0xff, dev->nDataBytesPerChunk);
			yaffs_InitialiseTags(&tags);
			tags.sequenceNumber = YAFFS_SEQUENCE_BAD_BLOCK;
			if (dev->writeChunkWithTagsToNAND(dev, chunkId -
				dev->chunkOffset, buffer, &tags) != YAFFS_OK)
				T(YAFFS_TRACE_ALWAYS, (TSTR("yaffs: Failed to "
					TCONT("write bad block marker to block %d")
					TENDSTR), blockInNAND));

			yaffs_ReleaseTempBuffer(dev, buffer, __LINE__);
		}
	}

	bi->blockState = YAFFS_BLOCK_STATE_DEAD;
	bi->gcPrioritise = 0;
	bi->needsRetiring = 0;

	dev->nRetiredBlocks++;
}

/*
 * Functions for robustisizing TODO
 *
 */

static void yaffs_HandleWriteChunkOk(yaffs_Device *dev, int chunkInNAND,
				const __u8 *data,
				const yaffs_ExtendedTags *tags)
{
}

static void yaffs_HandleUpdateChunk(yaffs_Device *dev, int chunkInNAND,
				const yaffs_ExtendedTags *tags)
{
}

void yaffs_HandleChunkError(yaffs_Device *dev, yaffs_BlockInfo *bi)
{
	if (!bi->gcPrioritise) {
		bi->gcPrioritise = 1;
		dev->hasPendingPrioritisedGCs = 1;
		bi->chunkErrorStrikes++;

		if (bi->chunkErrorStrikes > 3) {
			bi->needsRetiring = 1; /* Too many stikes, so retire this */
			T(YAFFS_TRACE_ALWAYS, (TSTR("yaffs: Block struck out" TENDSTR)));

		}
	}
}

static void yaffs_HandleWriteChunkError(yaffs_Device *dev, int chunkInNAND,
		int erasedOk)
{
	int blockInNAND = chunkInNAND / dev->nChunksPerBlock;
	yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, blockInNAND);

	yaffs_HandleChunkError(dev, bi);

	if (erasedOk) {
		/* Was an actual write failure, so mark the block for retirement  */
		bi->needsRetiring = 1;
		T(YAFFS_TRACE_ERROR | YAFFS_TRACE_BAD_BLOCKS,
		  (TSTR("**>> Block %d needs retiring" TENDSTR), blockInNAND));
	}

	/* Delete the chunk */
	yaffs_DeleteChunk(dev, chunkInNAND, 1, __LINE__);
	yaffs_SkipRestOfBlock(dev);
}


/*---------------- Name handling functions ------------*/

static __u16 yaffs_CalcNameSum(const YCHAR *name)
{
	__u16 sum = 0;
	__u16 i = 1;

	const YUCHAR *bname = (const YUCHAR *) name;
	if (bname) {
		while ((*bname) && (i < (YAFFS_MAX_NAME_LENGTH/2))) {

#ifdef CONFIG_YAFFS_CASE_INSENSITIVE
			sum += yaffs_toupper(*bname) * i;
#else
			sum += (*bname) * i;
#endif
			i++;
			bname++;
		}
	}
	return sum;
}

static void yaffs_SetObjectName(yaffs_Object *obj, const YCHAR *name)
{
#ifdef CONFIG_YAFFS_SHORT_NAMES_IN_RAM
	memset(obj->shortName, 0, sizeof(YCHAR) * (YAFFS_SHORT_NAME_LENGTH+1));
	if (name && yaffs_strnlen(name,YAFFS_SHORT_NAME_LENGTH+1) <= YAFFS_SHORT_NAME_LENGTH)
		yaffs_strcpy(obj->shortName, name);
	else
		obj->shortName[0] = _Y('\0');
#endif
	obj->sum = yaffs_CalcNameSum(name);
}

/*-------------------- TNODES -------------------

 * List of spare tnodes
 * The list is hooked together using the first pointer
 * in the tnode.
 */

/* yaffs_CreateTnodes creates a bunch more tnodes and
 * adds them to the tnode free list.
 * Don't use this function directly
 */

static int yaffs_CreateTnodes(yaffs_Device *dev, int nTnodes)
{
	int i;
	int tnodeSize;
	yaffs_Tnode *newTnodes;
	__u8 *mem;
	yaffs_Tnode *curr;
	yaffs_Tnode *next;
	yaffs_TnodeList *tnl;

	if (nTnodes < 1)
		return YAFFS_OK;

	/* Calculate the tnode size in bytes for variable width tnode support.
	 * Must be a multiple of 32-bits  */
	tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

	if (tnodeSize < sizeof(yaffs_Tnode))
		tnodeSize = sizeof(yaffs_Tnode);

	/* make these things */

	newTnodes = YMALLOC(nTnodes * tnodeSize);
	mem = (__u8 *)newTnodes;

	if (!newTnodes) {
		T(YAFFS_TRACE_ERROR,
			(TSTR("yaffs: Could not allocate Tnodes" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* Hook them into the free list */
#if 0
	for (i = 0; i < nTnodes - 1; i++) {
		newTnodes[i].internal[0] = &newTnodes[i + 1];
#ifdef CONFIG_YAFFS_TNODE_LIST_DEBUG
		newTnodes[i].internal[YAFFS_NTNODES_INTERNAL] = (void *)1;
#endif
	}

	newTnodes[nTnodes - 1].internal[0] = dev->freeTnodes;
#ifdef CONFIG_YAFFS_TNODE_LIST_DEBUG
	newTnodes[nTnodes - 1].internal[YAFFS_NTNODES_INTERNAL] = (void *)1;
#endif
	dev->freeTnodes = newTnodes;
#else
	/* New hookup for wide tnodes */
	for (i = 0; i < nTnodes - 1; i++) {
		curr = (yaffs_Tnode *) &mem[i * tnodeSize];
		next = (yaffs_Tnode *) &mem[(i+1) * tnodeSize];
		curr->internal[0] = next;
	}

	curr = (yaffs_Tnode *) &mem[(nTnodes - 1) * tnodeSize];
	curr->internal[0] = dev->freeTnodes;
	dev->freeTnodes = (yaffs_Tnode *)mem;

#endif


	dev->nFreeTnodes += nTnodes;
	dev->nTnodesCreated += nTnodes;

	/* Now add this bunch of tnodes to a list for freeing up.
	 * NB If we can't add this to the management list it isn't fatal
	 * but it just means we can't free this bunch of tnodes later.
	 */

	tnl = YMALLOC(sizeof(yaffs_TnodeList));
	if (!tnl) {
		T(YAFFS_TRACE_ERROR,
		  (TSTR
		   ("yaffs: Could not add tnodes to management list" TENDSTR)));
		   return YAFFS_FAIL;
	} else {
		tnl->tnodes = newTnodes;
		tnl->next = dev->allocatedTnodeList;
		dev->allocatedTnodeList = tnl;
	}

	T(YAFFS_TRACE_ALLOCATE, (TSTR("yaffs: Tnodes added" TENDSTR)));

	return YAFFS_OK;
}

/* GetTnode gets us a clean tnode. Tries to make allocate more if we run out */

static yaffs_Tnode *yaffs_GetTnodeRaw(yaffs_Device *dev)
{
	yaffs_Tnode *tn = NULL;

	/* If there are none left make more */
	if (!dev->freeTnodes)
		yaffs_CreateTnodes(dev, YAFFS_ALLOCATION_NTNODES);

	if (dev->freeTnodes) {
		tn = dev->freeTnodes;
#ifdef CONFIG_YAFFS_TNODE_LIST_DEBUG
		if (tn->internal[YAFFS_NTNODES_INTERNAL] != (void *)1) {
			/* Hoosterman, this thing looks like it isn't in the list */
			T(YAFFS_TRACE_ALWAYS,
			  (TSTR("yaffs: Tnode list bug 1" TENDSTR)));
		}
#endif
		dev->freeTnodes = dev->freeTnodes->internal[0];
		dev->nFreeTnodes--;
	}

	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/

	return tn;
}

static yaffs_Tnode *yaffs_GetTnode(yaffs_Device *dev)
{
	yaffs_Tnode *tn = yaffs_GetTnodeRaw(dev);
	int tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

	if (tnodeSize < sizeof(yaffs_Tnode))
		tnodeSize = sizeof(yaffs_Tnode);

	if (tn)
		memset(tn, 0, tnodeSize);

	return tn;
}

/* FreeTnode frees up a tnode and puts it back on the free list */
static void yaffs_FreeTnode(yaffs_Device *dev, yaffs_Tnode *tn)
{
	if (tn) {
#ifdef CONFIG_YAFFS_TNODE_LIST_DEBUG
		if (tn->internal[YAFFS_NTNODES_INTERNAL] != 0) {
			/* Hoosterman, this thing looks like it is already in the list */
			T(YAFFS_TRACE_ALWAYS,
			  (TSTR("yaffs: Tnode list bug 2" TENDSTR)));
		}
		tn->internal[YAFFS_NTNODES_INTERNAL] = (void *)1;
#endif
		tn->internal[0] = dev->freeTnodes;
		dev->freeTnodes = tn;
		dev->nFreeTnodes++;
	}
	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/
}

static void yaffs_DeinitialiseTnodes(yaffs_Device *dev)
{
	/* Free the list of allocated tnodes */
	yaffs_TnodeList *tmp;

	while (dev->allocatedTnodeList) {
		tmp = dev->allocatedTnodeList->next;

		YFREE(dev->allocatedTnodeList->tnodes);
		YFREE(dev->allocatedTnodeList);
		dev->allocatedTnodeList = tmp;

	}

	dev->freeTnodes = NULL;
	dev->nFreeTnodes = 0;
}

static void yaffs_InitialiseTnodes(yaffs_Device *dev)
{
	dev->allocatedTnodeList = NULL;
	dev->freeTnodes = NULL;
	dev->nFreeTnodes = 0;
	dev->nTnodesCreated = 0;
}


void yaffs_LoadLevel0Tnode(yaffs_Device *dev, yaffs_Tnode *tn, unsigned pos,
		unsigned val)
{
	__u32 *map = (__u32 *)tn;
	__u32 bitInMap;
	__u32 bitInWord;
	__u32 wordInMap;
	__u32 mask;

	pos &= YAFFS_TNODES_LEVEL0_MASK;
	val >>= dev->chunkGroupBits;

	bitInMap = pos * dev->tnodeWidth;
	wordInMap = bitInMap / 32;
	bitInWord = bitInMap & (32 - 1);

	mask = dev->tnodeMask << bitInWord;

	map[wordInMap] &= ~mask;
	map[wordInMap] |= (mask & (val << bitInWord));

	if (dev->tnodeWidth > (32 - bitInWord)) {
		bitInWord = (32 - bitInWord);
		wordInMap++;;
		mask = dev->tnodeMask >> (/*dev->tnodeWidth -*/ bitInWord);
		map[wordInMap] &= ~mask;
		map[wordInMap] |= (mask & (val >> bitInWord));
	}
}

static __u32 yaffs_GetChunkGroupBase(yaffs_Device *dev, yaffs_Tnode *tn,
		unsigned pos)
{
	__u32 *map = (__u32 *)tn;
	__u32 bitInMap;
	__u32 bitInWord;
	__u32 wordInMap;
	__u32 val;

	pos &= YAFFS_TNODES_LEVEL0_MASK;

	bitInMap = pos * dev->tnodeWidth;
	wordInMap = bitInMap / 32;
	bitInWord = bitInMap & (32 - 1);

	val = map[wordInMap] >> bitInWord;

	if	(dev->tnodeWidth > (32 - bitInWord)) {
		bitInWord = (32 - bitInWord);
		wordInMap++;;
		val |= (map[wordInMap] << bitInWord);
	}

	val &= dev->tnodeMask;
	val <<= dev->chunkGroupBits;

	return val;
}

/* ------------------- End of individual tnode manipulation -----------------*/

/* ---------Functions to manipulate the look-up tree (made up of tnodes) ------
 * The look up tree is represented by the top tnode and the number of topLevel
 * in the tree. 0 means only the level 0 tnode is in the tree.
 */

/* FindLevel0Tnode finds the level 0 tnode, if one exists. */
static yaffs_Tnode *yaffs_FindLevel0Tnode(yaffs_Device *dev,
					yaffs_FileStructure *fStruct,
					__u32 chunkId)
{
	yaffs_Tnode *tn = fStruct->top;
	__u32 i;
	int requiredTallness;
	int level = fStruct->topLevel;

	/* Check sane level and chunk Id */
	if (level < 0 || level > YAFFS_TNODES_MAX_LEVEL)
		return NULL;

	if (chunkId > YAFFS_MAX_CHUNK_ID)
		return NULL;

	/* First check we're tall enough (ie enough topLevel) */

	i = chunkId >> YAFFS_TNODES_LEVEL0_BITS;
	requiredTallness = 0;
	while (i) {
		i >>= YAFFS_TNODES_INTERNAL_BITS;
		requiredTallness++;
	}

	if (requiredTallness > fStruct->topLevel)
		return NULL; /* Not tall enough, so we can't find it */

	/* Traverse down to level 0 */
	while (level > 0 && tn) {
		tn = tn->internal[(chunkId >>
			(YAFFS_TNODES_LEVEL0_BITS +
				(level - 1) *
				YAFFS_TNODES_INTERNAL_BITS)) &
			YAFFS_TNODES_INTERNAL_MASK];
		level--;
	}

	return tn;
}

/* AddOrFindLevel0Tnode finds the level 0 tnode if it exists, otherwise first expands the tree.
 * This happens in two steps:
 *  1. If the tree isn't tall enough, then make it taller.
 *  2. Scan down the tree towards the level 0 tnode adding tnodes if required.
 *
 * Used when modifying the tree.
 *
 *  If the tn argument is NULL, then a fresh tnode will be added otherwise the specified tn will
 *  be plugged into the ttree.
 */

static yaffs_Tnode *yaffs_AddOrFindLevel0Tnode(yaffs_Device *dev,
					yaffs_FileStructure *fStruct,
					__u32 chunkId,
					yaffs_Tnode *passedTn)
{
	int requiredTallness;
	int i;
	int l;
	yaffs_Tnode *tn;

	__u32 x;


	/* Check sane level and page Id */
	if (fStruct->topLevel < 0 || fStruct->topLevel > YAFFS_TNODES_MAX_LEVEL)
		return NULL;

	if (chunkId > YAFFS_MAX_CHUNK_ID)
		return NULL;

	/* First check we're tall enough (ie enough topLevel) */

	x = chunkId >> YAFFS_TNODES_LEVEL0_BITS;
	requiredTallness = 0;
	while (x) {
		x >>= YAFFS_TNODES_INTERNAL_BITS;
		requiredTallness++;
	}


	if (requiredTallness > fStruct->topLevel) {
		/* Not tall enough, gotta make the tree taller */
		for (i = fStruct->topLevel; i < requiredTallness; i++) {

			tn = yaffs_GetTnode(dev);

			if (tn) {
				tn->internal[0] = fStruct->top;
				fStruct->top = tn;
				fStruct->topLevel++;
			} else {
				T(YAFFS_TRACE_ERROR,
					(TSTR("yaffs: no more tnodes" TENDSTR)));
				return NULL;
			}
		}
	}

	/* Traverse down to level 0, adding anything we need */

	l = fStruct->topLevel;
	tn = fStruct->top;

	if (l > 0) {
		while (l > 0 && tn) {
			x = (chunkId >>
			     (YAFFS_TNODES_LEVEL0_BITS +
			      (l - 1) * YAFFS_TNODES_INTERNAL_BITS)) &
			    YAFFS_TNODES_INTERNAL_MASK;


			if ((l > 1) && !tn->internal[x]) {
				/* Add missing non-level-zero tnode */
				tn->internal[x] = yaffs_GetTnode(dev);
				if(!tn->internal[x])
					return NULL;

			} else if (l == 1) {
				/* Looking from level 1 at level 0 */
				if (passedTn) {
					/* If we already have one, then release it.*/
					if (tn->internal[x])
						yaffs_FreeTnode(dev, tn->internal[x]);
					tn->internal[x] = passedTn;

				} else if (!tn->internal[x]) {
					/* Don't have one, none passed in */
					tn->internal[x] = yaffs_GetTnode(dev);
					if(!tn->internal[x])
						return NULL;
				}
			}

			tn = tn->internal[x];
			l--;
		}
	} else {
		/* top is level 0 */
		if (passedTn) {
			memcpy(tn, passedTn, (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8);
			yaffs_FreeTnode(dev, passedTn);
		}
	}

	return tn;
}

static int yaffs_FindChunkInGroup(yaffs_Device *dev, int theChunk,
				yaffs_ExtendedTags *tags, int objectId,
				int chunkInInode)
{
	int j;

	for (j = 0; theChunk && j < dev->chunkGroupSize; j++) {
		if (yaffs_CheckChunkBit(dev, theChunk / dev->nChunksPerBlock,
				theChunk % dev->nChunksPerBlock)) {
			
			if(dev->chunkGroupSize == 1)
				return theChunk;
			else {
				yaffs_ReadChunkWithTagsFromNAND(dev, theChunk, NULL,
								tags);
				if (yaffs_TagsMatch(tags, objectId, chunkInInode)) {
					/* found it; */
					return theChunk;
				}
			}
		}
		theChunk++;
	}
	return -1;
}


/* DeleteWorker scans backwards through the tnode tree and deletes all the
 * chunks and tnodes in the file
 * Returns 1 if the tree was deleted.
 * Returns 0 if it stopped early due to hitting the limit and the delete is incomplete.
 */

static int yaffs_DeleteWorker(yaffs_Object *in, yaffs_Tnode *tn, __u32 level,
			      int chunkOffset, int *limit)
{
	int i;
	int chunkInInode;
	int theChunk;
	yaffs_ExtendedTags tags;
	int foundChunk;
	yaffs_Device *dev = in->myDev;

	int allDone = 1;

	if (tn) {
		if (level > 0) {
			for (i = YAFFS_NTNODES_INTERNAL - 1; allDone && i >= 0;
			     i--) {
				if (tn->internal[i]) {
					if (limit && (*limit) < 0) {
						allDone = 0;
					} else {
						allDone =
							yaffs_DeleteWorker(in,
								tn->
								internal
								[i],
								level -
								1,
								(chunkOffset
									<<
									YAFFS_TNODES_INTERNAL_BITS)
								+ i,
								limit);
					}
					if (allDone) {
						yaffs_FreeTnode(dev,
								tn->
								internal[i]);
						tn->internal[i] = NULL;
					}
				}
			}
			return (allDone) ? 1 : 0;
		} else if (level == 0) {
			int hitLimit = 0;

			for (i = YAFFS_NTNODES_LEVEL0 - 1; i >= 0 && !hitLimit;
					i--) {
				theChunk = yaffs_GetChunkGroupBase(dev, tn, i);
				if (theChunk) {

					chunkInInode = (chunkOffset <<
						YAFFS_TNODES_LEVEL0_BITS) + i;

					foundChunk =
						yaffs_FindChunkInGroup(dev,
								theChunk,
								&tags,
								in->objectId,
								chunkInInode);

					if (foundChunk > 0) {
						yaffs_DeleteChunk(dev,
								  foundChunk, 1,
								  __LINE__);
						in->nDataChunks--;
						if (limit) {
							*limit = *limit - 1;
							if (*limit <= 0)
								hitLimit = 1;
						}

					}

					yaffs_LoadLevel0Tnode(dev, tn, i, 0);
				}

			}
			return (i < 0) ? 1 : 0;

		}

	}

	return 1;

}

static void yaffs_SoftDeleteChunk(yaffs_Device *dev, int chunk)
{
	yaffs_BlockInfo *theBlock;

	T(YAFFS_TRACE_DELETION, (TSTR("soft delete chunk %d" TENDSTR), chunk));

	theBlock = yaffs_GetBlockInfo(dev, chunk / dev->nChunksPerBlock);
	if (theBlock) {
		theBlock->softDeletions++;
		dev->nFreeChunks++;
	}
}

/* SoftDeleteWorker scans backwards through the tnode tree and soft deletes all the chunks in the file.
 * All soft deleting does is increment the block's softdelete count and pulls the chunk out
 * of the tnode.
 * Thus, essentially this is the same as DeleteWorker except that the chunks are soft deleted.
 */

static int yaffs_SoftDeleteWorker(yaffs_Object *in, yaffs_Tnode *tn,
				  __u32 level, int chunkOffset)
{
	int i;
	int theChunk;
	int allDone = 1;
	yaffs_Device *dev = in->myDev;

	if (tn) {
		if (level > 0) {

			for (i = YAFFS_NTNODES_INTERNAL - 1; allDone && i >= 0;
			     i--) {
				if (tn->internal[i]) {
					allDone =
					    yaffs_SoftDeleteWorker(in,
								   tn->
								   internal[i],
								   level - 1,
								   (chunkOffset
								    <<
								    YAFFS_TNODES_INTERNAL_BITS)
								   + i);
					if (allDone) {
						yaffs_FreeTnode(dev,
								tn->
								internal[i]);
						tn->internal[i] = NULL;
					} else {
						/* Hoosterman... how could this happen? */
					}
				}
			}
			return (allDone) ? 1 : 0;
		} else if (level == 0) {

			for (i = YAFFS_NTNODES_LEVEL0 - 1; i >= 0; i--) {
				theChunk = yaffs_GetChunkGroupBase(dev, tn, i);
				if (theChunk) {
					/* Note this does not find the real chunk, only the chunk group.
					 * We make an assumption that a chunk group is not larger than
					 * a block.
					 */
					yaffs_SoftDeleteChunk(dev, theChunk);
					yaffs_LoadLevel0Tnode(dev, tn, i, 0);
				}

			}
			return 1;

		}

	}

	return 1;

}

static void yaffs_SoftDeleteFile(yaffs_Object *obj)
{
	if (obj->deleted &&
	    obj->variantType == YAFFS_OBJECT_TYPE_FILE && !obj->softDeleted) {
		if (obj->nDataChunks <= 0) {
			/* Empty file with no duplicate object headers, just delete it immediately */
			yaffs_FreeTnode(obj->myDev,
					obj->variant.fileVariant.top);
			obj->variant.fileVariant.top = NULL;
			T(YAFFS_TRACE_TRACING,
			  (TSTR("yaffs: Deleting empty file %d" TENDSTR),
			   obj->objectId));
			yaffs_DoGenericObjectDeletion(obj);
		} else {
			yaffs_SoftDeleteWorker(obj,
					       obj->variant.fileVariant.top,
					       obj->variant.fileVariant.
					       topLevel, 0);
			obj->softDeleted = 1;
		}
	}
}

/* Pruning removes any part of the file structure tree that is beyond the
 * bounds of the file (ie that does not point to chunks).
 *
 * A file should only get pruned when its size is reduced.
 *
 * Before pruning, the chunks must be pulled from the tree and the
 * level 0 tnode entries must be zeroed out.
 * Could also use this for file deletion, but that's probably better handled
 * by a special case.
 */

static yaffs_Tnode *yaffs_PruneWorker(yaffs_Device *dev, yaffs_Tnode *tn,
				__u32 level, int del0)
{
	int i;
	int hasData;

	if (tn) {
		hasData = 0;

		if(level > 0){
        		for (i = 0; i < YAFFS_NTNODES_INTERNAL; i++) {
	        		if (tn->internal[i]) {
		        		tn->internal[i] =
		        		        yaffs_PruneWorker(dev, tn->internal[i],
						        level - 1,
						        (i == 0) ? del0 : 1);
        			}

	        		if (tn->internal[i])
		        		hasData++;
                        }
		} else {
		        int tnodeSize;
		        __u32 *map = (__u32 *)tn;
                	tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

                	if (tnodeSize < sizeof(yaffs_Tnode))
                	        tnodeSize = sizeof(yaffs_Tnode);
                        tnodeSize /= sizeof(__u32);

                        for(i = 0; !hasData && i < tnodeSize; i++){
                                if(map[i])
                                        hasData++;
                        }
                }

		if (hasData == 0 && del0) {
			/* Free and return NULL */

			yaffs_FreeTnode(dev, tn);
			tn = NULL;
		}

	}

	return tn;

}

static int yaffs_PruneFileStructure(yaffs_Device *dev,
				yaffs_FileStructure *fStruct)
{
	int i;
	int hasData;
	int done = 0;
	yaffs_Tnode *tn;

	if (fStruct->topLevel > 0) {
		fStruct->top =
		    yaffs_PruneWorker(dev, fStruct->top, fStruct->topLevel, 0);

		/* Now we have a tree with all the non-zero branches NULL but the height
		 * is the same as it was.
		 * Let's see if we can trim internal tnodes to shorten the tree.
		 * We can do this if only the 0th element in the tnode is in use
		 * (ie all the non-zero are NULL)
		 */

		while (fStruct->topLevel && !done) {
			tn = fStruct->top;

			hasData = 0;
			for (i = 1; i < YAFFS_NTNODES_INTERNAL; i++) {
				if (tn->internal[i])
					hasData++;
			}

			if (!hasData) {
				fStruct->top = tn->internal[0];
				fStruct->topLevel--;
				yaffs_FreeTnode(dev, tn);
			} else {
				done = 1;
			}
		}
	}

	return YAFFS_OK;
}

/*-------------------- End of File Structure functions.-------------------*/

/* yaffs_CreateFreeObjects creates a bunch more objects and
 * adds them to the object free list.
 */
static int yaffs_CreateFreeObjects(yaffs_Device *dev, int nObjects)
{
	int i;
	yaffs_Object *newObjects;
	yaffs_ObjectList *list;

	if (nObjects < 1)
		return YAFFS_OK;

	/* make these things */
	newObjects = YMALLOC(nObjects * sizeof(yaffs_Object));
	list = YMALLOC(sizeof(yaffs_ObjectList));

	if (!newObjects || !list) {
		if (newObjects){
			YFREE(newObjects);
			newObjects = NULL;
		}
		if (list){
			YFREE(list);
			list = NULL;
		}
		T(YAFFS_TRACE_ALLOCATE,
		  (TSTR("yaffs: Could not allocate more objects" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* Hook them into the free list */
	for (i = 0; i < nObjects - 1; i++) {
		newObjects[i].siblings.next =
				(struct ylist_head *)(&newObjects[i + 1]);
	}

	newObjects[nObjects - 1].siblings.next = (void *)dev->freeObjects;
	dev->freeObjects = newObjects;
	dev->nFreeObjects += nObjects;
	dev->nObjectsCreated += nObjects;

	/* Now add this bunch of Objects to a list for freeing up. */

	list->objects = newObjects;
	list->next = dev->allocatedObjectList;
	dev->allocatedObjectList = list;

	return YAFFS_OK;
}


/* AllocateEmptyObject gets us a clean Object. Tries to make allocate more if we run out */
static yaffs_Object *yaffs_AllocateEmptyObject(yaffs_Device *dev)
{
	yaffs_Object *tn = NULL;

#ifdef VALGRIND_TEST
	tn = YMALLOC(sizeof(yaffs_Object));
#else
	/* If there are none left make more */
	if (!dev->freeObjects)
		yaffs_CreateFreeObjects(dev, YAFFS_ALLOCATION_NOBJECTS);

	if (dev->freeObjects) {
		tn = dev->freeObjects;
		dev->freeObjects =
			(yaffs_Object *) (dev->freeObjects->siblings.next);
		dev->nFreeObjects--;
	}
#endif
	if (tn) {
		/* Now sweeten it up... */

		memset(tn, 0, sizeof(yaffs_Object));
		tn->beingCreated = 1;

		tn->myDev = dev;
		tn->hdrChunk = 0;
		tn->variantType = YAFFS_OBJECT_TYPE_UNKNOWN;
		YINIT_LIST_HEAD(&(tn->hardLinks));
		YINIT_LIST_HEAD(&(tn->hashLink));
		YINIT_LIST_HEAD(&tn->siblings);


		/* Now make the directory sane */
		if (dev->rootDir) {
			tn->parent = dev->rootDir;
			ylist_add(&(tn->siblings), &dev->rootDir->variant.directoryVariant.children);
		}

		/* Add it to the lost and found directory.
		 * NB Can't put root or lostNFound in lostNFound so
		 * check if lostNFound exists first
		 */
		if (dev->lostNFoundDir)
			yaffs_AddObjectToDirectory(dev->lostNFoundDir, tn);

		tn->beingCreated = 0;
	}

	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/

	return tn;
}

static yaffs_Object *yaffs_CreateFakeDirectory(yaffs_Device *dev, int number,
					       __u32 mode)
{

	yaffs_Object *obj =
	    yaffs_CreateNewObject(dev, number, YAFFS_OBJECT_TYPE_DIRECTORY);
	if (obj) {
		obj->fake = 1;		/* it is fake so it might have no NAND presence... */
		obj->renameAllowed = 0;	/* ... and we're not allowed to rename it... */
		obj->unlinkAllowed = 0;	/* ... or unlink it */
		obj->deleted = 0;
		obj->unlinked = 0;
		obj->yst_mode = mode;
		obj->myDev = dev;
		obj->hdrChunk = 0;	/* Not a valid chunk. */
	}

	return obj;

}

static void yaffs_UnhashObject(yaffs_Object *tn)
{
	int bucket;
	yaffs_Device *dev = tn->myDev;

	/* If it is still linked into the bucket list, free from the list */
	if (!ylist_empty(&tn->hashLink)) {
		ylist_del_init(&tn->hashLink);
		bucket = yaffs_HashFunction(tn->objectId);
		dev->objectBucket[bucket].count--;
	}
}

/*  FreeObject frees up a Object and puts it back on the free list */
static void yaffs_FreeObject(yaffs_Object *tn)
{
	yaffs_Device *dev = tn->myDev;

#ifdef __KERNEL__
	T(YAFFS_TRACE_OS, (TSTR("FreeObject %p inode %p"TENDSTR), tn, tn->myInode));
#endif

	if (tn->parent)
		YBUG();
	if (!ylist_empty(&tn->siblings))
		YBUG();


#ifdef __KERNEL__
	if (tn->myInode) {
		/* We're still hooked up to a cached inode.
		 * Don't delete now, but mark for later deletion
		 */
		tn->deferedFree = 1;
		return;
	}
#endif

	yaffs_UnhashObject(tn);

#ifdef VALGRIND_TEST
	YFREE(tn);
	tn = NULL;
#else
	/* Link into the free list. */
	tn->siblings.next = (struct ylist_head *)(dev->freeObjects);
	dev->freeObjects = tn;
	dev->nFreeObjects++;
#endif
	dev->nCheckpointBlocksRequired = 0; /* force recalculation*/
}

#ifdef __KERNEL__

void yaffs_HandleDeferedFree(yaffs_Object *obj)
{
	if (obj->deferedFree)
		yaffs_FreeObject(obj);
}

#endif

static void yaffs_DeinitialiseObjects(yaffs_Device *dev)
{
	/* Free the list of allocated Objects */

	yaffs_ObjectList *tmp;

	while (dev->allocatedObjectList) {
		tmp = dev->allocatedObjectList->next;
		YFREE(dev->allocatedObjectList->objects);
		YFREE(dev->allocatedObjectList);

		dev->allocatedObjectList = tmp;
	}

	dev->freeObjects = NULL;
	dev->nFreeObjects = 0;
}

static void yaffs_InitialiseObjects(yaffs_Device *dev)
{
	int i;

	dev->allocatedObjectList = NULL;
	dev->freeObjects = NULL;
	dev->nFreeObjects = 0;

	for (i = 0; i < YAFFS_NOBJECT_BUCKETS; i++) {
		YINIT_LIST_HEAD(&dev->objectBucket[i].list);
		dev->objectBucket[i].count = 0;
	}
}

static int yaffs_FindNiceObjectBucket(yaffs_Device *dev)
{
	static int x;
	int i;
	int l = 999;
	int lowest = 999999;

	/* First let's see if we can find one that's empty. */

	for (i = 0; i < 10 && lowest > 0; i++) {
		x++;
		x %= YAFFS_NOBJECT_BUCKETS;
		if (dev->objectBucket[x].count < lowest) {
			lowest = dev->objectBucket[x].count;
			l = x;
		}

	}

	/* If we didn't find an empty list, then try
	 * looking a bit further for a short one
	 */

	for (i = 0; i < 10 && lowest > 3; i++) {
		x++;
		x %= YAFFS_NOBJECT_BUCKETS;
		if (dev->objectBucket[x].count < lowest) {
			lowest = dev->objectBucket[x].count;
			l = x;
		}

	}

	return l;
}

static int yaffs_CreateNewObjectNumber(yaffs_Device *dev)
{
	int bucket = yaffs_FindNiceObjectBucket(dev);

	/* Now find an object value that has not already been taken
	 * by scanning the list.
	 */

	int found = 0;
	struct ylist_head *i;

	__u32 n = (__u32) bucket;

	/* yaffs_CheckObjectHashSanity();  */

	while (!found) {
		found = 1;
		n += YAFFS_NOBJECT_BUCKETS;
		if (1 || dev->objectBucket[bucket].count > 0) {
			ylist_for_each(i, &dev->objectBucket[bucket].list) {
				/* If there is already one in the list */
				if (i && ylist_entry(i, yaffs_Object,
						hashLink)->objectId == n) {
					found = 0;
				}
			}
		}
	}

	return n;
}

static void yaffs_HashObject(yaffs_Object *in)
{
	int bucket = yaffs_HashFunction(in->objectId);
	yaffs_Device *dev = in->myDev;

	ylist_add(&in->hashLink, &dev->objectBucket[bucket].list);
	dev->objectBucket[bucket].count++;
}

yaffs_Object *yaffs_FindObjectByNumber(yaffs_Device *dev, __u32 number)
{
	int bucket = yaffs_HashFunction(number);
	struct ylist_head *i;
	yaffs_Object *in;

	ylist_for_each(i, &dev->objectBucket[bucket].list) {
		/* Look if it is in the list */
		if (i) {
			in = ylist_entry(i, yaffs_Object, hashLink);
			if (in->objectId == number) {
#ifdef __KERNEL__
				/* Don't tell the VFS about this one if it is defered free */
				if (in->deferedFree)
					return NULL;
#endif

				return in;
			}
		}
	}

	return NULL;
}

yaffs_Object *yaffs_CreateNewObject(yaffs_Device *dev, int number,
				    yaffs_ObjectType type)
{
	yaffs_Object *theObject;
	yaffs_Tnode *tn = NULL;

	if (number < 0)
		number = yaffs_CreateNewObjectNumber(dev);

	if (type == YAFFS_OBJECT_TYPE_FILE) {
		tn = yaffs_GetTnode(dev);
		if (!tn)
			return NULL;
	}

	theObject = yaffs_AllocateEmptyObject(dev);
	if (!theObject){
		if(tn)
			yaffs_FreeTnode(dev,tn);
		return NULL;
	}


	if (theObject) {
		theObject->fake = 0;
		theObject->renameAllowed = 1;
		theObject->unlinkAllowed = 1;
		theObject->objectId = number;
		yaffs_HashObject(theObject);
		theObject->variantType = type;
#ifdef CONFIG_YAFFS_WINCE
		yfsd_WinFileTimeNow(theObject->win_atime);
		theObject->win_ctime[0] = theObject->win_mtime[0] =
		    theObject->win_atime[0];
		theObject->win_ctime[1] = theObject->win_mtime[1] =
		    theObject->win_atime[1];

#else

		theObject->yst_atime = theObject->yst_mtime =
		    theObject->yst_ctime = Y_CURRENT_TIME;
#endif
		switch (type) {
		case YAFFS_OBJECT_TYPE_FILE:
			theObject->variant.fileVariant.fileSize = 0;
			theObject->variant.fileVariant.scannedFileSize = 0;
			theObject->variant.fileVariant.shrinkSize = 0xFFFFFFFF;	/* max __u32 */
			theObject->variant.fileVariant.topLevel = 0;
			theObject->variant.fileVariant.top = tn;
			break;
		case YAFFS_OBJECT_TYPE_DIRECTORY:
			YINIT_LIST_HEAD(&theObject->variant.directoryVariant.
					children);
			break;
		case YAFFS_OBJECT_TYPE_SYMLINK:
		case YAFFS_OBJECT_TYPE_HARDLINK:
		case YAFFS_OBJECT_TYPE_SPECIAL:
			/* No action required */
			break;
		case YAFFS_OBJECT_TYPE_UNKNOWN:
			/* todo this should not happen */
			break;
		}
	}

	return theObject;
}

static yaffs_Object *yaffs_FindOrCreateObjectByNumber(yaffs_Device *dev,
						      int number,
						      yaffs_ObjectType type)
{
	yaffs_Object *theObject = NULL;

	if (number > 0)
		theObject = yaffs_FindObjectByNumber(dev, number);

	if (!theObject)
		theObject = yaffs_CreateNewObject(dev, number, type);

	return theObject;

}


static YCHAR *yaffs_CloneString(const YCHAR *str)
{
	YCHAR *newStr = NULL;
	int len;

	if (!str)
		str = _Y("");

	len = yaffs_strnlen(str,YAFFS_MAX_ALIAS_LENGTH);
	newStr = YMALLOC((len + 1) * sizeof(YCHAR));
	if (newStr){
		yaffs_strncpy(newStr, str,len);
		newStr[len] = 0;
	}
	return newStr;

}

/*
 * Mknod (create) a new object.
 * equivalentObject only has meaning for a hard link;
 * aliasString only has meaning for a symlink.
 * rdev only has meaning for devices (a subset of special objects)
 */

static yaffs_Object *yaffs_MknodObject(yaffs_ObjectType type,
				       yaffs_Object *parent,
				       const YCHAR *name,
				       __u32 mode,
				       __u32 uid,
				       __u32 gid,
				       yaffs_Object *equivalentObject,
				       const YCHAR *aliasString, __u32 rdev)
{
	yaffs_Object *in;
	YCHAR *str = NULL;

	yaffs_Device *dev = parent->myDev;

	/* Check if the entry exists. If it does then fail the call since we don't want a dup.*/
	if (yaffs_FindObjectByName(parent, name))
		return NULL;

	if (type == YAFFS_OBJECT_TYPE_SYMLINK) {
		str = yaffs_CloneString(aliasString);
		if (!str)
			return NULL;
	}

	in = yaffs_CreateNewObject(dev, -1, type);

	if (!in){
		if(str)
			YFREE(str);
		return NULL;
	}




	if (in) {
		in->hdrChunk = 0;
		in->valid = 1;
		in->variantType = type;

		in->yst_mode = mode;

#ifdef CONFIG_YAFFS_WINCE
		yfsd_WinFileTimeNow(in->win_atime);
		in->win_ctime[0] = in->win_mtime[0] = in->win_atime[0];
		in->win_ctime[1] = in->win_mtime[1] = in->win_atime[1];

#else
		in->yst_atime = in->yst_mtime = in->yst_ctime = Y_CURRENT_TIME;

		in->yst_rdev = rdev;
		in->yst_uid = uid;
		in->yst_gid = gid;
#endif
		in->nDataChunks = 0;

		yaffs_SetObjectName(in, name);
		in->dirty = 1;

		yaffs_AddObjectToDirectory(parent, in);

		in->myDev = parent->myDev;

		switch (type) {
		case YAFFS_OBJECT_TYPE_SYMLINK:
			in->variant.symLinkVariant.alias = str;
			break;
		case YAFFS_OBJECT_TYPE_HARDLINK:
			in->variant.hardLinkVariant.equivalentObject =
				equivalentObject;
			in->variant.hardLinkVariant.equivalentObjectId =
				equivalentObject->objectId;
			ylist_add(&in->hardLinks, &equivalentObject->hardLinks);
			break;
		case YAFFS_OBJECT_TYPE_FILE:
		case YAFFS_OBJECT_TYPE_DIRECTORY:
		case YAFFS_OBJECT_TYPE_SPECIAL:
		case YAFFS_OBJECT_TYPE_UNKNOWN:
			/* do nothing */
			break;
		}

		if (yaffs_UpdateObjectHeader(in, name, 0, 0, 0) < 0) {
			/* Could not create the object header, fail the creation */
			yaffs_DeleteObject(in);
			in = NULL;
		}

		yaffs_UpdateParent(parent);
	}

	return in;
}

yaffs_Object *yaffs_MknodFile(yaffs_Object *parent, const YCHAR *name,
			__u32 mode, __u32 uid, __u32 gid)
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_FILE, parent, name, mode,
				uid, gid, NULL, NULL, 0);
}

yaffs_Object *yaffs_MknodDirectory(yaffs_Object *parent, const YCHAR *name,
				__u32 mode, __u32 uid, __u32 gid)
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_DIRECTORY, parent, name,
				 mode, uid, gid, NULL, NULL, 0);
}

yaffs_Object *yaffs_MknodSpecial(yaffs_Object *parent, const YCHAR *name,
				__u32 mode, __u32 uid, __u32 gid, __u32 rdev)
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_SPECIAL, parent, name, mode,
				 uid, gid, NULL, NULL, rdev);
}

yaffs_Object *yaffs_MknodSymLink(yaffs_Object *parent, const YCHAR *name,
				__u32 mode, __u32 uid, __u32 gid,
				const YCHAR *alias)
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_SYMLINK, parent, name, mode,
				uid, gid, NULL, alias, 0);
}

/* yaffs_Link returns the object id of the equivalent object.*/
yaffs_Object *yaffs_Link(yaffs_Object *parent, const YCHAR *name,
			yaffs_Object *equivalentObject)
{
	/* Get the real object in case we were fed a hard link as an equivalent object */
	equivalentObject = yaffs_GetEquivalentObject(equivalentObject);

	if (yaffs_MknodObject
	    (YAFFS_OBJECT_TYPE_HARDLINK, parent, name, 0, 0, 0,
	     equivalentObject, NULL, 0)) {
		return equivalentObject;
	} else {
		return NULL;
	}

}

static int yaffs_ChangeObjectName(yaffs_Object *obj, yaffs_Object *newDir,
				const YCHAR *newName, int force, int shadows)
{
	int unlinkOp;
	int deleteOp;

	yaffs_Object *existingTarget;

	if (newDir == NULL)
		newDir = obj->parent;	/* use the old directory */

	if (newDir->variantType != YAFFS_OBJECT_TYPE_DIRECTORY) {
		T(YAFFS_TRACE_ALWAYS,
		  (TSTR
		   ("tragedy: yaffs_ChangeObjectName: newDir is not a directory"
		    TENDSTR)));
		YBUG();
	}

	/* TODO: Do we need this different handling for YAFFS2 and YAFFS1?? */
	if (obj->myDev->isYaffs2)
		unlinkOp = (newDir == obj->myDev->unlinkedDir);
	else
		unlinkOp = (newDir == obj->myDev->unlinkedDir
			    && obj->variantType == YAFFS_OBJECT_TYPE_FILE);

	deleteOp = (newDir == obj->myDev->deletedDir);

	existingTarget = yaffs_FindObjectByName(newDir, newName);

	/* If the object is a file going into the unlinked directory,
	 *   then it is OK to just stuff it in since duplicate names are allowed.
	 *   else only proceed if the new name does not exist and if we're putting
	 *   it into a directory.
	 */
	if ((unlinkOp ||
	     deleteOp ||
	     force ||
	     (shadows > 0) ||
	     !existingTarget) &&
	    newDir->variantType == YAFFS_OBJECT_TYPE_DIRECTORY) {
		yaffs_SetObjectName(obj, newName);
		obj->dirty = 1;

		yaffs_AddObjectToDirectory(newDir, obj);

		if (unlinkOp)
			obj->unlinked = 1;

		/* If it is a deletion then we mark it as a shrink for gc purposes. */
		if (yaffs_UpdateObjectHeader(obj, newName, 0, deleteOp, shadows) >= 0)
			return YAFFS_OK;
	}

	return YAFFS_FAIL;
}

int yaffs_RenameObject(yaffs_Object *oldDir, const YCHAR *oldName,
		yaffs_Object *newDir, const YCHAR *newName)
{
	yaffs_Object *obj = NULL;
	yaffs_Object *existingTarget = NULL;
	int force = 0;
	int result;
	yaffs_Device *dev;


	if (!oldDir || oldDir->variantType != YAFFS_OBJECT_TYPE_DIRECTORY)
		YBUG();
	if (!newDir || newDir->variantType != YAFFS_OBJECT_TYPE_DIRECTORY)
		YBUG();

	dev = oldDir->myDev;

#ifdef CONFIG_YAFFS_CASE_INSENSITIVE
	/* Special case for case insemsitive systems (eg. WinCE).
	 * While look-up is case insensitive, the name isn't.
	 * Therefore we might want to change x.txt to X.txt
	*/
	if (oldDir == newDir && yaffs_strcmp(oldName, newName) == 0)
		force = 1;
#endif

	if(yaffs_strnlen(newName,YAFFS_MAX_NAME_LENGTH+1) > YAFFS_MAX_NAME_LENGTH)
		/* ENAMETOOLONG */
		return YAFFS_FAIL;

	obj = yaffs_FindObjectByName(oldDir, oldName);

	if (obj && obj->renameAllowed) {

		/* Now do the handling for an existing target, if there is one */

		existingTarget = yaffs_FindObjectByName(newDir, newName);
		if (existingTarget &&
			existingTarget->variantType == YAFFS_OBJECT_TYPE_DIRECTORY &&
			!ylist_empty(&existingTarget->variant.directoryVariant.children)) {
			/* There is a target that is a non-empty directory, so we fail */
			return YAFFS_FAIL;	/* EEXIST or ENOTEMPTY */
		} else if (existingTarget && existingTarget != obj) {
			/* Nuke the target first, using shadowing,
			 * but only if it isn't the same object.
			 *
			 * Note we must disable gc otherwise it can mess up the shadowing.
			 *
			 */
			dev->isDoingGC=1;
			yaffs_ChangeObjectName(obj, newDir, newName, force,
						existingTarget->objectId);
			existingTarget->isShadowed = 1;
			yaffs_UnlinkObject(existingTarget);
			dev->isDoingGC=0;
		}

		result = yaffs_ChangeObjectName(obj, newDir, newName, 1, 0);

		yaffs_UpdateParent(oldDir);
		if(newDir != oldDir)
			yaffs_UpdateParent(newDir);
		
		return result;
	}
	return YAFFS_FAIL;
}

/*------------------------- Block Management and Page Allocation ----------------*/

static int yaffs_InitialiseBlocks(yaffs_Device *dev)
{
	int nBlocks = dev->internalEndBlock - dev->internalStartBlock + 1;

	dev->blockInfo = NULL;
	dev->chunkBits = NULL;

	dev->allocationBlock = -1;	/* force it to get a new one */

	/* If the first allocation strategy fails, thry the alternate one */
	dev->blockInfo = YMALLOC(nBlocks * sizeof(yaffs_BlockInfo));
	if (!dev->blockInfo) {
		dev->blockInfo = YMALLOC_ALT(nBlocks * sizeof(yaffs_BlockInfo));
		dev->blockInfoAlt = 1;
	} else
		dev->blockInfoAlt = 0;

	if (dev->blockInfo) {
		/* Set up dynamic blockinfo stuff. */
		dev->chunkBitmapStride = (dev->nChunksPerBlock + 7) / 8; /* round up bytes */
		dev->chunkBits = YMALLOC(dev->chunkBitmapStride * nBlocks);
		if (!dev->chunkBits) {
			dev->chunkBits = YMALLOC_ALT(dev->chunkBitmapStride * nBlocks);
			dev->chunkBitsAlt = 1;
		} else
			dev->chunkBitsAlt = 0;
	}

	if (dev->blockInfo && dev->chunkBits) {
		memset(dev->blockInfo, 0, nBlocks * sizeof(yaffs_BlockInfo));
		memset(dev->chunkBits, 0, dev->chunkBitmapStride * nBlocks);
		return YAFFS_OK;
	}

	return YAFFS_FAIL;
}

static void yaffs_DeinitialiseBlocks(yaffs_Device *dev)
{
	if (dev->blockInfoAlt && dev->blockInfo)
		YFREE_ALT(dev->blockInfo);
	else if (dev->blockInfo)
		YFREE(dev->blockInfo);

	dev->blockInfoAlt = 0;

	dev->blockInfo = NULL;

	if (dev->chunkBitsAlt && dev->chunkBits)
		YFREE_ALT(dev->chunkBits);
	else if (dev->chunkBits)
		YFREE(dev->chunkBits);
	dev->chunkBitsAlt = 0;
	dev->chunkBits = NULL;
}

static int yaffs_BlockNotDisqualifiedFromGC(yaffs_Device *dev,
					yaffs_BlockInfo *bi)
{
	int i;
	__u32 seq;
	yaffs_BlockInfo *b;

	if (!dev->isYaffs2)
		return 1;	/* disqualification only applies to yaffs2. */

	if (!bi->hasShrinkHeader)
		return 1;	/* can gc */

	/* Find the oldest dirty sequence number if we don't know it and save it
	 * so we don't have to keep recomputing it.
	 */
	if (!dev->oldestDirtySequence) {
		seq = dev->sequenceNumber;

		for (i = dev->internalStartBlock; i <= dev->internalEndBlock;
				i++) {
			b = yaffs_GetBlockInfo(dev, i);
			if (b->blockState == YAFFS_BLOCK_STATE_FULL &&
			    (b->pagesInUse - b->softDeletions) <
			    dev->nChunksPerBlock && b->sequenceNumber < seq) {
				seq = b->sequenceNumber;
			}
		}
		dev->oldestDirtySequence = seq;
	}

	/* Can't do gc of this block if there are any blocks older than this one that have
	 * discarded pages.
	 */
	return (bi->sequenceNumber <= dev->oldestDirtySequence);
}

/* FindDiretiestBlock is used to select the dirtiest block (or close enough)
 * for garbage collection.
 */

static int yaffs_FindBlockForGarbageCollection(yaffs_Device *dev,
					int aggressive)
{
	int b = dev->currentDirtyChecker;

	int i;
	int iterations;
	int dirtiest = -1;
	int pagesInUse = 0;
	int prioritised = 0;
	yaffs_BlockInfo *bi;
	int pendingPrioritisedExist = 0;

	/* First let's see if we need to grab a prioritised block */
	if (dev->hasPendingPrioritisedGCs) {
		for (i = dev->internalStartBlock; i < dev->internalEndBlock && !prioritised; i++) {

			bi = yaffs_GetBlockInfo(dev, i);
			/* yaffs_VerifyBlock(dev,bi,i); */

			if (bi->gcPrioritise) {
				pendingPrioritisedExist = 1;
				if (bi->blockState == YAFFS_BLOCK_STATE_FULL &&
				   yaffs_BlockNotDisqualifiedFromGC(dev, bi)) {
					pagesInUse = (bi->pagesInUse - bi->softDeletions);
					dirtiest = i;
					prioritised = 1;
					aggressive = 1; /* Fool the non-aggressive skip logiv below */
				}
			}
		}

		if (!pendingPrioritisedExist) /* None found, so we can clear this */
			dev->hasPendingPrioritisedGCs = 0;
	}

	/* If we're doing aggressive GC then we are happy to take a less-dirty block, and
	 * search harder.
	 * else (we're doing a leasurely gc), then we only bother to do this if the
	 * block has only a few pages in use.
	 */

	dev->nonAggressiveSkip--;

	if (!aggressive && (dev->nonAggressiveSkip > 0))
		return -1;

	if (!prioritised)
		pagesInUse =
			(aggressive) ? dev->nChunksPerBlock : YAFFS_PASSIVE_GC_CHUNKS + 1;

	if (aggressive)
		iterations =
		    dev->internalEndBlock - dev->internalStartBlock + 1;
	else {
		iterations =
		    dev->internalEndBlock - dev->internalStartBlock + 1;
		iterations = iterations / 16;
		if (iterations > 200)
			iterations = 200;
	}

	for (i = 0; i <= iterations && pagesInUse > 0 && !prioritised; i++) {
		b++;
		if (b < dev->internalStartBlock || b > dev->internalEndBlock)
			b = dev->internalStartBlock;

		if (b < dev->internalStartBlock || b > dev->internalEndBlock) {
			T(YAFFS_TRACE_ERROR,
			  (TSTR("**>> Block %d is not valid" TENDSTR), b));
			YBUG();
		}

		bi = yaffs_GetBlockInfo(dev, b);

		if (bi->blockState == YAFFS_BLOCK_STATE_FULL &&
			(bi->pagesInUse - bi->softDeletions) < pagesInUse &&
				yaffs_BlockNotDisqualifiedFromGC(dev, bi)) {
			dirtiest = b;
			pagesInUse = (bi->pagesInUse - bi->softDeletions);
		}
	}

	dev->currentDirtyChecker = b;

	if (dirtiest > 0) {
		T(YAFFS_TRACE_GC,
		  (TSTR("GC Selected block %d with %d free, prioritised:%d" TENDSTR), dirtiest,
		   dev->nChunksPerBlock - pagesInUse, prioritised));
	}

	dev->oldestDirtySequence = 0;

	if (dirtiest > 0)
		dev->nonAggressiveSkip = 4;

	return dirtiest;
}

static void yaffs_BlockBecameDirty(yaffs_Device *dev, int blockNo)
{
	yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, blockNo);

	int erasedOk = 0;

	/* If the block is still healthy erase it and mark as clean.
	 * If the block has had a data failure, then retire it.
	 */

	T(YAFFS_TRACE_GC | YAFFS_TRACE_ERASE,
		(TSTR("yaffs_BlockBecameDirty block %d state %d %s"TENDSTR),
		blockNo, bi->blockState, (bi->needsRetiring) ? "needs retiring" : ""));

	bi->blockState = YAFFS_BLOCK_STATE_DIRTY;

	if (!bi->needsRetiring) {
		yaffs_InvalidateCheckpoint(dev);
		erasedOk = yaffs_EraseBlockInNAND(dev, blockNo);
		if (!erasedOk) {
			dev->nErasureFailures++;
			T(YAFFS_TRACE_ERROR | YAFFS_TRACE_BAD_BLOCKS,
			  (TSTR("**>> Erasure failed %d" TENDSTR), blockNo));
		}
	}

	if (erasedOk &&
	    ((yaffs_traceMask & YAFFS_TRACE_ERASE) || !yaffs_SkipVerification(dev))) {
		int i;
		for (i = 0; i < dev->nChunksPerBlock; i++) {
			if (!yaffs_CheckChunkErased
			    (dev, blockNo * dev->nChunksPerBlock + i)) {
				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   (">>Block %d erasure supposedly OK, but chunk %d not erased"
				    TENDSTR), blockNo, i));
			}
		}
	}

	if (erasedOk) {
		/* Clean it up... */
		bi->blockState = YAFFS_BLOCK_STATE_EMPTY;
		dev->nErasedBlocks++;
		bi->pagesInUse = 0;
		bi->softDeletions = 0;
		bi->hasShrinkHeader = 0;
		bi->skipErasedCheck = 1;  /* This is clean, so no need to check */
		bi->gcPrioritise = 0;
		yaffs_ClearChunkBits(dev, blockNo);

		T(YAFFS_TRACE_ERASE,
		  (TSTR("Erased block %d" TENDSTR), blockNo));
	} else {
		dev->nFreeChunks -= dev->nChunksPerBlock;	/* We lost a block of free space */

		yaffs_RetireBlock(dev, blockNo);
		T(YAFFS_TRACE_ERROR | YAFFS_TRACE_BAD_BLOCKS,
		  (TSTR("**>> Block %d retired" TENDSTR), blockNo));
	}
}

static int yaffs_FindBlockForAllocation(yaffs_Device *dev)
{
	int i;

	yaffs_BlockInfo *bi;

	if (dev->nErasedBlocks < 1) {
		/* Hoosterman we've got a problem.
		 * Can't get space to gc
		 */
		T(YAFFS_TRACE_ERROR,
		  (TSTR("yaffs tragedy: no more erased blocks" TENDSTR)));

		return -1;
	}

	/* Find an empty block. */

	for (i = dev->internalStartBlock; i <= dev->internalEndBlock; i++) {
		dev->allocationBlockFinder++;
		if (dev->allocationBlockFinder < dev->internalStartBlock
		    || dev->allocationBlockFinder > dev->internalEndBlock) {
			dev->allocationBlockFinder = dev->internalStartBlock;
		}

		bi = yaffs_GetBlockInfo(dev, dev->allocationBlockFinder);

		if (bi->blockState == YAFFS_BLOCK_STATE_EMPTY) {
			bi->blockState = YAFFS_BLOCK_STATE_ALLOCATING;
			dev->sequenceNumber++;
			bi->sequenceNumber = dev->sequenceNumber;
			dev->nErasedBlocks--;
			T(YAFFS_TRACE_ALLOCATE,
			  (TSTR("Allocated block %d, seq  %d, %d left" TENDSTR),
			   dev->allocationBlockFinder, dev->sequenceNumber,
			   dev->nErasedBlocks));
			return dev->allocationBlockFinder;
		}
	}

	T(YAFFS_TRACE_ALWAYS,
	  (TSTR
	   ("yaffs tragedy: no more erased blocks, but there should have been %d"
	    TENDSTR), dev->nErasedBlocks));

	return -1;
}



static int yaffs_CalcCheckpointBlocksRequired(yaffs_Device *dev)
{
	if (!dev->nCheckpointBlocksRequired &&
	   dev->isYaffs2) {
		/* Not a valid value so recalculate */
		int nBytes = 0;
		int nBlocks;
		int devBlocks = (dev->endBlock - dev->startBlock + 1);
		int tnodeSize;

		tnodeSize = (dev->tnodeWidth * YAFFS_NTNODES_LEVEL0)/8;

		if (tnodeSize < sizeof(yaffs_Tnode))
			tnodeSize = sizeof(yaffs_Tnode);

		nBytes += sizeof(yaffs_CheckpointValidity);
		nBytes += sizeof(yaffs_CheckpointDevice);
		nBytes += devBlocks * sizeof(yaffs_BlockInfo);
		nBytes += devBlocks * dev->chunkBitmapStride;
		nBytes += (sizeof(yaffs_CheckpointObject) + sizeof(__u32)) * (dev->nObjectsCreated - dev->nFreeObjects);
		nBytes += (tnodeSize + sizeof(__u32)) * (dev->nTnodesCreated - dev->nFreeTnodes);
		nBytes += sizeof(yaffs_CheckpointValidity);
		nBytes += sizeof(__u32); /* checksum*/

		/* Round up and add 2 blocks to allow for some bad blocks, so add 3 */

		nBlocks = (nBytes/(dev->nDataBytesPerChunk * dev->nChunksPerBlock)) + 3;

		dev->nCheckpointBlocksRequired = nBlocks;
	}

	return dev->nCheckpointBlocksRequired;
}

/*
 * Check if there's space to allocate...
 * Thinks.... do we need top make this ths same as yaffs_GetFreeChunks()?
 */
static int yaffs_CheckSpaceForAllocation(yaffs_Device *dev)
{
	int reservedChunks;
	int reservedBlocks = dev->nReservedBlocks;
	int checkpointBlocks;

	if (dev->isYaffs2) {
		checkpointBlocks =  yaffs_CalcCheckpointBlocksRequired(dev) -
				    dev->blocksInCheckpoint;
		if (checkpointBlocks < 0)
			checkpointBlocks = 0;
	} else {
		checkpointBlocks = 0;
	}

	reservedChunks = ((reservedBlocks + checkpointBlocks) * dev->nChunksPerBlock);

	return (dev->nFreeChunks > reservedChunks);
}

static int yaffs_AllocateChunk(yaffs_Device *dev, int useReserve,
		yaffs_BlockInfo **blockUsedPtr)
{
	int retVal;
	yaffs_BlockInfo *bi;

	if (dev->allocationBlock < 0) {
		/* Get next block to allocate off */
		dev->allocationBlock = yaffs_FindBlockForAllocation(dev);
		dev->allocationPage = 0;
	}

	if (!useReserve && !yaffs_CheckSpaceForAllocation(dev)) {
		/* Not enough space to allocate unless we're allowed to use the reserve. */
		return -1;
	}

	if (dev->nErasedBlocks < dev->nReservedBlocks
			&& dev->allocationPage == 0) {
		T(YAFFS_TRACE_ALLOCATE, (TSTR("Allocating reserve" TENDSTR)));
	}

	/* Next page please.... */
	if (dev->allocationBlock >= 0) {
		bi = yaffs_GetBlockInfo(dev, dev->allocationBlock);

		retVal = (dev->allocationBlock * dev->nChunksPerBlock) +
			dev->allocationPage;
		bi->pagesInUse++;
		yaffs_SetChunkBit(dev, dev->allocationBlock,
				dev->allocationPage);

		dev->allocationPage++;

		dev->nFreeChunks--;

		/* If the block is full set the state to full */
		if (dev->allocationPage >= dev->nChunksPerBlock) {
			bi->blockState = YAFFS_BLOCK_STATE_FULL;
			dev->allocationBlock = -1;
		}

		if (blockUsedPtr)
			*blockUsedPtr = bi;

		return retVal;
	}

	T(YAFFS_TRACE_ERROR,
			(TSTR("!!!!!!!!! Allocator out !!!!!!!!!!!!!!!!!" TENDSTR)));

	return -1;
}

static int yaffs_GetErasedChunks(yaffs_Device *dev)
{
	int n;

	n = dev->nErasedBlocks * dev->nChunksPerBlock;

	if (dev->allocationBlock > 0)
		n += (dev->nChunksPerBlock - dev->allocationPage);

	return n;

}

/*
 * yaffs_SkipRestOfBlock() skips over the rest of the allocation block
 * if we don't want to write to it.
 */
static void yaffs_SkipRestOfBlock(yaffs_Device *dev)
{
	if(dev->allocationBlock > 0){
		yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, dev->allocationBlock);
		if(bi->blockState == YAFFS_BLOCK_STATE_ALLOCATING){
			bi->blockState = YAFFS_BLOCK_STATE_FULL;
			dev->allocationBlock = -1;
		}
	}
}


static int yaffs_GarbageCollectBlock(yaffs_Device *dev, int block,
		int wholeBlock)
{
	int oldChunk;
	int newChunk;
	int markNAND;
	int retVal = YAFFS_OK;
	int cleanups = 0;
	int i;
	int isCheckpointBlock;
	int matchingChunk;
	int maxCopies;

	int chunksBefore = yaffs_GetErasedChunks(dev);
	int chunksAfter;

	yaffs_ExtendedTags tags;

	yaffs_BlockInfo *bi = yaffs_GetBlockInfo(dev, block);

	yaffs_Object *object;

	isCheckpointBlock = (bi->blockState == YAFFS_BLOCK_STATE_CHECKPOINT);


	T(YAFFS_TRACE_TRACING,
			(TSTR("Collecting block %d, in use %d, shrink %d, wholeBlock %d" TENDSTR),
			 block,
			 bi->pagesInUse,
			 bi->hasShrinkHeader,
			 wholeBlock));

	/*yaffs_VerifyFreeChunks(dev); */

	if(bi->blockState == YAFFS_BLOCK_STATE_FULL)
		bi->blockState = YAFFS_BLOCK_STATE_COLLECTING;
	
	bi->hasShrinkHeader = 0;	/* clear the flag so that the block can erase */

	/* Take off the number of soft deleted entries because
	 * they're going to get really deleted during GC.
	 */
	if(dev->gcChunk == 0) /* first time through for this block */
		dev->nFreeChunks -= bi->softDeletions;

	dev->isDoingGC = 1;

	if (isCheckpointBlock ||
			!yaffs_StillSomeChunkBits(dev, block)) {
		T(YAFFS_TRACE_TRACING,
				(TSTR
				 ("Collecting block %d that has no chunks in use" TENDSTR),
				 block));
		yaffs_BlockBecameDirty(dev, block);
	} else {

		__u8 *buffer = yaffs_GetTempBuffer(dev, __LINE__);

		yaffs_VerifyBlock(dev, bi, block);

		maxCopies = (wholeBlock) ? dev->nChunksPerBlock : 10;
		oldChunk = block * dev->nChunksPerBlock + dev->gcChunk;

		for (/* init already done */;
		     retVal == YAFFS_OK &&
		     dev->gcChunk < dev->nChunksPerBlock &&
		     (bi->blockState == YAFFS_BLOCK_STATE_COLLECTING) &&
		     maxCopies > 0;
		     dev->gcChunk++, oldChunk++) {
			if (yaffs_CheckChunkBit(dev, block, dev->gcChunk)) {

				/* This page is in use and might need to be copied off */

				maxCopies--;

				markNAND = 1;

				yaffs_InitialiseTags(&tags);

				yaffs_ReadChunkWithTagsFromNAND(dev, oldChunk,
								buffer, &tags);

				object =
				    yaffs_FindObjectByNumber(dev,
							     tags.objectId);

				T(YAFFS_TRACE_GC_DETAIL,
				  (TSTR
				   ("Collecting chunk in block %d, %d %d %d " TENDSTR),
				   dev->gcChunk, tags.objectId, tags.chunkId,
				   tags.byteCount));

				if (object && !yaffs_SkipVerification(dev)) {
					if (tags.chunkId == 0)
						matchingChunk = object->hdrChunk;
					else if (object->softDeleted)
						matchingChunk = oldChunk; /* Defeat the test */
					else
						matchingChunk = yaffs_FindChunkInFile(object, tags.chunkId, NULL);

					if (oldChunk != matchingChunk)
						T(YAFFS_TRACE_ERROR,
						  (TSTR("gc: page in gc mismatch: %d %d %d %d"TENDSTR),
						  oldChunk, matchingChunk, tags.objectId, tags.chunkId));

				}

				if (!object) {
					T(YAFFS_TRACE_ERROR,
					  (TSTR
					   ("page %d in gc has no object: %d %d %d "
					    TENDSTR), oldChunk,
					    tags.objectId, tags.chunkId, tags.byteCount));
				}

				if (object &&
				    object->deleted &&
				    object->softDeleted &&
				    tags.chunkId != 0) {
					/* Data chunk in a soft deleted file, throw it away
					 * It's a soft deleted data chunk,
					 * No need to copy this, just forget about it and
					 * fix up the object.
					 */

					object->nDataChunks--;

					if (object->nDataChunks <= 0) {
						/* remeber to clean up the object */
						dev->gcCleanupList[cleanups] =
						    tags.objectId;
						cleanups++;
					}
					markNAND = 0;
				} else if (0) {
					/* Todo object && object->deleted && object->nDataChunks == 0 */
					/* Deleted object header with no data chunks.
					 * Can be discarded and the file deleted.
					 */
					object->hdrChunk = 0;
					yaffs_FreeTnode(object->myDev,
							object->variant.
							fileVariant.top);
					object->variant.fileVariant.top = NULL;
					yaffs_DoGenericObjectDeletion(object);

				} else if (object) {
					/* It's either a data chunk in a live file or
					 * an ObjectHeader, so we're interested in it.
					 * NB Need to keep the ObjectHeaders of deleted files
					 * until the whole file has been deleted off
					 */
					tags.serialNumber++;

					dev->nGCCopies++;

					if (tags.chunkId == 0) {
						/* It is an object Id,
						 * We need to nuke the shrinkheader flags first
						 * Also need to clean up shadowing.
						 * We no longer want the shrinkHeader flag since its work is done
						 * and if it is left in place it will mess up scanning.
						 */

						yaffs_ObjectHeader *oh;
						oh = (yaffs_ObjectHeader *)buffer;
						oh->isShrink = 0;
						tags.extraIsShrinkHeader = 0;
						oh->shadowsObject = 0;
						oh->inbandShadowsObject = 0;
						tags.extraShadows = 0;

						yaffs_VerifyObjectHeader(object, oh, &tags, 1);
					}

					newChunk =
					    yaffs_WriteNewChunkWithTagsToNAND(dev, buffer, &tags, 1);

					if (newChunk < 0) {
						retVal = YAFFS_FAIL;
					} else {

						/* Ok, now fix up the Tnodes etc. */

						if (tags.chunkId == 0) {
							/* It's a header */
							object->hdrChunk =  newChunk;
							object->serial =   tags.serialNumber;
						} else {
							/* It's a data chunk */
							yaffs_PutChunkIntoFile
							    (object,
							     tags.chunkId,
							     newChunk, 0);
						}
					}
				}

				if (retVal == YAFFS_OK)
					yaffs_DeleteChunk(dev, oldChunk, markNAND, __LINE__);

			}
		}

		yaffs_ReleaseTempBuffer(dev, buffer, __LINE__);


		/* Do any required cleanups */
		for (i = 0; i < cleanups; i++) {
			/* Time to delete the file too */
			object =
			    yaffs_FindObjectByNumber(dev,
						     dev->gcCleanupList[i]);
			if (object) {
				yaffs_FreeTnode(dev,
						object->variant.fileVariant.
						top);
				object->variant.fileVariant.top = NULL;
				T(YAFFS_TRACE_GC,
				  (TSTR
				   ("yaffs: About to finally delete object %d"
				    TENDSTR), object->objectId));
				yaffs_DoGenericObjectDeletion(object);
				object->myDev->nDeletedFiles--;
			}

		}

	}

	yaffs_VerifyCollectedBlock(dev, bi, block);

	chunksAfter = yaffs_GetErasedChunks(dev);
	if (chunksBefore >= chunksAfter) {
		T(YAFFS_TRACE_GC,
		  (TSTR
		   ("gc did not increase free chunks before %d after %d"
		    TENDSTR), chunksBefore, chunksAfter));
	}

	/* If the gc completed then clear the current gcBlock so that we find another. */
	if (bi->blockState != YAFFS_BLOCK_STATE_COLLECTING) {
		dev->gcBlock = -1;
		dev->gcChunk = 0;
	}

	dev->isDoingGC = 0;

	return retVal;
}

/* New garbage collector
 * If we're very low on erased blocks then we do aggressive garbage collection
 * otherwise we do "leasurely" garbage collection.
 * Aggressive gc looks further (whole array) and will accept less dirty blocks.
 * Passive gc only inspects smaller areas and will only accept more dirty blocks.
 *
 * The idea is to help clear out space in a more spread-out manner.
 * Dunno if it really does anything useful.
 */
static int yaffs_CheckGarbageCollection(yaffs_Device *dev)
{
	int block;
	int aggressive;
	int gcOk = YAFFS_OK;
	int maxTries = 0;

	int checkpointBlockAdjust;

	if (dev->isDoingGC) {
		/* Bail out so we don't get recursive gc */
		return YAFFS_OK;
	}

	/* This loop should pass the first time.
	 * We'll only see looping here if the erase of the collected block fails.
	 */

	do {
		maxTries++;

		checkpointBlockAdjust = yaffs_CalcCheckpointBlocksRequired(dev) - dev->blocksInCheckpoint;
		if (checkpointBlockAdjust < 0)
			checkpointBlockAdjust = 0;

		if (dev->nErasedBlocks < (dev->nReservedBlocks + checkpointBlockAdjust + 2)) {
			/* We need a block soon...*/
			aggressive = 1;
		} else {
			/* We're in no hurry */
			aggressive = 0;
		}

		if (dev->gcBlock <= 0) {
			dev->gcBlock = yaffs_FindBlockForGarbageCollection(dev, aggressive);
			dev->gcChunk = 0;
		}

		block = dev->gcBlock;

		if (block > 0) {
			dev->garbageCollections++;
			if (!aggressive)
				dev->passiveGarbageCollections++;

			T(YAFFS_TRACE_GC,
			  (TSTR
			   ("yaffs: GC erasedBlocks %d aggressive %d" TENDSTR),
			   dev->nErasedBlocks, aggressive));

			gcOk = yaffs_GarbageCollectBlock(dev, block, aggressive);
		}

		if (dev->nErasedBlocks < (dev->nReservedBlocks) && block > 0) {
			T(YAFFS_TRACE_GC,
			  (TSTR
			   ("yaffs: GC !!!no reclaim!!! erasedBlocks %d after try %d block %d"
			    TENDSTR), dev->nErasedBlocks, maxTries, block));
		}
	} while ((dev->nErasedBlocks < dev->nReservedBlocks) &&
		 (block > 0) &&
		 (maxTries < 2));

	return aggressive ? gcOk : YAFFS_OK;
}

/*-------------------------  TAGS --------------------------------*/

static int yaffs_TagsMatch(const yaffs_ExtendedTags *tags, int objectId,
			   int chunkInObject)
{
	return (tags->chunkId == chunkInObject &&
		tags->objectId == objectId && !tags->chunkDeleted) ? 1 : 0;

}


/*-------------------- Data file manipulation -----------------*/

static int yaffs_FindChunkInFile(yaffs_Object *in, int chunkInInode,
				 yaffs_ExtendedTags *tags)
{
	/*Get the Tnode, then get the level 0 offset chunk offset */
	yaffs_Tnode *tn;
	int theChunk = -1;
	yaffs_ExtendedTags localTags;
	int retVal = -1;

	yaffs_Device *dev = in->myDev;

	if (!tags) {
		/* Passed a NULL, so use our own tags space */
		tags = &localTags;
	}

	tn = yaffs_FindLevel0Tnode(dev, &in->variant.fileVariant, chunkInInode);

	if (tn) {
		theChunk = yaffs_GetChunkGroupBase(dev, tn, chunkInInode);

		retVal =
		    yaffs_FindChunkInGroup(dev, theChunk, tags, in->objectId,
					   chunkInInode);
	}
	return retVal;
}

static int yaffs_FindAndDeleteChunkInFile(yaffs_Object *in, int chunkInInode,
					  yaffs_ExtendedTags *tags)
{
	/* Get the Tnode, then get the level 0 offset chunk offset */
	yaffs_Tnode *tn;
	int theChunk = -1;
	yaffs_ExtendedTags localTags;

	yaffs_Device *dev = in->myDev;
	int retVal = -1;

	if (!tags) {
		/* Passed a NULL, so use our own tags space */
		tags = &localTags;
	}

	tn = yaffs_FindLevel0Tnode(dev, &in->variant.fileVariant, chunkInInode);

	if (tn) {

		theChunk = yaffs_GetChunkGroupBase(dev, tn, chunkInInode);

		retVal =
		    yaffs_FindChunkInGroup(dev, theChunk, tags, in->objectId,
					   chunkInInode);

		/* Delete the entry in the filestructure (if found) */
		if (retVal != -1)
			yaffs_LoadLevel0Tnode(dev, tn, chunkInInode, 0);
	}

	return retVal;
}

#ifdef YAFFS_PARANOID

static int yaffs_CheckFileSanity(yaffs_Object *in)
{
	int chunk;
	int nChunks;
	int fSize;
	int failed = 0;
	int objId;
	yaffs_Tnode *tn;
	yaffs_Tags localTags;
	yaffs_Tags *tags = &localTags;
	int theChunk;
	int chunkDeleted;

	if (in->variantType != YAFFS_OBJECT_TYPE_FILE)
		return YAFFS_FAIL;

	objId = in->objectId;
	fSize = in->variant.fileVariant.fileSize;
	nChunks =
	    (fSize + in->myDev->nDataBytesPerChunk - 1) / in->myDev->nDataBytesPerChunk;

	for (chunk = 1; chunk <= nChunks; chunk++) {
		tn = yaffs_FindLevel0Tnode(in->myDev, &in->variant.fileVariant,
					   chunk);

		if (tn) {

			theChunk = yaffs_GetChunkGroupBase(dev, tn, chunk);

			if (yaffs_CheckChunkBits
			    (dev, theChunk / dev->nChunksPerBlock,
			     theChunk % dev->nChunksPerBlock)) {

				yaffs_ReadChunkTagsFromNAND(in->myDev, theChunk,
							    tags,
							    &chunkDeleted);
				if (yaffs_TagsMatch
				    (tags, in->objectId, chunk, chunkDeleted)) {
					/* found it; */

				}
			} else {

				failed = 1;
			}

		} else {
			/* T(("No level 0 found for %d\n", chunk)); */
		}
	}

	return failed ? YAFFS_FAIL : YAFFS_OK;
}

#endif

static int yaffs_PutChunkIntoFile(yaffs_Object *in, int chunkInInode,
				  int chunkInNAND, int inScan)
{
	/* NB inScan is zero unless scanning.
	 * For forward scanning, inScan is > 0;
	 * for backward scanning inScan is < 0
	 *
	 * chunkInNAND = 0 is a dummy insert to make sure the tnodes are there.
	 */

	yaffs_Tnode *tn;
	yaffs_Device *dev = in->myDev;
	int existingChunk;
	yaffs_ExtendedTags existingTags;
	yaffs_ExtendedTags newTags;
	unsigned existingSerial, newSerial;

	if (in->variantType != YAFFS_OBJECT_TYPE_FILE) {
		/* Just ignore an attempt at putting a chunk into a non-file during scanning
		 * If it is not during Scanning then something went wrong!
		 */
		if (!inScan) {
			T(YAFFS_TRACE_ERROR,
			  (TSTR
			   ("yaffs tragedy:attempt to put data chunk into a non-file"
			    TENDSTR)));
			YBUG();
		}

		yaffs_DeleteChunk(dev, chunkInNAND, 1, __LINE__);
		return YAFFS_OK;
	}

	tn = yaffs_AddOrFindLevel0Tnode(dev,
					&in->variant.fileVariant,
					chunkInInode,
					NULL);
	if (!tn)
		return YAFFS_FAIL;
	
	if(!chunkInNAND)
		/* Dummy insert, bail now */
		return YAFFS_OK;
		

	existingChunk = yaffs_GetChunkGroupBase(dev, tn, chunkInInode);

	if (inScan != 0) {
		/* If we're scanning then we need to test for duplicates
		 * NB This does not need to be efficient since it should only ever
		 * happen when the power fails during a write, then only one
		 * chunk should ever be affected.
		 *
		 * Correction for YAFFS2: This could happen quite a lot and we need to think about efficiency! TODO
		 * Update: For backward scanning we don't need to re-read tags so this is quite cheap.
		 */

		if (existingChunk > 0) {
			/* NB Right now existing chunk will not be real chunkId if the device >= 32MB
			 *    thus we have to do a FindChunkInFile to get the real chunk id.
			 *
			 * We have a duplicate now we need to decide which one to use:
			 *
			 * Backwards scanning YAFFS2: The old one is what we use, dump the new one.
			 * Forward scanning YAFFS2: The new one is what we use, dump the old one.
			 * YAFFS1: Get both sets of tags and compare serial numbers.
			 */

			if (inScan > 0) {
				/* Only do this for forward scanning */
				yaffs_ReadChunkWithTagsFromNAND(dev,
								chunkInNAND,
								NULL, &newTags);

				/* Do a proper find */
				existingChunk =
				    yaffs_FindChunkInFile(in, chunkInInode,
							  &existingTags);
			}

			if (existingChunk <= 0) {
				/*Hoosterman - how did this happen? */

				T(YAFFS_TRACE_ERROR,
				  (TSTR
				   ("yaffs tragedy: existing chunk < 0 in scan"
				    TENDSTR)));

			}

			/* NB The deleted flags should be false, otherwise the chunks will
			 * not be loaded during a scan
			 */

			if (inScan > 0) {
				newSerial = newTags.serialNumber;
				existingSerial = existingTags.serialNumber;
			}

			if ((inScan > 0) &&
			    (in->myDev->isYaffs2 ||
			     existingChunk <= 0 ||
			     ((existingSerial + 1) & 3) == newSerial)) {
				/* Forward scanning.
				 * Use new
				 * Delete the old one and drop through to update the tnode
				 */
				yaffs_DeleteChunk(dev, existingChunk, 1,
						  __LINE__);
			} else {
				/* Backward scanning or we want to use the existing one
				 * Use existing.
				 * Delete the new one and return early so that the tnode isn't changed
				 */
				yaffs_DeleteChunk(dev, chunkInNAND, 1,
						  __LINE__);
				return YAFFS_OK;
			}
		}

	}

	if (existingChunk == 0)
		in->nDataChunks++;

	yaffs_LoadLevel0Tnode(dev, tn, chunkInInode, chunkInNAND);

	return YAFFS_OK;
}

static int yaffs_ReadChunkDataFromObject(yaffs_Object *in, int chunkInInode,
					__u8 *buffer)
{
	int chunkInNAND = yaffs_FindChunkInFile(in, chunkInInode, NULL);

	if (chunkInNAND >= 0)
		return yaffs_ReadChunkWithTagsFromNAND(in->myDev, chunkInNAND,
						buffer, NULL);
	else {
		T(YAFFS_TRACE_NANDACCESS,
		  (TSTR("Chunk %d not found zero instead" TENDSTR),
		   chunkInNAND));
		/* get sane (zero) data if you read a hole */
		memset(buffer, 0, in->myDev->nDataBytesPerChunk);
		return 0;
	}

}

void yaffs_DeleteChunk(yaffs_Device *dev, int chunkId, int markNAND, int lyn)
{
	int block;
	int page;
	yaffs_ExtendedTags tags;
	yaffs_BlockInfo *bi;

	if (chunkId <= 0)
		return;

	dev->nDeletions++;
	block = chunkId / dev->nChunksPerBlock;
	page = chunkId % dev->nChunksPerBlock;


	if (!yaffs_CheckChunkBit(dev, block, page))
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Deleting invalid chunk %d"TENDSTR),
			 chunkId));

	bi = yaffs_GetBlockInfo(dev, block);

	T(YAFFS_TRACE_DELETION,
	  (TSTR("line %d delete of chunk %d" TENDSTR), lyn, chunkId));

	if (markNAND &&
	    bi->blockState != YAFFS_BLOCK_STATE_COLLECTING && !dev->isYaffs2) {

		yaffs_InitialiseTags(&tags);

		tags.chunkDeleted = 1;

		yaffs_WriteChunkWithTagsToNAND(dev, chunkId, NULL, &tags);
		yaffs_HandleUpdateChunk(dev, chunkId, &tags);
	} else {
		dev->nUnmarkedDeletions++;
	}

	/* Pull out of the management area.
	 * If the whole block became dirty, this will kick off an erasure.
	 */
	if (bi->blockState == YAFFS_BLOCK_STATE_ALLOCATING ||
	    bi->blockState == YAFFS_BLOCK_STATE_FULL ||
	    bi->blockState == YAFFS_BLOCK_STATE_NEEDS_SCANNING ||
	    bi->blockState == YAFFS_BLOCK_STATE_COLLECTING) {
		dev->nFreeChunks++;

		yaffs_ClearChunkBit(dev, block, page);

		bi->pagesInUse--;

		if (bi->pagesInUse == 0 &&
		    !bi->hasShrinkHeader &&
		    bi->blockState != YAFFS_BLOCK_STATE_ALLOCATING &&
		    bi->blockState != YAFFS_BLOCK_STATE_NEEDS_SCANNING) {
			yaffs_BlockBecameDirty(dev, block);
		}

	}

}

static int yaffs_WriteChunkDataToObject(yaffs_Object *in, int chunkInInode,
					const __u8 *buffer, int nBytes,
					int useReserve)
{
	/* Find old chunk Need to do this to get serial number
	 * Write new one and patch into tree.
	 * Invalidate old tags.
	 */

	int prevChunkId;
	yaffs_ExtendedTags prevTags;

	int newChunkId;
	yaffs_ExtendedTags newTags;

	yaffs_Device *dev = in->myDev;

	yaffs_CheckGarbageCollection(dev);

	/* Get the previous chunk at this location in the file if it exists */
	prevChunkId = yaffs_FindChunkInFile(in, chunkInInode, &prevTags);

	/* Set up new tags */
	yaffs_InitialiseTags(&newTags);

	newTags.chunkId = chunkInInode;
	newTags.objectId = in->objectId;
	newTags.serialNumber =
	    (prevChunkId > 0) ? prevTags.serialNumber + 1 : 1;
	newTags.byteCount = nBytes;

	if (nBytes < 1 || nBytes > dev->totalBytesPerChunk) {
		T(YAFFS_TRACE_ERROR,
		(TSTR("Writing %d bytes to chunk!!!!!!!!!" TENDSTR), nBytes));
		YBUG();
	}
	
	/*
	 * If there isn't already a chunk there then do a dummy
	 * insert to make sue we have the desired tnode structure.
	 */
	if(prevChunkId < 1 &&
		yaffs_PutChunkIntoFile(in, chunkInInode, 0, 0) != YAFFS_OK)
		return -1;
		
	newChunkId =
	    yaffs_WriteNewChunkWithTagsToNAND(dev, buffer, &newTags,
					      useReserve);

	if (newChunkId > 0) {
		yaffs_PutChunkIntoFile(in, chunkInInode, newChunkId, 0);

		if (prevChunkId > 0)
			yaffs_DeleteChunk(dev, prevChunkId, 1, __LINE__);

		yaffs_CheckFileSanity(in);
	}
	return newChunkId;

}

/* UpdateObjectHeader updates the header on NAND for an object.
 * If name is not NULL, then that new name is used.
 */
int yaffs_UpdateObjectHeader(yaffs_Object *in, const YCHAR *name, int force,
			     int isShrink, int shadows)
{

	yaffs_BlockInfo *bi;

	yaffs_Device *dev = in->myDev;

	int prevChunkId;
	int retVal = 0;
	int result = 0;

	int newChunkId;
	yaffs_ExtendedTags newTags;
	yaffs_ExtendedTags oldTags;
	YCHAR *alias = NULL;

	__u8 *buffer = NULL;
	YCHAR oldName[YAFFS_MAX_NAME_LENGTH + 1];

	yaffs_ObjectHeader *oh = NULL;

	yaffs_strcpy(oldName, _Y("silly old name"));


	if (!in->fake ||
		in == dev->rootDir || /* The rootDir should also be saved */
		force) {

		yaffs_CheckGarbageCollection(dev);
		yaffs_CheckObjectDetailsLoaded(in);

		buffer = yaffs_GetTempBuffer(in->myDev, __LINE__);
		oh = (yaffs_ObjectHeader *) buffer;

		prevChunkId = in->hdrChunk;

		if (prevChunkId > 0) {
			result = yaffs_ReadChunkWithTagsFromNAND(dev, prevChunkId,
							buffer, &oldTags);

			yaffs_VerifyObjectHeader(in, oh, &oldTags, 0);

			memcpy(oldName, oh->name, sizeof(oh->name));
		}

		memset(buffer, 0xFF, dev->nDataBytesPerChunk);

		oh->type = in->variantType;
		oh->yst_mode = in->yst_mode;
		oh->shadowsObject = oh->inbandShadowsObject = shadows;

#ifdef CONFIG_YAFFS_WINCE
		oh->win_atime[0] = in->win_atime[0];
		oh->win_ctime[0] = in->win_ctime[0];
		oh->win_mtime[0] = in->win_mtime[0];
		oh->win_atime[1] = in->win_atime[1];
		oh->win_ctime[1] = in->win_ctime[1];
		oh->win_mtime[1] = in->win_mtime[1];
#else
		oh->yst_uid = in->yst_uid;
		oh->yst_gid = in->yst_gid;
		oh->yst_atime = in->yst_atime;
		oh->yst_mtime = in->yst_mtime;
		oh->yst_ctime = in->yst_ctime;
		oh->yst_rdev = in->yst_rdev;
#endif
		if (in->parent)
			oh->parentObjectId = in->parent->objectId;
		else
			oh->parentObjectId = 0;

		if (name && *name) {
			memset(oh->name, 0, sizeof(oh->name));
			yaffs_strncpy(oh->name, name, YAFFS_MAX_NAME_LENGTH);
		} else if (prevChunkId > 0)
			memcpy(oh->name, oldName, sizeof(oh->name));
		else
			memset(oh->name, 0, sizeof(oh->name));

		oh->isShrink = isShrink;

		switch (in->variantType) {
		case YAFFS_OBJECT_TYPE_UNKNOWN:
			/* Should not happen */
			break;
		case YAFFS_OBJECT_TYPE_FILE:
			oh->fileSize =
			    (oh->parentObjectId == YAFFS_OBJECTID_DELETED
			     || oh->parentObjectId ==
			     YAFFS_OBJECTID_UNLINKED) ? 0 : in->variant.
			    fileVariant.fileSize;
			break;
		case YAFFS_OBJECT_TYPE_HARDLINK:
			oh->equivalentObjectId =
			    in->variant.hardLinkVariant.equivalentObjectId;
			break;
		case YAFFS_OBJECT_TYPE_SPECIAL:
			/* Do nothing */
			break;
		case YAFFS_OBJECT_TYPE_DIRECTORY:
			/* Do nothing */
			break;
		case YAFFS_OBJECT_TYPE_SYMLINK:
			alias = in->variant.symLinkVariant.alias;
			if(!alias)
				alias = _Y("no alias");
			yaffs_strncpy(oh->alias,
					alias,
				      YAFFS_MAX_ALIAS_LENGTH);
			oh->alias[YAFFS_MAX_ALIAS_LENGTH] = 0;
			break;
		}

		/* Tags */
		yaffs_InitialiseTags(&newTags);
		in->serial++;
		newTags.chunkId = 0;
		newTags.objectId = in->objectId;
		newTags.serialNumber = in->serial;

		/* Add extra info for file header */

		newTags.extraHeaderInfoAvailable = 1;
		newTags.extraParentObjectId = oh->parentObjectId;
		newTags.extraFileLength = oh->fileSize;
		newTags.extraIsShrinkHeader = oh->isShrink;
		newTags.extraEquivalentObjectId = oh->equivalentObjectId;
		newTags.extraShadows = (oh->shadowsObject > 0) ? 1 : 0;
		newTags.extraObjectType = in->variantType;

		yaffs_VerifyObjectHeader(in, oh, &newTags, 1);

		/* Create new chunk in NAND */
		newChunkId =
		    yaffs_WriteNewChunkWithTagsToNAND(dev, buffer, &newTags,
						      (prevChunkId > 0) ? 1 : 0);

		if (newChunkId >= 0) {

			in->hdrChunk = newChunkId;

			if (prevChunkId > 0) {
				yaffs_DeleteChunk(dev, prevChunkId, 1,
						  __LINE__);
			}

			if (!yaffs_ObjectHasCachedWriteData(in))
				in->dirty = 0;

			/* If this was a shrink, then mark the block that the chunk lives on */
			if (isShrink) {
				bi = yaffs_GetBlockInfo(in->myDev,
					newChunkId / in->myDev->nChunksPerBlock);
				bi->hasShrinkHeader = 1;
			}

		}

		retVal = newChunkId;

	}

	if (buffer)
		yaffs_ReleaseTempBuffer(dev, buffer, __LINE__);

	return retVal;
}

/*------------------------ Short Operations Cache ----------------------------------------
 *   In many situations where there is no high level buffering (eg WinCE) a lot of
 *   reads might be short sequential reads, and a lot of writes may be short
 *   sequential writes. eg. scanning/writing a jpeg file.
 *   In these cases, a short read/write cache can provide a huge perfomance benefit
 *   with dumb-as-a-rock code.
 *   In Linux, the page cache provides read buffering aand the short op cache provides write
 *   buffering.
 *
 *   There are a limited number (~10) of cache chunks per device so that we don't
 *   need a very intelligent search.
 */

static int yaffs_ObjectHasCachedWriteData(yaffs_Object *obj)
{
	yaffs_Device *dev = obj->myDev;
	int i;
	yaffs_ChunkCache *cache;
	int nCaches = obj->myDev->nShortOpCaches;

	for (i = 0; i < nCaches; i++) {
		cache = &dev->srCache[i];
		if (cache->object == obj &&
		    cache->dirty)
			return 1;
	}

	return 0;
}


static void yaffs_FlushFilesChunkCache(yaffs_Object *obj)
{
	yaffs_Device *dev = obj->myDev;
	int lowest = -99;	/* Stop compiler whining. */
	int i;
	yaffs_ChunkCache *cache;
	int chunkWritten = 0;
	int nCaches = obj->myDev->nShortOpCaches;

	if (nCaches > 0) {
		do {
			cache = NULL;

			/* Find the dirty cache for this object with the lowest chunk id. */
			for (i = 0; i < nCaches; i++) {
				if (dev->srCache[i].object == obj &&
				    dev->srCache[i].dirty) {
					if (!cache
					    || dev->srCache[i].chunkId <
					    lowest) {
						cache = &dev->srCache[i];
						lowest = cache->chunkId;
					}
				}
			}

			if (cache && !cache->locked) {
				/* Write it out and free it up */

				chunkWritten =
				    yaffs_WriteChunkDataToObject(cache->object,
								 cache->chunkId,
								 cache->data,
								 cache->nBytes,
								 1);
				cache->dirty = 0;
				cache->object = NULL;
			}

		} while (cache && chunkWritten > 0);

		if (cache) {
			/* Hoosterman, disk full while writing cache out. */
			T(YAFFS_TRACE_ERROR,
			  (TSTR("yaffs tragedy: no space during cache write" TENDSTR)));

		}
	}

}

/*yaffs_FlushEntireDeviceCache(dev)
 *
 *
 */

void yaffs_FlushEntireDeviceCache(yaffs_Device *dev)
{
	yaffs_Object *obj;
	int nCaches = dev->nShortOpCaches;
	int i;

	/* Find a dirty object in the cache and flush it...
	 * until there are no further dirty objects.
	 */
	do {
		obj = NULL;
		for (i = 0; i < nCaches && !obj; i++) {
			if (dev->srCache[i].object &&
			    dev->srCache[i].dirty)
				obj = dev->srCache[i].object;

		}
		if (obj)
			yaffs_FlushFilesChunkCache(obj);

	} while (obj);

}


/* Grab us a cache chunk for use.
 * First look for an empty one.
 * Then look for the least recently used non-dirty one.
 * Then look for the least recently used dirty one...., flush and look again.
 */
static yaffs_ChunkCache *yaffs_GrabChunkCacheWorker(yaffs_Device *dev)
{
	int i;

	if (dev->nShortOpCaches > 0) {
		for (i = 0; i < dev->nShortOpCaches; i++) {
			if (!dev->srCache[i].object)
				return &dev->srCache[i];
		}
	}

	return NULL;
}

static yaffs_ChunkCache *yaffs_GrabChunkCache(yaffs_Device *dev)
{
	yaffs_ChunkCache *cache;
	yaffs_Object *theObj;
	int usage;
	int i;
	int pushout;

	if (dev->nShortOpCaches > 0) {
		/* Try find a non-dirty one... */

		cache = yaffs_GrabChunkCacheWorker(dev);

		if (!cache) {
			/* They were all dirty, find the last recently used object and flush
			 * its cache, then  find again.
			 * NB what's here is not very accurate, we actually flush the object
			 * the last recently used page.
			 */

			/* With locking we can't assume we can use entry zero */

			theObj = NULL;
			usage = -1;
			cache = NULL;
			pushout = -1;

			for (i = 0; i < dev->nShortOpCaches; i++) {
				if (dev->srCache[i].object &&
				    !dev->srCache[i].locked &&
				    (dev->srCache[i].lastUse < usage || !cache)) {
					usage = dev->srCache[i].lastUse;
					theObj = dev->srCache[i].object;
					cache = &dev->srCache[i];
					pushout = i;
				}
			}

			if (!cache || cache->dirty) {
				/* Flush and try again */
				yaffs_FlushFilesChunkCache(theObj);
				cache = yaffs_GrabChunkCacheWorker(dev);
			}

		}
		return cache;
	} else
		return NULL;

}

/* Find a cached chunk */
static yaffs_ChunkCache *yaffs_FindChunkCache(const yaffs_Object *obj,
					      int chunkId)
{
	yaffs_Device *dev = obj->myDev;
	int i;
	if (dev->nShortOpCaches > 0) {
		for (i = 0; i < dev->nShortOpCaches; i++) {
			if (dev->srCache[i].object == obj &&
			    dev->srCache[i].chunkId == chunkId) {
				dev->cacheHits++;

				return &dev->srCache[i];
			}
		}
	}
	return NULL;
}

/* Mark the chunk for the least recently used algorithym */
static void yaffs_UseChunkCache(yaffs_Device *dev, yaffs_ChunkCache *cache,
				int isAWrite)
{

	if (dev->nShortOpCaches > 0) {
	FS: Yet AnsrLastUse < 0 || AND-flash speci> 10pyrightystem.	/* Reset the csh F usages */ 200int i; 200for (i = 1; i <le sysother Flash Fi; i++) 200	AND-flaash F[i].lh speci= 0;
 200AND-flash speciharle		}es@aAND-flash spec++les@aOne L->nning <chaAND-flash specles@aS: YisAW/*
 * 	e; you cadirtyd and
	}
}

/* Invalidate a singlh One Ltpage.
 * Do this when a whol as
 * gets written,publieleph sher  One Ltill ed bytwareis no longer blic  pub/
static void yaffs_Public LicChunkash F(09-12-Object *o
#inc, oby c chaId* YAify
 de "yp->myDt Another Flash File System.09-12-5 charles  *One Lt=009-12-Find5 charles Ede "ypor.h"

#in modify
 One Ler the termsde "yp = NULLNU General Public Licenllleph One Lts
 *s associated withs_c_vede "yppublished by theevets_cer filon =
delet.h"or resizes_guts.c,v 1.101 2009-12-25 01:53:0We So5 charles Exp $";

#incluinncludeby Chur09-12-Device *dev = interfaceles@S: Yet Another Flash File System.al Public Liceit.  for ill Ltd a0d Brightstar Engineering
 *
 * C) 200 A NAND-flaharles Mampat.h"
yaff Created by Charles Mampat.h"
#ifndef 	}U General-kInNAND,
		int erase Checkpointing kInNAND,
		int erase*/

.c,v 1.1VE_G09-12-t undk);
staticVlic ityMarkerExp $";

#include ortenvheainclud"
#inclu chunkInNAND,
		 cples@memset(&cp, 0, #incof(cp).h"

cp.structTypvaliteChunk(ya;_Devimagic = YAFFS_MAGICD,
			versiont yaffs_ECHECKPOINT_VERSIOND,
			yaff = (yaffs ? 1 :arles@return Exp $";ags *tags)t undYet , ndleUteChunk(yafyaffs_UnlinkObje?
		_Update}kOk(yaffs_Device *dReadnt chunkInNAND,
				const __u8 *data,
				const yaffs_ExtendedTags *tags);
static voidIVE_Gokles@okc vofs_Object *obj);HasCic int yaffs_UnlinkObject(yaffs_Objec"


/* Roker tct *haevice *dev, intt(yaffs_Object&&
		 xten			c	const  yaffs_Extendyaffs_ExtendedTa *tags);


/* Other local prototypeyaffs_ExtendedTatatic = (oid yaffs_Updayaffarent(yaokffs_Updatetic int ya01 2009-12-

#incToags *tags)

#incardList);

static

#inclucp,CreatExte __u8 *data,
				ncludcp->nErasedBlockstribute atic void yafD,
		->allocationid yatribute s_Object *direcry(yaffs_Object *Pwareory,
				yaffs_Ob yafry(yaffnFrekedtagffs_AddObjject *in, fs_Dev->nDeckptrFileffs_AddObjorce, int isyaffs_ObUnlink int isShrink, ivoid yaffs_Reyaffs_ObBackgroundorce,ionffs_AddObjObject *obj);
statiry(yaffsequenceNumbertribute itatic int yafry(yaffoldestDof tStatic ifs_Updat*in, yaffs_Tnode *t;
);

static yaffs_ObjecDevice *dev, intToObject(yaffs_*data,
				cer,
					yaffs_Device *dev, int numncludAddObjectToDirecto = ;
static void yaf; *yaffss_Object *directoryaffs_Object *direcint blockNo);


st yaffs_j);
static int yafint blocbject *in, confs_Object *in, les@ink, int shadows);atic intnt shadows);
steObjectFromDirectoratic inttFromDirectory(ynt yaffs_CheckStructures(atic intfs_CheckStructures(voiDeleteWorker(yaffs_atic ineWorker(yaffs_Obj__u32 level,
			int chunatic in level,
			int chunkO}nkOk(yaffs_Device *dev, int chunkInenericObjectDeletion(yafs_ExtendedTags *tags)

#incldev, __u32 nBytory(yyaffs_Obd yaffs_Yet AninternalEnoid ya -ibute _VerifyDStartctory(+ 1affs_Daffs_Obje/* t undibutinclruntime yafues for_Object *yaffs_CreateNewObject(ndleUvoidD,
			ce *dev, int chunkInNAND,ject *hardList);

static
static int yaffs_UnlinkObject(yaffs_ObjecARANOID
statibtory(info  forDevice stem.Object =static vo*(yaffs_O09-12-atic Infofs_Catic void yaffs_InvalidateWholeChuet An_Invac vo,_Objectject(tic intNU GeANOID
stati.h"

 bit   forkCache(yaffs_Object *object, inet An.h"

BitmapStridnt caffs_InvalidateCheckpoint(yaffs_Device ags *tagstatic int yaffs_FindChunkarent(y	, int inScan)atic int yaffs_ObjectHasCachedWriteevice *dev);

static void yaffs_CheckObjectDetailsLoaded(yaffs_Object *in);

static void yaffs_VerifyDirectory(yaffs_Object *directory);
#ifdef YAFFS_PARANct *hardList);

static int yaffs_WriteNewChunkWithTagsToNAND(yaffint c!ce *derent(yarles@inclueckFileSanity!t(yaffs_Objec		yaffs_Extendaffs_CheckObjectDetailsLoGenericOc int yatic v_Object *object, int chunkId);

static void kWritten(yaffs_Device *dev,
					ivice *dev);

static int yaffs_FindCh*data,
					yaffs_Extennode,
				yaffs_ExtendedTags *tags);

static,
		__u32 *offsetOut)
{
	int chunk;
	__ice *dev, yaffs_Tnode *tn,
		unnkInNAND, int inScan);

static yaffs_Objec

#incfs_CreateNew

#incardList);

static

#incluumber,
					yaffs_

#include * YAFF **bl
#incIic vobjunk;
		offry(yaffparenoffsetffs_chunkBasaffs

	*chunkOuunk;
		offspdaterChunhdr5 chaset = (_/* Functet;
}

varianev, int 

	*cr of shiftset;
}

heckptrwfor a pheckptrvoid);
softorce, i
 * equa number
 * et;
}

uoid yaf
 * equa for allet;
}

faks for a phis et;
}

renameAllowll possibl be hellishlyo cater for allishly efficie2 ShiftsGE(__void);
strialNote we d nShiyaffs_ObData *in, con

	*caBits = 0;
ffs_Devic a power of 2 gyaffs_PutCOBJECT_TYPE_FILEalcund thileSizeOrEquivalenkBase =ffset = (_r of sh.f (eVShifts;
}

xtra;
	else (x & 1)
			extraBits++;
		x >>= 1;
		nShifHARDLINK;
	}

	if (extraBits)
		nShifts++;

	return nShifts;hardLink/* Functes)
		nShifts++;

tatic int yaffs_Object;

		chunkBase =Todev->chunkDiv

#include ,>= 1;
		nShifts++;
	}

ockInfo nity(yaf

#incluunkBas	if (x & 1)
			extraBits+!tic inr of shifts(yaffsT(affs_ETRACE_ERROR, (TSTR("ags *tags) raticmpat.h"%d tits+%d "r thTCONT("s_Obje%d doe=
  t match exisc voimpat.h"0, size"er thTENDSTR),o **blts++;

{
		der of shifts{
		de/* Functfs_Ob 1)
			extraBitsu8 *d>chunkShift)nkIn = (__u32)(adnfo **blwhile (!(dedTags chunkBase)alcuunkBasalidity.h"
#iOrCreate

#incBynt yaf(er,
		 1)
	rfacefs_Objehunk);
	}

	fs_Obje		x >>= 1;
		nShifDIRECTORY8 *drn teturn buf ? ifndefdedTaghunkOut tem. A N;
	*offsempBuffers(yafflineNo)
{
	int i, j;

	dev->C) 200 i;
	__u8 *bufALWAYS_u8 *)1;

	memset(dev->tempBuffer, rn buf r, 0, sizef(devv->tempBfs_Obje%d Pn buf 0, s, %d,r (i directoryBUFFERRS; i++) {mp) {		dev->tempBuffer[vice *devffer[i].line = 0;>maxTemp ot in use == 0) ev->tempBuffer[i].b>chunkShift)signity(yafAdd;
	}

	rDdev->maxp)
		de,< YAdChunkIn to return thatic ineturn the n 1)
			extraBits++ffer[i].line = 0FERS,
	 than or
 *ter than orFERS,
	  number
 * Notiven number
 * FERS,
	  for all poater for allFERS,
	 his doend this FERS,
	  be hellishly efo
 * be hellishlyYAFFS_N_TEMP_Bev->tempBuffer
	int extraBits;;

	nShiftsfs_Deviceifts = hile (x > 1) {
	 *obj);

 > 1) {
		if (x & 1)
			extraBits++;
		x >>= 1;
		nShifts++;
	}turn nShifts;
}

/* Function to rBUFFERS, (extraBits)
		nShifts++;

eturn the number of shifts to get a 1 in bit 0
 */

static __u
	nShifts =  0;

	if (!x)
		return 0;

	while (v->nDataBytesPerChunk);

}

void yafff (x & 1)
	/* Functie Sy    int lazyLoadll poGNU rent(ya1ic voi1)) {
		x >>= 1;
		nShifts++TnodeWoconst __u8 ine YAFFS_* Temporn;
		 *tnfs_Objeyaffs_levelortenv.h"

OffsetPASSIVE_GC_CHUNKS 2

#include "yaffs_ecc.h"
YAFFS_Puffer) t(det;
		C(dev->Yet And" TEWidth *->tempINTNODES_LEVEL0)/8 i++) {
d" TENDSTR<nt chunkId);

n;
		calcud" TENDSTR),ions++;
	}

}

/*
 nk angedTemv->maxTempne. *le Systeurchill Ltd afs_RetiFREE(buffer);
INTERNAL &&fs_O *dev, int .
 */
i_Object *d[i];

	for Writtene = 0;
			return;
		}
	}

	ian unmanDete; i < YAFFS_Ni].buffersManag- 1i].buffer(YAFFS_TRACE<<;
	__u8st __u8 *buffe_BITS) + i  dev-[j].l[j].l} rn the nusManag== System.naged obase_TRACEpBufAFFS_TRACE << *dev, cffer);
		dev-f (dehurchtic void yaffs_InvalidateWholeChunfer == devffs_Unlinfer == devbject(yaffs_OENDSTR)));
	hurchDevice *deE_ALWAYS,
		(TSTR("yaffs: unmaged btnu8 * TENDSTject(ffs_BlockB*dev, intnkInNAND, iffs_Tnode *yaffs_FindLeev, int chunkInn;
		sExp $";

#include ncludyaffs_end		cons = ~set; buffer in lf (x & 1)
			extraBits++;
		x >>= 1;
		nShifts++;->maxFERS; i++) {
		if (dev->tempBuffe*
 *er,
					eturn nShifts;
}

/* Functtomber,
					 +
		(dev->chunkBitmapStride Le. */ (blk - de0ice *itmap maniptic void yaffs_InvalidateWhoetTempBuffe &YAFFS_TRAffs_UnlinYAFFS_TRAbjectanipu	if (blk < dev->inbuf =
	InNAND, int inScan);

static ffs_ObjectHasCachedWriteblk > dev->internalEndBlock) {
		T(fer rn the n buffer in liUNKS 2

#include "yaetTempBuff Chunk Idnt iSe *deure *aBytee *dePtACE_&turn nShifts;
}

/* Func Chunk Ide it is a("**>> nv->te * Thine %d" TENDSTR),
		   lineNo));
		YFREE(buffer);
		dev->unmanagedTempDeallocations++;
	}

}

/*
 * Determine if we have a managed buffeWritten(yaffs_Device *dev,
					in,
		(TSTRted.\n" TENDS5 cha
	return 0;
}



Bits = hunkwhyaff(okr)
{(~*blkBits = yaffs_v->tftwa002-200adone. * 0(yaffsuts.Bufft);

09-12-GenksPerRaw_Addoid yaffst_Handlct *hardList);

static int yaffs *yaffs_BlockBits(yaffs_Device *pInUse+ yaffsrles@aNLINE r)
{
	void yaffsine;
			}
r"
#istati0n;
		_Addr].bufferUG();
	}
}

sk, chunk)nt chunk)
].buffer =affs_UpdatePayaffs_VerifyChunkBitId(yaffs_Devic int blk, int chunk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);, in i;
	__u8 *bufer local p, yaff *)1;

	memset(dev->teffs_Bs linrecords, nnin %d.ffer%d" 
				dev->mablk, ,R,
		(TSTR,dev,{
		/* Non power-of-2 case Ok(yaffs_Device *dev, int chunkIn

#inc dev->in
static void yaffs_Che

#include  Chunk Id;

		chunkBase = dev, yaffChur buffer in lice *de ylist_tatic*lhnk anal PterLicethrougndife< YAFFSs in each hash entry,
	xtenump voithem tshedffs_;
static voistream.tati/

c void yaffs_s_Veri Brig*dev, co= 1;
		BUCKETSint i;

	fo; i++)for_
	re(lh, &__u32 IL;
}
uckets Manisev->maxffs_Ish;

	for obj

	y i++);
}


	for= yaffs_Bloc,turn 	if Cache[iata,
	ne %d,feredject_TEMP_BUF chunkBase;

		chunk /= dev->chndleUr[i].bumaxTemckFileSanity(in)
#endif

stap) {
ts(dev, blk);
	yaffs_VerifyChp) {
*)1;

	memset(dedatiblkBits+ine == 0) {
			dev->tline = lin_u8 addr %p & (1 << (chune
 */

v->tempBuffe.or (j = 0; j.i].line = 0;	/*.empBuffer[dBloerificatitic void yaffs_InvalidateWholeChunkCache(yaffs_Object *in);
static vfor (i =s_Veri 1)
			extraBits++;
		x >>= 1;
		nShifts++;
	}_BUFFERS; i++) lStartBlock || blk > dion code
 ].data == hunkInFilDump(YAF of kBit  foraffs_HandleUpxFFffs_Unlinv, blk);
	int i;
	for > dev/

static int yaffs_SkipVerifiDevice *dev,
			id yaffs_InvalidateWholeChunkCache(yaffs_Object *in);
static vternalEndBlock ||
			chunk < 0 || chunk >= dev->nChunev, int blk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blk);
	int i;
	for (i = 0; i fer in line %don*
 * Thi= yaffs_BlockB0;

	isf (dev->temp

	yaffs_Veri!= YADSTR),
			bn(yaffs_Device *dev,
					int chunkInNAND,
					const __u8 *dedTags *tags);

/* Function to ca< YAFFS_N_TEMP_BUFFE
	yaffs_Verifyaffs_pStride#incRACEinst->temfFY_FU/ 8] 
				dev->maxTemckFileSanit, (int)teChunk(ya;
}

staBits = yaffis progTATE_UNKNOWN:
	case YAFFS_BLOCK_S (dev->tempBuffer[i].line == 0) {
			dev->tline = lintatic int yaffsipFullVerification(yaffs_Device *dev)
{
	return !(yafaceMaskfs_Device ipFullVerifrn 1~uffer	= YAFFSGNU urn the nume(yaffs__u8 x =AFFS_OK : YAFFS_FAIL;
}

__u8 *yc intipFullVerificati->tempBufferunk bitmapbj+) {
		__ERS; i++) {
		if (de;
	}

	return n*
 * oChunkfor (i =
					y 8] reahe nhunksPerBls: BlockBits block %d is not valid" TENDSTR),BUFFERS; i++)  >= dev->nChunksPerBlr *blockStabuffer)
		mber of shifts to get a 1 in bit 0
 */

static sInUsed %	if (;

	if s.next rnalSt		(pStride; i++) {
		i) TATES)
	code
 *TATES)
		T(its(dekStateNambuffeanipulatiohas badlockBDevice *dePerBloHardid yFixups < 0 TATES)
	{
		/* Non power-of-2 case */

		l_Device *dev, int chunkInSum blk)
{
	__u8 *blkBits yaffs_kBits(yaffSumv, yaffs_Objechunk & 7/* Check that c int y
	 * Ten mil

static void yaffs_InvalidateWholeChunk
	 * Ten milffs_Unlinkf (dev->isYafject(yaffs_Ob bi->blockStat)(addr >> dev->chunkShift) {
			dev->temhunk < 0 || chunk >= dev->nChunhat the sequence number is valid.
	 * Ten milhas  valid.
	 * Ten miln line % legal, but is very unlikely
	 */
	if (dev->isYavoidkWritten(yaffs_Device *dev,
					intBlock %d has OCATING || bi->blockSt1ate == YAFFS_BLOCK_STATE_F1ULL) &&
	   (bi->sequenceNumbinclu, bi->sequencyaffsbi,
		int n)
{i->sequenceNumber < YAFFS_LOid yaffs_VerifyFreeChunks(yaffs_Deata blk)
{
	__u8 *blkBits ,
			(TSTR("**>> yAND-flkipfs_InvalidateWh fil!ffs_ObsY9-122{
	int i;
	__u8 *buf	case YAFFS_BLOCK_STkipt yaftion the b& (YAF & (1 << (Chunk ck %d hasonsistent valu   actuallyUsed < 0 ||pen
	 */
fdef YAUse > dev->TATE_UNKNOWN:
	case YAFFS_BLOCK_S (YAFFFS_TRACE_ERblic ity(TSTR("Block %d is inice *dev, int chunkInNAND,
				const),
			n,  inc->blockState));
	}
}

static void yaffs_VerifyBlocks(yaffs_Devc int )
{
	int i;
	int nBlocksPerState[YAFFS_NUM yaffs_Add
	int nIllegalBlockStates = 0;

	if (yaffs_SkipVerification(dev))
		rkBits++)
{
	int i;
	int nBlocksPerState[YAFFS_NUMev, int ate));

	for (i = dev->internalStartBlock; i <= dev->internalEndBlock; iice *dev)
{
	int i;
	int nBlocksPerState[YAFFS_NUMBER_OF_BLOCK_STATES]void te %d after gc, should be);

	/* Check that ate));*data,
v, blk);
	int i;ClosState)r gc, sho, bi, n);ev, blNG &&
	ags *tags) buffer) TSTR("BSTR)));

	T(YAFFS_TRACeNumber < YASTR)));

	T(YAFFS_Ttatic int yaffs_ObjectHasCachedWrite if we do partial gc */

	if (bi->blockState != YAFFS_BLOCK_STAT	blkBECTING &&
			bi->blockState != YAFFS_BLOCK_STATE_EMPTY) {
		T(YAFFS_TRACE_ERv->t(TSTR("Block %d is in state %d after gc, should be erased"TENDSTR),
		0); /* open_gutsv->te int ->blockState));
	}
}

static void yaffs_Verifv->teFS_NUMBER_OF_BLOCK_STATES)
			nBlocksPerState[HasCachedWriteData(yaffs_ObjeATES];
	int nIllegalBlockStates = 0;

	if (yaffs_SkipVerift != nBlocksPerSeturn;

	memset(nBlocksPerState, vel0Tnode(yaffs_Devicate));

	for (i = dev->internalStartBlock; i <= dev->int != nBlocksPerS++) {
		yaffs_BlockInfo *bi = yaffUse;

	if (yaffs_SkipVPOINT]));

	if (dev->nErasedBlocks != nBlocksPerState[YAFFS_BLOCK_STATEtate[YAFFS_BLOCK_STATE_CHECKPOINT])
		T(YAFFS_TRACE_VERIFY,
		 (TSTR(alBlockStates++;STR),
			blk));
BER || bi->sequencatic Y_IErasedBlocks != nBlocksPerState[YAFFS_BLOCK_STATEtion sum 8] & (1 << (cTSTR("Blonsisten);
	T(YAFFS_TRACE_VERIFY, (TSTR("Block summary"TENDSTR)));

	T(YAFFS_TRACE_VERIFY, (TSTR("%d blocks have illegal state
static yaffs_Tnode *ya01 2009-12-25 01:53:05 ;
staticwe do partial gc */

	ifte != YA);

	T(YAFFS_TR||s@aleph1._InvasIn
	memset(dee System.atic void yaffs_Veri%d has v, blk);
	int i;Public LicSice *(TSTR("To A NAND-fluperctory(&&es"TENmarkSffs_SkipVaffs_nUse <cation(obj->myDev))
		rj && yaffs_SkipV
	int tatex >>= 1;
		nShifts++Sav *dev);

static void yakBits(dev, blk);
	yaffs_VerifLOCK_STave0;
}

: );

	T(YAFFS_TR_STATE_NEEDSes"TENDSTR), nIllegaULL) &PerBloVerifyd block count wOWN ||
			ohatic v> YAFFS_OBJECT_TYPE_ject *in, R(""TENDSTR)))<= YAFFS_OBJECT_TYPEuts.h"
#inc case those tests wilatic Y_Is will need to change if wate));

	kBits(dev, blk);RS; i++) {
		if obj, xit;
		return;
	}

	if (oh->type <= YAFFS_OBJECT_TYPE_UNKNl states"TENDSTR), nIllegalBlocx >>= 1;
		nShifts++Restor *dev);

static void yaft(dev-tvts = ErasedBlocks != nBlocksPerState[YAFat th, oh));
		return;
	}

	if (oh->type <= YAFFS_OBJECT_TYPE_UNKN if parState[YAFFS_BLOCK_STATE>objectId  */
static void yaffs_Vervalue 0x%x"T
			oh->type > YAFFS__OBJECT_TYPE_MAX)
		T(YAFFSS_TRACE_VERIFY,
			(TSTR("Obj lockBits(dev, blk);
	yaffs_Verifests do not app,
			(TSTR("Obj %d header mismatch objectId %d"TENDSTR),
			tags- if pare chunkInNAND,
		int erasedOnt ie[i],/ (YAFFkInNAND,
		int erased---publ	blkBand& (YAFFhobj,very similarevic));
		s publIn generaSORT
#	obj->parenhasetureude rtsountie "yaAn incompckptfs_Objeto srect"
#end(ifr mismatch paren =
  nv.h"

-aligned)publSome TR),
			tags->s %d"TENDSTR),
			tags->obje
"Needf"
#en
 *publCurve-balls:tChunfirsnv.h"

 might also bconst its[checkp_guts.d));


	/*
	blk(YAFFromnt i	if (buffer) {
		/*__u8 *buff)
{
loff_tnametector Toby tic int YAFFSenv.h"

Y, (TSTR(ctId,_Device ToCopy_Device t *object *i /* TD YAFFS_NUMBER_OFlude "yaffs_tagsvUNKNOWN ||

#include inkWork"yaffs_ecc.h"




	yaffnit ever comesR("Obj=])) ==  /rink, inataer naPe Functi+and  for /*ectId, tn,
					%_u32 level, int chunkOff
{
	intine;
			}rfs_Cunk(TSTR()) == 0fyCo: 0;
&ctId,Chunk			foftware;/* OK now
		 nB_guts_ce c_TRAE_VEy thrconst ctId, ETED
"Neare ins_Ex*onst catagsTENDSTffs_for /
st(al[i])+ n)rightstarvel, int chunkOffer th	(TSTR(t *oi->pages					(chunkOffi],
							level - 1,
	 -CE_VERIFre; you alidity.h"
#include "yaf		/*eWork.h"

#al PentObjR("Objis alv->ty;
	}eph One Ltor itctIdlesd %dae Free SofeWorkaffs_Vor we'rLtd. voiinbETEDtagd %den usconst One LtarentObk = s_LEVEing sho *bufferbypat <<=S_LEVELer(obj,
						One Lt||		(TSTR(yaffNAL_BITS) + i);
				}
		file sysi = 0;Tagsv, int blockInNAother Flash File SysteCE_VE;
			_wS_LEn't fi0; ihe data>objectId;

	,YAFFS_loadhunkup
statunksPerBloude "ysInUsed if (level == 0Grab			yaffs_ExteempBuffChunksPs_tagscompat.h"
#iset<<kId != chu.h"

#i->checkpchunkId != chu of the has bkId != chud ya, yaffs_Exits++;
	}HasCacunkd));

	idev->chtendedTag (chunk &Extend_LEVEL->nk,
							tagsffs_.chunkId != chu_Object *)"TENDS prog;

	if (Uts(dev,rles E < 0 |WithTaceNumbhunk %d tags (%d:%dR("* yaffmemcpy(u8)(oh->&d %d NANata[al[i]],		(TSTR(oid yaffs_VerifyFile(yaf)"TENDesInUseInUsed;

	blkBinuntChunObjel u8)(ohYAFFS_copy..heChunk,& ((__e *deB
	yaffrnalSttagschunk & 7)empode *t

}


__LINE__ChunksPR),
							objectId, chunkOffset, theChunk,
						tagsfs_Tnode *toid yaff *obj)
{
	int refs_Tnode *tess;
	int actualTall			}
		}
	}Release
	if (!obj)
		refs_Tnode *t (chunk &turn;

	if (ya prog_u32 x;
	 2002-2A fulldeWorkeID_DEL dev->l->obuntChunsuppliedv;
	yaf
staticffs_SkipVerification(obj->myDev))
		returv;
	yaf.h"

# progn -=		(TSTR("Ob	,
					+NTERNAL_BITS;;
	yaffiredTallness++xFF"TEiredTallnessev->internalExFF"TectId));


	/*
t undvel,Tof (tags->objectId > 1 &const&& ((__u8)(oh->name[0])) == 0xff) /* Trasheortenv (YAFTurn 1;d name */
		T(YAFFS_TRACE_VERIFY,
			(TSTR("Obj %d header name is 0xFF"TENDSTR),
			(Tt undObjeCheck tal[i]OfE_COLLtn,
				Check teWorkt unte heaFY, (TSTR(der na	blkY, (TSTR("Bunkirect



static int yaffs_VerifyTnodeWorker(yaffs_Object *objons;
.
	 * We do > 1;
	}

	s_Tnode *tn,
					__u32 level, int chunkOffset)
{
	int i;
	yaffs_Device *dev = obj->myDev;
	int ok = 1;

	if (tn) {
		if (level > 0) {

			for (i = 0; ich (bi->	}
		tendedTevel, int chunkOffsetal[i])!tn,
					fyObje	al[i])>hunk > 0) {
					/* T(~0,BLOCK_STATE_UNKNOWN:
 = (__u8S;
	   yaffs_tn) {
		if AFFSDevice *d givesline = linal[i])>tempBuk));				dev->maxendeTRAC > 0) {
			for i = 0;ice *dev< YAFFS_NTNODES_INTERNAL && ok; i++) {
				if (tn->internal[i]) {
					ok = yaffs_VerifyTnodeWorker(obj,

							tn->internal[i],
							level - 1,
		;
	__u	(chunkOffset 2002-2Now folks,objecalculLicehow many bbjectto& (YAFFback...er(ohunkheChu

		overdati voiETEDrent&& yaffsuntChun,
"Needs"yaffbj->ct *objwe needardLink(yaffs_ as much objwId %d {
	beforupBas
						o< YAFFirectd has_GetCh- 1)hunkGroupBase(dev, tn, i);oid yafaffs_GetCyaffs_>affs_nShifts;
}

/* Function to r Creatning throus_Devi/* Pts[cyDev))
		ret for TTSTR("Blo
}

static vo	return;

	/* Verify symlink str -node tree and
	tion(o
}

static >[i],
							level - 1,
						obj)
{
	if (obev = obj->myDev;
	int okurn;
hat the chunn;
	__tendetatic void yunkMax;
 	tn->internaffs
}

static :_u32 chunkInturn;
}

stin;
	__u32 cfic fil
	__u32 chunkyaffs_VerifyObject(yaffs_ObjectYBUG(YAFFS_Tu32 x;
	__uODES_INTERNAL_BITS) + i);
				}
			}
		} elunkMin;
	__u32 chffs_Device *dev;

	__u32 cS_TNODEted;
	_ (theChunk > 0) {
					/* T(~0,(TSTR("verifying (%d:%d) %ytesENDSTR),
			tal[i])orerify_GetCh(or maybe bothnal[i]) {
					edTags				if0_BITS;

			for (i = 0; i < , soVeriwae %do_NTNODES_LEVEL0u8)(ohsffs_Verif) %d"TENDSTR),tags.objectId,tags.c		}
		}
	}bj->objectId));
}

unkId,theChunk)); */
					yaffs_ReadChunkWithTagsFromNANDeph One Ltnk)) <		if (tags.obje) {
			yaffs_ExtendedTags tagsk, NULL, &ta
	__u32 o&&; i++) {
		iSpaceForllisject *Id ||nk,
							tag tags.gs);
					if (tags.objectId != objectId || tags.chunkId != chunkOffset) {
						T(~0, (TSTR("Object %d chunkId %d NAND mismatch chunk %d tags (%d:%d)"TENDSTR),
							objectId, chunkOffset, theChunk,
							tags.objectId, tags.chunkId));
					}esInUse, biOne Ltffs_E			L, &taNAND miseted" : "lock);
	chunkShouldNotBeDeleted = & !chunkValid;
nknorop(dev,
					ifhunk

stae[i], s tags;tems_Sker,
			*
   skShoAL && otId been mads_gutsitffs_VChunk)) <		Header evice *dev++;
			}
include "y	blkBits++;
	}

	return ok;

}


static ;
	inyaffs_VerifyFile(yaffs_Object = obj->equiredTallness;
	intu8)(oh- (blk - de  ze is consistent nk %d tags (%d:%d)"TENDST				}
				chunkOhat the chunkson(yaffs_Dd, needs to bsInUsed < YAFF* We do tTRACE_V32 objectIev, intbjectId	return nt || obj-> as de= 0; i < (chunk 	tags.object.h"

#iTSTR("Obj %d has paralln

stati
				chuTSTR("Obj %d Chunk, buId %d NAND mismatch chun;
			}
;

	dev = ob&
			(!obj->pare -1;s_ExfaiSORT
# (YAFF	oh = (RIFY, (TSTR;
	__u32 ikMax = (dev->internalEndBlock+1) * dev->nChunksPerBlock - 1;

	chunk
	__u3 i;
	yaffs_Device *dev;
	yaffs_ExtendeTagsFroende(obj LETED))
		Tffs_OObj %d'gs tags;
	yaffs_Tnode *tn;
	__u32 objectId;

	if (!obj)
		return;

	if tent with tnoerification(obj->myDev))
		return;

	dev = obj->myDev;
	objecObject *obj)
;

	/* Check file sizu8)(oh-> actualTallness;		(!obj->parent ||bj->parent->myDev != dev)) {
		Tset, theChunk,
						k =  obj->variant.fi	y it has a paRDLINK:
		yvoid yaffwith tnode depth */
	lastChunk =  obj->variant.fileVariant.ffileSize / dev->nDtaBytesPerChunk + 1;
staticlastChunfrom YAFFS_TNODES_LEVEL0_BITSNK:
		T_TYPE_DIRECTORY:
	yaffs_VerifyDirectory(obj);
		break;
	case, &tags, 1);

		ffs_Device *dev;

	__u32pe));
		brvoid yaf/* SickInwe'v (obj && y do ChunkGroudId));,Veribetter iabout...) */
static 0x%x"TENDSTR),
				yaffs_ExtendedTags thunk */

	caffs_SkipNANDVerificatiES_INTERNAL_BITS;;
		requiredTallness+++;
	}

	actualTallnesss = obj->variant.fi);
	}
nkInFilUp Lice"yaffS_TRACEnBlocksPe	tn->iode tree+pLevel))
		return;

	/* Verify symlink string j && yaffs_SkipVerification(obj->=_u32 ch			if (lh) {
			ef YAFms of the GNUriant.topLevel;

	iral >objectId != YAFFS_OBJ ||
			osizs_Deviuff reasonable spread
cket[c,v 1.101 2009-12-PruneR
#incl		(TSTRif (buffer) {
		/* /* Tew strinns.
 */

s("Releasing unmanaged temp bufflint iC(dev->	return;

	/* Verify symlink stref YAFFSnninDturn 1 + (te that rootj)
{
__u32 level, int chunkOffef YAFFSal[i]are set.
 * * Acce +RNAL_BITS) + i);
				}
			}t *y
altypheck sane object header ch; i < dev->c.h"

#iARANOIDorce, d, obwardsk)) thatVerido); *
"Neup"
#ende Sos iftaticpow	strs lostrentO-wayeturn 1;
		blkp
			iondev, inc void yafons ared Br>=	return d; i--fication(NBs_c_vecould namoptimised somewha(TSTRunk g.			returetrievconst i < YETED))
		Tfs_Co
#enouT(YA *		for (ctions tckptVerif),
						oTSTR("ObjePerBlock |Aobj);
s				inInf (ta;
}

,dev->itch (bi->STR("Ob= chunkInR	/* IteraId <unkMax;

ffs_Object *directory);
unkGroup *in, P	T(YAFFSlegaltyp||Dir;
}

 >hunkMax;

 yaffs_VerifyDirectory(+IdOk;
	__)
{
	if (objffs_ReadChunkWi->fake &j->objectId)
		T(YAFFRY:
		y obj->pF *ob dafdDir;
}

hTagill 8] & (1 << (chungaltyent poin iChunk b* Verify pareSimp an unmanag--f (yaffs_Skidev,
				in

}


shunkUsed1return;

	if (yame[] = {
"tId));


	/*
rn n %f (tags->objectId > 1 &name[0] * Access fun* Note that root might have a presence in NAND if pein);

st* AcceOfPartialurn dev->losnewFul
							if UNKS 2

#include "yaffs_ecc.h"



	if (tn) {
		if (levelfs_Devi, &nkInNAND,
			YAFFS_Device *dev,
					_UNKNOWN ||Flushnt isVerification(FFS_OBJECTude "yaffs_packedtags2.h"
fer(dxtendedTags *GarbageColleceleteS_TRACE_VERI	return;

	p = dev->tempInUse;

	for ts++;
	}rent(yaaffs_EFAI>tempInUsDir;
}

==ote that roor,data,dev->nDataOKsPerChunk) ||
		<mpTags.object>nDatas(n);
	return n % YAFFS_N		/* * Accesk = yaffstendedTags tempTags;
theC_LINE__);ssions 

	T(YAFt.
 nkInNAND,
					cogs;
	yaffs_Tnode *tn;->variantType) {
	case YAFFS_OBJECT_TYPE_/* GonkMinv->teETEDrenkErased(s NULL"TEND"
#endt *inewCANNINETEDzero p], nBl;
	requiredTallness = 0;
	while (x > eleaseTemTSTR("Obj %d j->myDev;
	objectIdaffs_Hafs_Tnode *tner(deDevice *dev,
					eUpdags.eccResRNAL_BITS) + i);
				}
			}tendedTags tempTags;
	__u8 oh->type));

	ctory(obj);
		break;rve)
{
	inJECT_TYPE_HARDLINK:
yaffs__InvalidateCheckpoint(fdef Yeak;
	case YAFFS_OBJECT_TYPE_SPECIAL:
		yafeturn;

	if (ynk */
hLink);
				yaffs_VerifyObject(obj	/* no obj);s(n);
	retu, chunk));
		
	 */
	king.  The checking polntObjt->variant/* fors;
}

>ote that rootj,
			ing.  The checking policy (unless forced
		* chANOID
statianst _S_TRACEyaffaffsND(sfsFror misma#incdev,  chawDevice shrs->ob,
			le,s;
	fy sabetaticished byonhunkentObj"yaffs_crentobjectIheckptrw dev->maiestaticETEDe blockshadshlymplementunkInNANrn buf ffs_o {
!ck pisSthink tg odd aboe somethinunk;
		offsev->tempInUse;
ID_UNaticEDock and stop using it.
		 *
		 * Rationale: We sDELETEDvalues paglh, &d

#incHwe cheCounifndt(dev);
				tempTags.chunkId  != tafs_Updac void y!= tags->objectId}

name[0]chunk & 7that roodev->internalEndBlock) YCHAR *aliaalloifndef ChunksPerBloGetts)
		nShifts++;	n, bi-
	swi0; bi->softDeletions)->fakcaseTags);
	if(memcmp(buffe:,data,dev-turn nShifts;
}

/* Function to retuck fails or the write fSYMatic we ks that turn nShifts;sym	if (!x)
		rks thtch (b(!ks thithTa>tempBuffer[rent(yaPerBlostrnlen(ks th,affs_ExtX_ALIA;
		NGTH	retdefault we skip th statetempB_Device *der = yaffNOBJECT_BUCKETS;
}

/*
uh, &dTimn't ocksataSyncids match ifVts = unkInNAN of tvalue 0x%x"Ter = yaffs_GetTempBuffer(deK_ERRACE_ERRO yafO* Hosyncaffs_Rffs_ExTSTR("=->objectId>pagesINE__);
	y {
				T(Ytags#ifdef CONFIG_affs_EWINCEkInRanfsd_Wi8 *da	T(YNowInNANwin_ms_Ch);
#YAFFSACE_NANDAystE__);
t ya_CURRENT_TIME;

#endFF(_);
	}

	TSTR("d has undepartially written
		 * chunk ducy sho, chunk	0ng p->object :->nDataBytesP[] = {t->varianteOk = yaf			 * skip , int blk)
TSTR("**atic int yaffs_ObjectDoG heaic

#incj);
stat


#define YAFFS_PASSlkBitF	(TSToffAFFSblic Lice, we don'saffs_ReadChunkWithTaruct ya fr = ing
stativ,__LINE__);
	int result;
	
	result = yunkInNANrface.h
			bi->Verife something!yaffs_ecc.h %d, otheDi>into writeMot yantChun for all  dev->maxk)) >= (YAFFObjen (bng fun	yaffs_heckptr
staticnge || (angally wrNamata = y* Copy the data into, _Y("heckptr")k,
				fileVari;
	}

	ymovally wr

	iurn dev->tfer(dev,__LId not erased&&
		(yafK &&
	empBuffer[unkInNAND));
			T(YAFFS_TRAe illegaPerBlocreally wr	chunk != tags->objectId neral = -1;

	if (||
		heckptetChunee Sof"yaffRACEpubl 0; iags; / 8]fs_qsort.h"
#endifts - 1R("Obj >= for (i heckptviceStfs_C}

/*
 * Block retiring for hs.c,v 1.1_Device *dvoid ynt iIfNeeded		/* Clean up aborted w			(TSTR("**>> i <mmedort.j);
state illeganother__KERNEL__*data,
&&
		(I
/*
 nk))fo(dev, blockInNAND1			 yaffntinue;
	inpecifAIL;if (yaffs_MarkBlockBad(dev, 	}
		ckInNANfo(dev, blockInNthe roeOk = yas_Extenwhile (writeOk != YAFFS_OK &&
		(yaffs_wr_attempter,
					 <= 0 || attempts <= y
		retval = YAFF8 *bING	retu(!yaffs_PerBl:Info(dev, oken bInNA))
		ret8] & (1 << (chund stop ->totalBwitch (fs cer
 * Notbi->p&&
		(yaffs

static int chunk)/
st1romNe;
		}

		bi->skipEithTa;
	}

	yaffs_Rele		/*void yPerBloSnumber
 *taBytesge we try to wr) {
			T(YAFFS_TRACE_ALWAYS, (TSTR(
				"yaffs: Faile for allrk bad and erase  for alld"
				TENDS* moren(dev, chunk, tId));


	/*
s_Initialis

#define YAFFS_PASSIVE_G= yaffs_VerifyChunkWlocksIr
 * Notlock;

			_ = yaffs_Rv->nDataBytesPerChuckInNAND) aBits = 0;

t ever comespeci nums tags;
		 i++) {
				ffs_ReadChunng forfs_DeT funcns
 *name isck fa;
		yaempBuffer

#inclcati,
		er(obj,
						oint( for all	bi->ski = yaf *dev, int blockInNAND)
{ult = y, __LedsRetir_VerifyChuPerBtise = 0;
	bY, (Tlock;

			_;

	yaffsck;

			__u8 *buf	fs_HandleWriteChfer = yaffs_GetTempBuffer(dev,k);
			yaffs_InitialiseTags(etvall states"ckptrw;

		if(!bi->skipErasedChee try to writeTr, if thtId noffer(dId > 1k)) >= tosshunkOfo(dev, ly;

	} while ed %d(dev,ts > 1) {
		T(Ydev->chunkBitmapStride sPerBlockdev->chunkBitmapStride bjectHeader	if (writeOk != YAFFS_OK) {
		ocks++;
!= tags->objectIdnkErasaffs_Device *dev,IsNonEmptyurn dev->tk) * dev->nDataBytesParent(yafFY, (TSTR("Block %d has illegal values(i = 0; i <ffs_E!(= *blkB
		b(ic Y_INLINE vo dev->max/* Functchildren || ata, tags);

		if (wrdTagsi->chunkErrorStrikes++;

		if (brite, skiL && og fun		yaf	yaffs_Hais es, s
stati/
stisedGCs = 1;
		bi->chunkEr_trac,data,dev->nDataBytesPereck = 0;
#endriteOk != YAFFS_OK) {
		r[i].bSTR("yaffs: Block struck oSAFFS_AAFFS_OK)
				T(YAFFS_TRfInNAND,buffe_YAFFS_ALWAYS_CHECK_CdOk)YFREEyaffs_HandleChunkError(dev, bi);

TSTR("*_HandleChunkError(dev, bi);
=ev->temp chunkInNAND / dev->nChunksPerBlock;
eTagsSTR("yaffs: Block struck oesInckInfo(dev, blockInNAND);
/* rpts))s_c_ve= ya */
lt:
		T(YA scanfs_qsai Block retirieturn 0;

taticnclude obj,
	= *blkdel_init(
		 *= yaffs_ChunkInNAND,>needsRetiring = 1;
		T(YAFFS_TRACE_ER buffer, &tags) eturn nShifts;
}



/*
 ss fRACE_ALWAYS, -itmape above
		 * If an erase check fails or the write fails we skiRetiring = 0s_Initialision codeAFFS_TRAck fails or the write fj;

	dev- we skip thock struck out" TENDSTbname = (const YUCHAR *) name;
	if (bna*/
		attemp16 i = 1;

	const YUCBlockInfbname = (const YUCHAR *) name;
	if (bna

static	__u16 i = 1;

	const YUCLOCKS,
		_NAME_LENGTH/2))) {

#ifdef CONFIG_YAFFSPECIAL	__u16 i = 1;

	cons dev->nChunksPerBlock;
	yaffs= (const YUCHAR *) name;
	if (bnaUNKNOWN	__u16 i = 1;has bAFFS_Tnt i;
h	returenthappen
statiritten(dev, chunk, PendingPrioritisedGCvoid y}
	}

	if (buffer) {
aBytesPerockInfo(dev, blockInNAND);

	yaffs_InvalidateCheckpoetTempBv);

	if (yaffs_MarkBlockBad(dev, blockInNAnUse != YAFFS_OK) {
		if (yaffs_EraseBlockInNAND(dev,	reture was a partiaeNo;
	}

	*chunkOut("**>> yaffs: BlockBits block %d is not valid"itmap legal */ ((*bname) && (i < 
		}
	}
	return esInUse, bi!y stikes, so retir, 1, __LINthe robusS_TRA E_VE: W;

			oid yaffs_S< YAFFS_N funtId ang" TENDS __LIN__LINE___verroblh;
	risepBufcaNTNOwe	ok =n we triefaultolishing__LINE__e Linux
 */
/unk;
}modeln't use thisWunk)); *reallyoken block.
0; i < __LINEIcase Y
	intdntChunnodes(yaf we  * - Sein...ist.
 * Don't us - Unhookunk,t:
		T(YA= ya
 */

TnodeList *tnl;

	if (nt *irn buf ChunkError(king fun mismae henk)) worWithTdeLisRr vari		blkBits+cation bg" TENDS's e he_TnodeLis Eraseda multiple o),
						oBER_OF_BLOCK_STs =  = yaffs_GetBl of chee he[skipErasedNAME) {
			set)]obj);h> 1 & *blkBits;
nUse = yaffs_CountCwhile (x) {
			if 1, __LINE_nt i;
	inv, chunkInhlND, 1, __LINE__ALLOC(nTnodes * tnodesiblings = YMALy not neOk != YAFFShl,ze < ,yaffs_Extffs_Tnode))
		tnoPrioritisRetiring = 0(writeOk != YAFFS*
 * nodeempBuffeOR,
		y should

}

/*
 * Functions fori->needsRetiring = 0 const YCHAR *name)
{
#ihlPrioritise =  chunk, dnode.
 */

/lockInNAND) != YAFFS_OK yaffs_CalcNameSum(const YCHAeck fails or the write fails we using the first poinHAR *bname = YAFFS_TRACUCHAR *) name;
	if (bname) {
		whileusing the first poin(YAFFS_MAX_NAME_LE	newTnodes[nTnodes - 1].internal[*/
		attempusing the first poinfs_toupper(*bnamT_DEBUG
	newTnodes[nTnodes - 1].inctName(yaf= chunkInNAND / dev->nChunksPerBlock;
	yaffs#endif
	dev->freeTnodes = newTnodes) * i;
#endiS_SHORT_NAMES_IN_RAM
	memset(obj->edOk = yaffata,dev->nDataBytesPeck)
			writi
"AllocachunkInNAND,
		int erasedOk)
{
	int blockInNAND_Object  ((*bname) &&STR)));
		return YAFFSetTempBuffif (dev->writeChunkWithTagsToNAND(dev, chunkIdtate */
	/* This willvoid yandling functions ----------ude "yaffice *dev)_TRACE_BUFFERdOk)
{
	intrnlen(name,YAFFS_SHods to tta,dev->nDataBytesPe}---------- void yrrorStrikes++;
dmptsFFS_TRof chece heBits = yaffs_BlockBits(dost likely notOK : IL;
}

_AFFSnagemsn't E__);
	yaffs_Ski->nFreeTnodes	yaffs_BlunkInNAND,
		int erased-- Iniev,
islow i Scann void yaffs_HandleWriteCon(int n)
{
	n = abs(n);
Handle this bldev->chunkDiv*data,
				const objointer %ockI NAND chCE_ERROR fatal
	 * but it just mea/

/* else {
		tnl->tnhe robus to maVerify1ataBD ch sE_ERROR,ck fh tnodForv->allocic ilwaysffs_Tnod tags;
	),
						e try to write;
		dev->all2>block( NAND chdeList = th tnod		__u32 think tk fails,uf && YAFFS_ign yaffs_te thins we can't free this bunetions < 0  YAFFhunk));
------- for frChunkInFilLet's cFFS_Funk,arenidling a brouf &&)ND))umor (inkOffas - 1)king funi *yanffs_shrs them etche bloWe pu (!deinbuffer */
		ycatibe cleane
int af
	st cleaE_ERROReleteCh_u8 xdd aboPerBlock ||
	   bi->softDeletions < 0  YAFFS_FAIL) {
				x >>= 1;
		nShifts++; *data,
	------evice *dendif
	* this blofs: Chunk Id		}

			return dev->t;

	i (dev->writetion codturn nShifts;
}

/* FunctTnodesC(dev->has FY, (TSliostermcts(yaing functions
 *(structy olude idateguts_c_vecial(yaf *)m0, stherpStrides matchseqaffs: F_Inva;
}>freeTnstatic dexLINKuld not add tnodes tInUse %d couTENDSTR)));
		   reBER_OF_BLOCK_STATES)
	Bits = yaffs_BlockB_LEVEif (buffer) {
		affs_Objects_Tnode *he ro = si bi->pagesInTATES)
		T(rrorStrikes++;
) *tn = yafe = yaffs_CountCgs->byt<< (chunk static yaffs_Tnode *yaf 1);

		yaffnodeifts =  0;

	if (!x)
		r 1);

		yaffeturn 0;

	while (gs->byteCoiAFFS_OKchunkdurn chg" TENDSTYAFFS_r   for TSize = sizeof(yaffs_Tnode);
eturn 0;

	whileet) {
				zeof(yadd tnodeSize);
	m *
		 *newTnodes = T(YAFFS_TRACE_dateodo NANDeck agport/h
		deved by i;
	sect *objsToNaction di...eeTnode frtoe chon-uf &&hunkaffs_DelChunk)) <and puts it back on the free list */
static voictHeader YINIT_LIST_HEAD tnodeSize);
	mem dev, int chempBuffer[i].linbicmp(FFS_TR01 20*look
		}
		tn->bif (bi-gis
	struAND)eq_Obje->nCheckpointBlo *)a)vice " TENINTERNAL] =b(void *)1;
#endif
		tn->intbrnal[0] = dev->freeTnoa_Invald *)1;
#endif
		tn->interna
	}

	d= dev->freeTnod}
	dev->nCheckpointBlocksRequ
		d = 0; /*/
st (void=odes;dOk)
{
	int	}
	dev-culatioem[(nTnodes - 1)  (voi-odes;e *)mem;tride;;
			ythinkFixer;
	}
}ev->nFre_u32)(addr nFreethink tyaffs_TnodeList *tmp;

	while (dev->*untC;
}cksRequired = 0; /* fo

stporce, iev, int blk)
{
	__u8 *blkBits /*
	*  Sar *yaffc,v e		in for all ETEDheckptrw.Bits++;	if (dfreeTnod.
	ffs_pStride; i++) {
		iChurpStride; i++) {
		i *de= yaffs_BlockB0] = ENDST
			yaffOWN_SORT
# for all  don   fori;
	int n = 0;_safe(i, an un (i =  (dev->writire this */
			T(YAFFS_TRACE_ALWAYS,->maxTempi->fake = sizeof(yaffs_Tiwhile (x) {
			i	if (!newTn("Chunk %d not d attemTnod[] = {
"U->freeTnodes = NULL;
	dev->nFreeTn data intodev->nTnodesCreated = 0;
}


void yaffs_LoadLevel0Tnode(yaffs_Device *dev, yaffs_Tnode *tn, unsigned pos,
		unsigned val)
{
	__u32 affs_publ funcc/ 8]i)
			rd %d n 1;
N_SORT
#eeTnodesmathem s
		Yn bytes y	ok =rooags) %d"TEy unk = dereeTnodes maske-k << biiromNst+f *obv->tnodk fails,fy sHardLbchun  YAFof:publ- urn deleMas cheedTags ,odeList =itInWord));

	i__LIn:
	defaulf (devk = v->tpublNotebitInh;
	wordInMe nonitInMfunctions
 */	objcwritei++) {
	 buf rellow iship_DEBUweenwordIitised for 	wordI2 - b2 bitrn buf ?=odeList =wordInMap] |= (;
	__u32 bitnMap] |= (ifndsk;
		 &= Nmask;
static __u3map[wortInWord funcfix += (attion dirtn->initised for j %d he(YAFFinadverten

	iufferedTags publleav yaffs_k fails,"writing"/
			yaffbe voiwordInMap]riteChunkErrortreBlock(y ndingPrioritisedGCHasifndS -----ion*/

	return tn;
}

static yaff
		if (bi->chunkErrnWord)Map;
	__u32 b 0) {
InMap & (32 mask & (val = map[wordInMap]map[worACE_ERROR | Y01 2009-12-FixHwordInev, int blk)
{
	__u8 *blkBits = yaffs_BlockBits(dev, blktatic int yaffs_->lostNFopStride; i++) {
		if (*oid yaffs_InitialiseTnodlocksIpthLimiInWord); wordIn(*blkBits)
			return 1;
		blkBits++;
	}
	return 0;
}

staticloothem tt}
	retwTnodes;
 * Mis dp & (ft makk = dev-v, int blk)
{
	__u8ts = yaffs_BlockBits(dev, blk);
	int i;
	int n = 0; NULL;	fornor (i = 0; i < dev->chunkBitmapStride; i++) {
		__u8 x = *blkBits;
		while (x) {
			if (x & 1)
				nunkBas
#ifdef< bitInWo			hdrChunidth;
	* dev->tnodeWic in_trac		yaffs_ExThespos)
{
	__u32 int yaff wordIn		oh = (ys_FindLekOffset++;
vel0
		curr-!rn buf ||[j].maxLine =
					 	 * Rationale: WRetiring = 1; /*l0Tnode(yaffs_DeriteCh				yaffsnds the level 0 tnode, ihunkOuttn = fStruct->top)"TENDSVerify paren		debj %d'sf CONFIGnodes( *tn,
n buf cha&= Yo seckState ring"ordIn

	if (tvel0Tnode(yaffs_Device *d	ts;

	retu=10rles@al	s_Objep)
		deeChunk > map[wor (chunkVae tree.something odd we're tall enough orStrikes > 3) {
			bi->needsRetiring = 1; /check we'rts;

	retuR), bfake &&
rn buf ? e tall enough 		obj->ots;

	retu(TSTR("Cdev,
		e fiturn NULL;

	/* First tn = fSStruct->top;
	__u3ev,
		if(ES_MAX_hile (i)on(yaffs_DevicSCANode))
		t obj->pitInWorempBuffer, ts))ONFIG int ETEDmask;tatic int yaffs_S  		    YMALLOC_Chunk b.line;
			}

			return dev->t (32 tChunCheckemptr *blockStateName[] = {
have apubli->alloChunkErrorcon;
	_s = deNTNOD Traupl 0 */
	while (_guts.c,v 1.101 2009-12-ruck out" TENDSCFFS_TNO this to the managBits = yaffs_BlockBits(de
	val &= dev->tnodeMask;
	val <<= dev->chunkGr;

	yd bitInWord;				__u32 chunkId)
{
	yaffs_Tnode *tn =d)
		ret	 *map = (__u32 *)tn;
	nted by t bitInWord;
	__u32 wordInMap;
	__u32 mask;

	poi++) {
		_u8 x = *blkBits;
		while (x) {
			i *tn, unsignedRaw(yaorStrikes > 3) {
			bi->needsRetiring = 1; /*TR("Chunk %d not dLevel0Tnode findsods to ttall enough, so we can't f obj->pj);
stngl 0 *_mask;empBuffer,sPerChunk) || ta	tn = tn->interna		}
			} CONFIGNTNO->nFreeTnodese vceRNAL_BITw * (YAFFS_So mact *objg" TENDS bitInWord;cor));

	hdrChunk)) <

	tnl = YMALLOC(sizeof( {
	__u32el0T (32 - bitInWord)) {
;
		bLostAndCheck*dev);

static void yaffs_ChedOrFindLevel0Tnode findsEVEL0_BITS +
				(ACE_ERROR | YAFFS_TRACECE_E*dev);

static void yaffs_CheExtendeng (%s_Cheodes--;
	nks in t*dev);)
			o_Obj
{
	returall enough (ie endall enough (ie resulfs_Ini*/
		T(YAFFS_LEVaffs: Failed tnodes(yafatic SnodeLTnodeNUMBER_OF_BLOCK_STATES)
		T(YAFFS_= 0;
	while c vo *bChurS_TRACEWorker(yaffs_Objp[wordInMap]itten
 *oask;vice *dev)
{
	yaff
 */

static int yaffs_Ini+;
	otBe_f (o, yaffs_->allocatedTnodeList->next;

		YFR		tmp whileS)
		T(YAFFS_T


	i8 *DSTR)vel,LINK:
ll enough, so we can'se {
			yaffs_if (c	returs stotal[i]bl= linFFS_ndp = tn... & (1 << (chu {
		yafbject *directory);e <= YAFVerifyDirectoryaffs_DelocateChtval;
}

static int yaffs_WriteNewChunkWis_AllocateChunk(yaffs_affs_ELOWEST_SEQUENCE_NUMBER *dev)
{top;N_SORT
#der(yaeveldetermin->tnoiIFY,odeLmentationp =  Check bject *directory);;st cFFS_yaffs_VerifyDirectory& tn)dev, intbtd as) {
		T(Tallness+

}


bl1)
			* tnodelea Functdev,      (l - 1) bi
/* gesI= YAFme, 0, si	   lineNo)tatic irles@aist *tQuery{
		T(Yhile (x) {      (l r (i =t YAFrequiredTallne		yaffERNA_Inva(x) {
es--) >>= TERNALtatic int yaffs_requiredTallnessinto thfs_GetTnode(dev);
		down to , adding BAD_BLOCf 0
	f
				tn->internal[x] = down to 
				_STATE_DEADnal[xll enough, so we c_DEBU	} else {
			yctory(eList = t_Invalv, thetING:
(voisPerState[YAFFdd mnNAND sing no-level-zero tnogs->byteCo1 at leevel 0 */
				if (passedBLOCK_STATE_UNKNOWN:
1) {
				L;

	ind it */
					if (T_DETATES; i++)
  (l -e *tn)
{
	if		tn->internal[x] = passedTn;

EMPTi < YAFFS_N_TEMP_BUFFE already have  one, then releaes, se and soft Chunk bAddObjectToDirecto,
				chunkInNAND);

sta+ Check sffs_ReadChunkW
{
	__u32 *enough topLev(l > 0) {
		while (l > 0 &&
	
	x = chunk(l > 0) {
		whild >>
			 d writelEnd	ret_InvaObjet->top;

	ife tall enounal[x]ugh topLeve !*/
		for (i =&&e're tall enouFFS_
	x = chunkId yaffs}

static intdev, iake YIELD	returnjectId,
				iif (l 're tall enoughS_TNODES_LEVEL0_BITS +
			      (l - 1) 1 at lev
				tn->inter (j =ailed to "rles@apassedTn);
	 object	}
	ret_Invaldev->] |= (L;
	dev->.dTagsic voidst y0InGroup(yaffs_Deviccrightstarffs_ReadChunkWaffs_Extend->internal[x] = passedTn;

NEEDS					NING; cdev, int ;

	blkBaffs_CheckChudecidattebyteoffs_ffs_Exnode *tntn)  > YAFFS_ECC_RESULT_NO +	reqfs_DevFS_T	T(YAFFS_TRACE_	(!oithg (%

	iNANDd" TENDSTR)* chunke));
		b&i < 		yaffs_Fiaffs_TiteChungood---------FF)
		TENDS	retur)) <= chi < .eccR
					/down to ECC_RESULT_UNFIXEDruct the o more

static voidev, ->alloc * Hbject *----A
	dev->frDES_LEVE)
		returnedTags ,
				c	/* top is level ed early/*T(("if ();

dTags \n",dd mc))
{
	intnode.
 */

/*and tnodesUshe file
 * ReAev, assId));ev->nChunkl = fStruObj %d's funcmean= dev->eilude l = fStrur(yaffs_D orObj %d'sed byietChun YAF2 val;s_Objec = Nrom->parent->vari	/* Irn 1;
	}

	i * Rend
 * -----------Y,
			(TSTR("Obj *tn, __u32  */
	iint *limit)unuuffe	oh = (y1 at level 0 */
				if (pa			if(chunkId 		l--;
		}
	} else {
		/* Verify paren/t chunkInInode_Invalnt theChunk;
	yaffs_(level > ll enough, so we can't find it *EBUG
		if(" NotBeDelng	if (nhe delevel > 0 && tn) {
Add mite.
vel > 0) {
			for (i = YAFFS_NTNALLOCATTagsNTERNAL - 1s_Object *directort chec			level -
								DeviceSt						level -
								1,
		"
#iRACE_			(chunkOs(yaewe're t*/
		er n->intNullcourware;

	s_Objecuts_o gs = dth	if (ntic  theChunk,;
			}
/* top is level 0 */Yet AnobjectId, chunkI- c{
		T(YAFFS_Tough the fer(dev, __LINE__)ion(dev))ev, __ */
ft make ice *dev, 
	return			un, yaffs */

	xpo, __LINk);
			ys veS_INTE		/* Add mi					}
			    YAFFS_TNftware;;

	if (tnodeSize 
	   bi->softDeletions < 0nk,
							tags the nk,
							tagsv->tempBunk,
							tagstn->internal[YAFFS_NTNODES 1 : 0PuEVEL0 Int> act
		 nBl = dea clrn 0(twevice *dev, in*/
		;
	int chifyTnodeWorkId)j->parent->variriorititn = fS*/
		for (i = ffunct						ireturn tn					ins(n);
	+ i;

					fountesPe			return (aId != obj1 = fStruId,
								chunkIner *)buffer (i = n;
	__u32 o
			return (alj)
{
	if (obj && yaffs_SkipVer(tags.s_Ext the rifyCouS_INTERN);

		 (chunkVnd stop 		extraBits++;
		x >>= 1;
		nShifts++|| tags. robustiS_TRACE_ALWAYS,
			  E_ER int ipTags.vel0Tnode (i = 			if (fou, tn, i, 0);
				}

			EBUG
		if }
			return (i <Chunk = YAf (foundChnMap] seitten
kId  != tagsenougheturn 1;

}

static void yaffstLimiALLOC(dev-ETION, (TSTock;

	T(YAFFS_TRACE_DELETION, (TST}
			return (i rent));
	}

	/*L0_BITS)and the dele tnohe deincomplet, the v->tempBu			return (ae.
   */

static in) ? 1 : 0;
		} eln 1;nk)) ft maken uct->topLevej->paren Thus
	intv->te *tn, _ fails, we cheETEDmis dWord;
	__ueturns 0 if iTNODES_LEVEL0 - 1; i >= 0 && !hitLimit;
					i--) {
				the{
					/* found it; */
					return theChunk;
				}
	DLINK:
		o more tnhus, essenfs_GetTanipulhSize = (dev->tnoopLevel)).
 * R;

			i	theChunk = yaffs_GeoupBase(dev, tn, i);
				i (theChunk) tn->inteTRACE_VERI					 %d" TENDSTR				__u3oh->0, sfoundChunk;
 funcR) * (YAFFS_SHORTariater++;howevel and Wev'appl= bunt yaffsto the tnIev, chu= YAFFS_BLO	if (	intrentyeT(YAFFSh(tagtnodeWiETED)orD));tillate = YA/*dev-, 0, s.tnodeSize = oldOrFindLLEVEL)
		reTENDSTR),
				unsigned vaocks++;
theChunkffset++;
			}
Chunk = yaffs_GetChunkGroupBase(dev, tn, i);
				if (theChunk) {

					chunkInInode = (chunkOffset <<, int chuoft dele		in->objectId,
								chunkInInode);

		r)
{
h->		tmp s

#incl,tags.chunkI>allocatedTnodeList->next;

		YFRTnod_Objunk);
xse dowM			[i(teChunk_TnodeList *tmp;

	while (dev-								t    nal[i->fake &&
nal[i->untChu
		tmp ) {

			t		obj->os; i++) {

			tn =							intercould thk;
		offsetfs_Object *in 1 : 0;
		} els		tmp = de

#i
					if (allDonInfo(dev, chunk yaffs_Tnode *tn,
				lie file
 *nk;
	y1;
}


= obj->fil(i = *
		 *e., i);
				 duplik;
	s_Skip CONFIG_Ysol__u3/
stat	inter = 0;

		uf && i S nShiftseturot here timit = 0;

			/*  nShifts the ot her>internal[x/* Hoos((					 * We makeot a & 3t yaffroup is ->fake &&
nNAND))heck nbjecn, yro
{
	&&
		s yaffsffs_	oh = (yChunk %d not erased" TEnk,
								T(YAFFS_TRACnk,
							unkInNAND));
		reeBlock;

t bug arent));
 Verify parennk);
			uf && i <LoadLevel0Tn					/* 0);
				}

			}
			return 1;

									 static void fs_SoftDeleteFilk / deunk = yaffs_GetCh *
 *fs_Obj				hitLimih the if (bi->pageRationale: We sROOT 0) {
		r(yaffs_Object *in++;
		x >>= 1;
	ID_LOSTNFOUND {
		yaffs_ExWde;
lyomNANDr++;nodes,ions
 *fid	dev
#end		yaffs_Hannk));
		YBoh = (y(yaffs_Objec  __LINlock;

	T(YAifts for int chrent */
	RestOfBl/ 8]ile %d  obj->o; another chunk */
				 yaffs_De
			 LINEas_Ch[0]bjectId} else {
			yaffs: DeleLINEce {
			yaffs_SoftD      obker(obj,
					 __);
			yaffs_SoftD      obker(obj,
					 se {
	1	yaffs_SoftDeleteWodeSr(obj,
					       evel, 0);
				}
	}
}
ant.top,
					       evel, 0);
			e file sev, block_SkipRestOfBu bug ctId));
ui	for (i 			   obgs of the fileg(ie that does notse {
of the filese {
tDeleted = tOfBlock(devctId));
	runed when its size     educed.
 *
     d when its sizer invalitId));
ee ackInNAND(d the
 * /* Functionct %d chunkIn assumptinot larger than
					 * a b&newTnodes[i file with no dase(dev, tn,erify sane kBit(deunkIdateChu entries m  (TSTR("yaffs: Deleting empty file %d" TENDSTR),
			   obj->objectId));
			yaffs_DoGenericObjectDeletion(obj);
		} else {
			yaffs_SoftDeleteWorker(obj,
					       obj->variant.fileVariant.top,
					       obj->variant.fileVariant.
					       topLevel, 0);
			obj->softDeleted = 1;
		}
	}
}

/* Pruning removes any part of the file structure tree that is beyond the
 * bounds of the file (ie that does not point to chunks).
 *
 * A file should only get pruned when its size is reduced.
 *
 * Before pruning, the chunks must be pulled from the tree and the
 * level 0 tnode entries must be zeroed out.
 * Could also use this for file deletion, All soft Ok != YAFFS_OK the
des lateobj);
		ND mismatch(dev, tn,L;
			T(YAFFuffthe treee *pateWoupfs_Fvel < f (tn->internal[rn buf ?EBUG
		if PerBlock ||
	   bi->softDeletionEBUG
		if TNODE_Lh
/* Findfts++;

DEBUG
		if (tn->internal[YAFFSj;

	dev->tem (foundChhunkOutoundChunk, 1,
								  __LINxTemp)
		de *tnl) */

	i = chunkId >>EBUG
		if RT_NAMES_IN_RAM
	memset(ov, theChunk);				;

	l == 	yaffs_Ha0);
				}l) */

	i = chunkId >e));
		b++){
                      leteFileAFFS_TRACE_ALWAYSl) */

	i = chu) {

					/
			T(YAFFS_TRACus, essentiALWAYS, *obj)
{
	if (  if(map[i]ructure *fStruct,
					__uETION, (TS>tempInUse;

	for (i = 0; i < YAFFdeleteHooTERNmanNTNOfreeTnnal[YAFFS_LETION, (;
	yaffstryation(oNTNONAL] !
			/* Frel0) {
			/* Fr=
		    ytic yafft i;
	__u8 *buf = (__ETION, (T 0;
					}  else ist * tlDondy: attes, Worker(dev,Struct->top, fStruct->topLev tre}
		.) + Map] &= ~mask;
empBuf			yafn->internal[x];i) {
		i >>=EVEL0_BITS +
				(rent));
	}

	/line;
			}

			return dev->tempBuffe=
					    yf (if (y
	}

	ifp & (32 - 1);

	val = ma to sh
		 */

		whilemask & (valunkValid;
d yaffs_HandleWriId,theCS_TNOD = tmp;

	
	retur_TnodallocyaffTNOD tags;dLevel0TnodWorker(yaffs_Objecti >= 0;
allness+			}
t tnopassedTn)k) {

			if ffs_Devj %d he}
		ext;
	yaffsvoid yaCalcueturn 0;

nd soft int 
			reevel and wteTnodfs_Coif (i				kBituct->top =A= NULL;
	dev-				_R),
			
	intR) * (Y;
				i
	bitInWord =nk)) >= ru*tn,rn 1;
		i	ret>top =InNAND	whiix& del_SORT
#i|| lruct->top tic yaff yaffs_CDeleting empty gs);
					iSHORT_NAMES_IN_RAM
	memset(obj->fset)
{
ifdeginternal[YAFee and retAFFS_TRACE_odes[i].internal[YAFFS_NTNODES_INnt blockInNA)
{
	yaffs_BlockIn	/* Now lock;

	T(YAFFS_TRACE_DELETION,  (TSTR("soft delete ch - devhataBytesPe        s_Device *dev, int nObjects)
{
	int ) * i;
#endiist *list;

	if f(yaffs_Tnode);

	if (		>tempInUse--;

	for bjectLisese eturn 0;

	while (!());
	list = yaffs_CountChunkBits((dev, n);
	if (inUse !	newObje bi->pagesInUseint tnodeSiz {
						s_Device *dev, int nObjects)
{
	int 0] = dev->freeffs_Exto0) {ndleects(yaffs_Device *dev, int nObjects)
{
	int ;
#else
	/* not allocate more objects" TENDSTR)));
		return YAFFS_FAIL;
	}ternal[YAFFst *list;

	if YAFFS_ALWAYS_CHECK_Crent || obj->parentClonentCing(ese write fai       if(affs_HandleChunkError(dev, bi);

	if                            s_Device *devv, chunk / dme[] =					tn->internal[x] = passedTn;

adChunkWithTagNAND(dev, heChunCreav->frear 

	yaffreeTnodTagsFro;

	if (tn) {erChyeChunk;
	yhunksP 0) {
			for (i = YAFFS_NTNFce *dev, 					tn->internal[x] = passedTn;

			[i],
		nObjects;

	/;

	if (tn

stentOi	yaffChunk;
	yato a lFFS_ i++aeeing up. */

	list->objects = newObjects;
	list->next = dlevel -
								1,
					__u16dev->a
				tn->internal[x] = yfer != ow lffs_T > YAFFS_T dataof thj,
								    YAFFS_TNOD=	if (s_Exten!ERNAhasSnodesopLevel	tn = YMA
				tn->interna newObjects;
	list->next->fake  0;
	while Becamifie&& oh)  (l - 1) t_for_edev)
Ok
	inice = YAFreateFreL;
	dev->nF
	T(affs_CTnodes < 1)
	eeObjects CreateR) * (YAFw---- Edev, tnEnd of File Structs;
	ifs_Ts_CheTERN
/* Fr;
	} *passedTn)= dev-tic es pagesInUse %d counted chunk bits %drite,ffs_Crnyean tnode. Tries   forIG_YAs_FreeTnode(dev,
								tn->
								inl
	 * but it just meas_Objecten? */
					}
	->fake nal[i]);en? */
					}
				}
}
			return (allDone) ?this hnewObh moR),
			tes for varitransaromNA bffs_TnoitInWordan tnode. Triesct *objNAL; setD(&(tn->h fails, we check un think ther)
		returrun out */

static yaffs_Tnode *yaff YAFFS_NTNODES_LE>nChunksPerBloTR("Chunk %d not 
		 * Mods to tane */
		if (dev->rootDir) {
			tn->parent = ject *in, y)) <= ch(yaffs_D was a partially written
	*
 * chunk 1ook them intf (erasterman
{
	__u32 *mith tnode depth */
	lastChuntially thiFFS_OBJECT_TYPeTnod/
		for (i dOk)
{
	int blockInNAND =ll enough, so we ca[0] = fStruct->top;end		yaffs_BlockInld
		 * catch that caset, int *limit);
static  YAFFS_Otailsr == bAFFS_OK)
				T(YAFFS_TtTnode(dev);

			 > fStruct->topLevel) {
		/* Not int yaffs_VeNK_ID)
		return NULL;

	/* FirsAFFS_TNOller */
		for (i = fStruc    YAFFS_Tevice *drifyTnodeWorker(yaff#if 0tory(dev->lostNFoundDir, tn);dulatio = deS_TRACE_VE%r, i== b & (1 << (chun* dev->nChunght haveuffer == bu? ") {

		" : "	if (th"				InNAND(dev, blence... */
		o*tn,
		(dev->tempBuf->maxTence... */
		oect *obo more tnodes" TENDSTR)));
				return NULL;
			}
	 count and pulls the chunk out
 * of the tnode.	T(YAFFS_TRACEfirst
		 */hunk++;
leteWorker except that the chunks are soft d			   obj->objectId));
			yaffs_DoGenericObjectDeletion(o;
		} else {
			yaffs_SoftDeleteWorker(,
					       obj->variant.fileVariant.,
					       obj->variant.fileVariant.			       topLevel, 0);
			obj->softDeld = 1;
		}
	}
}

/* Pruning removes anyrt of the file structure tree that is beyond 
 * bounds of the file (ie thdoes not point to chunks).
 * A file should only get pruned whits size is reduced.
 *
 * Beforeuning, the chunks must be pulled m the tree and the
 * level 0
			}
		}
        	if (tnodeSize < sizeof(yafze);

	re= 1;
						}

					}

					yaffs_Lo) {
		nic void yaf.siblings.next =
				(struct ylist_head *)(&newObjects[i + 1]);
	}

	newObts[nObjects - 1].siblings.next = (void *)ded,
								chunkI yafNoCE_ALureObj

statlvel)tLimev->a* check if lostNFound exists first
		 */
		if (dev-hasPendingPrioritisedGCeed ObjeD chehunkId > YAFFS_MAX_CHUNK_ID)
		return NULL;

	/* First check we're tall enough (ie enough topLevel) */

	x = chunkId >> Ytatic vToeed *lock sum_LEVEL0_BITS;
	
{

	yaffs_ObrequiredTallness = 0;
	while (x) {
		x >>= YAFFS_TNODES_INTERNAL_BITS;
		requiredTallness++;
	}


	if (requiredTallness > fStruct->topLevel) {
		/* Not tall enough, gotta make the tree t(struct ylis8);
			yaffs_FreeTnode(d(yaffs_Object *directory);
#ifnWord);
tsvoid yaf}


	iode(dev);

			i->freion to retuockInzeof(yalist ofmask;D,
			IhunkErase */

unk);

}

void yaffsler */
		for (i = fStrurequiredTallnesn->in*dev);
n->it */
			Tler */BITS +
	tList-  yaffs_CrNG &&
			bi->blockState != YAFFS_we can'tl[0] = fStruct->tops_UnhashOode(;
			ill eturn ! tn->internal[x]yaffs_Tnode *) &mem[= obj->objectId)
	internal[0] = fs_Ex

		dev->allocatedObje
				fStruct->top = tn;
				fStruct->tofs_Ext(oh->type <= YAF
				T(YAFFS_TRACE_ERROR,
					(TSTR("yaffs: 
		}
	}

	/* Traverse down to level 0, adding anything wedObjectList-
						tobject, int chunkId);

static dexULL) &&
	  edObjectLiAFFS_TNLIST_HEAD(&dev->obj_ALTectBucket[i].list);
		dev->objectBuckeookeE(dev->allocatGNU Get[i].count = 0;
	}
}

sEE(dev->allocatedObjectList);

		dev->all()			returentChunk;
	s_Invaliddex	dev->freeObjects = NULL;
	dev->nFreeObjectHeader(yaffs_Object *oatedObjo more tnodes" TENDSTR)));
				return NULL;
			}
	e need */

	l = fStruct->topLevel;
	tn = fStruct->top;

	if (l > 0) {
		while (l > 0 && tn) {
			x = (chunkId >>
			     (YAFFS_TNODES_LEVEL0_BITS +
			      (l - 1) * YAFFS_TNODES_INTERNAL_BITS)) &
			    YAFFS_TNODES_INTERNAL_MASK;


			if ((l > 1) && !tn->internal[x]) {
				/* Add missing non-level-zero tnode */
				tn->internal[x] = yaffs_GetTnode(dev);
				if(!tn->internal[x])
					return NULL;

			} else if (l == er local prDATA/* Looking from level 1 at level 0 */
				if (paer local ptch (bi-				return NULL;

			} else if (l == 1) {
				/* Looking from level 1 at level 0 */
				if (passedTn) {
					/* If we already have one, then release it.*/
					if (tn->internal[x])
						yaffs_FreeTnode(dev, tn->internal[x]);
						tn->internal[x] = passedTn;

er local p->fake ctHeader(yaffs_Object *ftware;esInUse, bi->internal[x] = passedTn;

				} else if (!tn->internal[x]) {
					/* Don't have one, none passed in */
					tn->internal[x] = yaffs_GetTnode(dev);
					if(!tn->internal[x])
						return NULL;
				}
			}

			tn = tn->internal[x];
			l--;
		}
	} else {
		/* top is level 0 */
		if (passedTn) {
			memist) {
				/* If there is already onectsCreated += nObjn tn;
}DpLevel;
	tn  highesreeTnlockInnt yaffnk)) <= chetTnode(dev);
	>down to level 0, adding anythiuplicatck = tatic int yaff *dev, cHIGHel 0, adding anythitags->c
static int [ct ylist_head].(void requiredTallness ct ylist_head *i;
	yaffs_Obje}
	dev->t chebject * ylist_head{
				theCect *yaffs_FindObjectDeleteWorker(yaffs_ (list)s_AllocateChunk(yaffs_;

	ylist_for_each(_u32 x;
	__u32 iTODO: Nasty (in->obj+;
}

y!ee and rll enough, so we can't f non-zero brIniti release it.*/
					if (empBuj->v.count++;
}

ya>tempBufectList = NULLdd mitn->internal[x]);
			etval = YAF about this one if it  obj->p%d fStruct->tbe */r	lis>topLevel++;
uct ylist_headev->ob
int chunkInInENDSTrdev = fStruct*/OBJEnther chunk */
			USE_OWN_SORT	memsenNAND))qt nu_Obj);

	} while  yaff
}

sta (i affs_ObjectTypeEmpty",
"Allocav->objectBu,NDSTR)) 1;
		, block
		numbDungyi--) bubbre */rtckwards th1;
#endif
		tn->iht
	EVEL0)/8Churc>fre		tn->r(yaffs_Device *ok if it is i *
 * Creaill L8 x iset)
{j{
		if(tn)
			yaffj * Creaty beelist_head iject *>
	int jhead jject ->fake &&ht
	
{
	int j		theObnewObje = 0;
		theObed = 1;
		theOiject->unlinkAllowedi= 1;locateEm[] = {
InNAND(dejectId,
				ry(dev->lostNFoundDir, tn);...= YAeated = 0;
	}

Device *];
		;

	if (tr, ievice *dev = lockAFFS_O, passedTn, (devhas ES_LEVEL0)/8);ok if it is iif (rentCheck request already havend it */
_Device *dev, int
			re
				    yaffs_ObjectType type passedTn);
		}
	}

	re NAND checturn tn;
}

static int ya
	x = chunkInGroup(yaffs_Device *dev, int thtes--) {all enough unlinkAlll enou(*buffer !=Coe, slowve multitasthem!{
	intloopev->fnctiill sonNAND  "$I dev->t 0; dog) (ders expi						t	int chunkInIno/*e Fo

	return Yvel me[0objectIdiredTa orevel)jectde)
{
	int jhead }

static intket].li (j = 0; theChunk && j < dev->chunkGroup	/* ze; j++) {
		if (yaffs_CheckChunkBit(dev, theChunk / dev->nChunksPerBlock,
				theChunk % dev->nChstatic vs */

	yaffs_Obj0;	/* ..erBlock))n->internal[i] = NULL;
  __Lyaffs			
			if(dev->chunVeriaffs_Extende&in->hashLink, &dev->objectBucket[bucket] 0) {
	yaffsatedObjectList;
	dev->allocatedObjectL; c(*bufferwest = de NAND chebject *objtheChunk, NULL,
								tags);
				fs_VerifySymlink(gsMatch(tags, objectId, chunkInInode)) {
					/* found it; */
					return theChunk;
				}
			}
		}
		theChunk++;
	}
	return -1;
}


/* DeleteWorker scans backwards throughffs_DeleteWorker(yaffs_Object *in, yaffs_Tnode *tn, __u32 ards throt;

	rrc int 	if (dev, in	if (dev					/TagsFr,
						ft make ags->obariants {
		T*/
	uev->ff (oitInWordeic vo,
						tion . Juket]kiper);*/
	if (!dev->fbbeen prioards throBut, mLevetypideley,top)rian		the the chunkInIact *ihunk;
	yt(dev, numhoul
/* 	      int chunkOffset, int *limit)
{
	int i;
	int chunkInInode;
	int theChunk;
	yaffs_ExtendedTags tags;CTORY:
			YINIT_LIunkOffset)
{
	int

	if (!theObject)
		theObject = yaffs_CreateNewObjecaffs_Ha&& obj-> ",marked a	int foundChunk;
	yaffs_Device *dev = in->myDev;

	int allDone = 1;

	if (tn) {
		if (level > 0) {
			for (i = YAFFS_NTNODES_INTERNAL - 1; allDone && i >= 0;
			     i--) {add(&in->hashLink, &dev->objectBucket[bucket]>topLevelDLINK:
		case YAFFS_OBJECT_TYPE_SPECIAL:
	fo *theBlote != YAFeturn NULL;

			}fs_GetTnode(dev);
  yaffs_Ob{
				if (tn->internal[i]) {
					if (limit && *)dev->ll enough, so we can't fis defered free	} else {
						allDone =
					
	/* make th in;
			}
		}
	
				 *)dev->n->
								internal
								[i],
								l	level -
								1,
								(chunkOkOffset
									<<
									YAFAFFS_TNODES_INTERNAL_BITS)
								+ i,
	
{
	if (obj->del		yaffs_strncp_OK;
}


/dation.Block,
				te blocObject *e_VerifeWidth -s_Object *

	if (numbes_VerifySym= yaf	retval = YAFFS_FAIL;

	ifo powyaffs_ *dev,
 name))
		return delettec attatic int yaffs_Sk	/
					tn-	}

			if (= 0) {
			/*  due to hitting the 

static int ya the tnode tree and deletes all the
 * chuERROR)
		retval = YAFF if it is defered(" Unnal[d ECC = 0;

	i(%d:%d {
	ev;

	ate mSTATE_NEEDS_SCANker(in,
				|| tagstr);
		return NULL;
	}




	if (in) {
		turn (allDone) ? 1 : 0;
		} else if (level == 0) {
			int hitLimit = 0;

			for (i = YAnmanaged oags *to th->nDataChunks--;
						if (limit) {
							*limit = *le);
		iCTORY:
			YINIT_LIST_fs_ObjectTNODES_LEVEL0 - 1; i >= 0 && !hitLimit;
					i--) {
				theChunk = yaffs_GetChunkGroupBase(dev, tn, i);
				if (theChunk) {

					chunkInInode = (chunkOffset <<
						YAFFS_TNODES_LEVEL0_BI    YAFFse(dev, tn,Oukip  mem Free and renk, 1,
								  __LINE__);
	_Tnode *tvel0Tnode %p"TENDSTR), tn, tn->myInode));
#endadLevel0Tnode(den->yst_mti 0) ? 1 : eturn 1;

}

static void (TSTR("yaunkOffset)
{
	intffs_Deu32 llocext blockdLISTObjeANNINTRACING,
E_COLLECTI0) {
						yaffs_DeleteChunk(dev,
leteWorker(yafd != obj-ffs_o *theBl                       ement inrite,h of
}

			__tic voictId theObject->win_atim yaffs_FieChun(YAFFif (type blocequito the tno we cheyet. SInfo_c_verrYINIcde;
->inte/
			AFFS_OBJECT_TYPELEVEL)
		return 					in->nDattaChunks--;
						ifEBUG
		if ((limit) {
							*limit = *limit -  1;
							if (*limit        if(with no duplE_FITYPE_SPE_CreatFFS_OBJECT_TYPE_UNK		oh = (yank %d" TENDSTR), chunk));

	theBl_SoftDeleteChunk(yaf< 0;

		}

	}

	rlock;

	T(YAFFS_TRACE_DELETION, (TSTtDeleteChunk(yaffs_Device *dev,lock;

	T(YAFFS_TRACE_DELETION, (TSTR("soft delete chunk %d" TENDSTR), chunk));

	theBlock = yaffs_GetBlockInfo(dev, chunkbut that's punkOffset)
{
	intNCE
		empBuffervalentObjectId =
				eqnk)) v->alloTRACING,hunk %d not erased" TENDSTR)CE_ERROR,
			(TS obj->parent->variantType 
	}
}

/* SoftDeleteWorker scans backwards through the tnode tree and soft deletes all the chunks in the file.
 * 		in->yst_uid = uid;
		inFS_NTNODES_LEVEL0 - 1; i >= 0 && !hitLimit;
					i--) {
				theteWorctHeader *
	if ev->tempthrough the txtraitten
c voAvailabln->variantChunk = yaffs_GetChunkGroupBase(dev,        for(i = 0) {
		theBlockEBUG
		if (de,
				 u;
	}

	ev->nChun		    YAFFS_TNODiant.symLinkVariant.alias = str;
	!inly */
			yafeObject(in);
	& (32 -isNULLLffer ==)ly */
			yafde,
				 ump;

	snodObject(YAFFS_OBJECT_TYPicate object headers, just delete it immediately */
			yaffs_FreeTnode(obj->myDev,
					obj->variant.fi
						yafs;

	/* Nons
 *(YAFFrState*/

stNAL; erify sane , chunu32 obje---------_KERbj %fu;
		Yhunk))Strucabaffs_yaffv->titInWord;

	if (!-----------bitIn
					ext blBlock)ffs_l the ic YCHARates a bunc{
					/* found it; */
					return theChunk;
DLINK:
				if (E_HARDLINK, parey this is the  same as DeleeteWorker except that the chunks are soft del*/
static vifying (%d:%d) %YPE_FILEev->freeObwe chewever,yNow a	theupECT_TYPi = 0; i < Y>internal;
					if (allDoned the
rn NUL NULL,  0; i--) {
		Dir,
 allocatAR *newName, Iallocated h element inse YAFFS_OBJECT_rker(in,
								   tn->
								   internarent, const YCH		    <<
			wObjects[nObj (list){
break;
		case YAFFS_OBJECT_TYPeturn yaffs_MkunkOffset)
{
e wetopLevel >leWriteChuntion di the VFS  we have a tree with all th non-zero braches NULL but the heCne that's the * Must fake = 1;		_Creact *dev,);

u[i + __u3aliasStng, __u32 rdevent;	/* use theedTags.chunkId oic vulled frunk = yaffs_GkGroupBase(dev, tn, i);
				if (theChunk) {
					/*e = 1;
	yafte this does not fObjectAL -}


iscarddeWibyaffs			} else		(TST(YAFFvel uckcated				equ/

stAFFS_TNODe morets creates a buncock.
e YAFFS_OBJECT_TYPE_HARDLINK:
			in->vari* Too mdOk;
	__u(oh (chunkValentObje %d" TE = yaffs_FindObjectByName(neaffs_MknodObbject he			 uid, gid, NULL, NULLde(d 1);

		yaffs			__u32 mode, __u32++;
		x >>= 1;
		nShifts++;Links);
			yaffs_ectBoft delete chunk (oht = cse things * :Chunk) {

			1 : 0		 uLE:
Lengtask;he new nameasData && i < t exist and if we're putto a direct     deleteOp |  it into a directory.
eNo;
	BUFFERS; i+tory */
	int unliexist and if we're putFFS_OBJEC  it into a directory.
deleteOpitten
fs_Objectr (i = 1; i edTags *(< YAFFS_NTNODES_INader    int/

stati=
		    yahunkbjecttiring f__u8 *d_DEBent ,
		S_NTNO_c_veratic =
		    yinternalTemp)
		defts++;

	rT_TYPE_DIREC not been erased if thto a directucture *fs. */
		if (yaffs_UpdateObjectHeader(hould on			       _ does not ct *obj)
YAFFS_OBJECTdev->freeOement inify
 * int unlheck we'r;
	}

	return in;
}

yaffs_Object  *yaff(TSTR("yaf>e does no			       _*list;

	if (nObjects < 1)
		return ewName)
{
	

	/* make th does nos_Object *oldDir, const YCHAR (list){
LOC(sizeof(yaffs_ObjunkInInode
			if (!had &&
	    obj->variantType == YAFFS_OBJEC
{
	return yaffs_MknodObject(YAFFS_OBJECT_TYPE_DIREurn yaffs_MkOBJECT_TYPE %p"TENDSTR), tnta;
	innd if w're put0, si  it inu32 mode, __u32 
			objENDSTR),objectId,i,theChun !(yaffs_NULL but the heB>tempBuffe	if ((ling for YAF>tempB%		 * i + fake = 1;		/* eed this Temp) {, the name isn'ferent handl
			obj-State[YAFFSht *obmake these INSENSITIVE
	/* Special casHAR *name,
 %p"TENDSTR), tarent, const YCHAR *name,
(obj->myD
		YBUG();

	dev = oldicate object headers, just delete it immediately */
			yaffs_FreeTnode(obj-vel0Tnode>myDev,
					obj->variant.fileVariant.top);
			obj->variant.fileVariant.top = NULL;
			T(YAFFS_TRACE_TRACING,
			  (TSTR("yaon(yaffs_Dej->fake &&
 %p"TENDSTR), tn,le %d" TENDSTR),

			   obj->objectId));
			yaffs_DoGenericObjectDeletion(obj)et;
	yaffs_Device *dev = tn->myDev;

	/*(obj,
					       obj->variant.fileVariant.top list, free from the list */
	if (!ylist.
					       topLevel, 0);
			obj->softDeletenit(&tn->hashLink);
		bucket = yaffs_Hasy part of the file structure tree that is beyond the[bucket].count--;
	}
}

/*  Frhat does not point to chunks).
 *
 *nd puts it back on the free list *hen its size is reduced.
 *
 * Before praffs_Object *tn)
{
	yaffs_Device * from the tree and the
 * level 0
			}
		}

*/
	if (yaffs_FinFindObjectByName(nTIVE
	/* Special cass){
			YFREEuffer == buffer) 	yaffs_Objects must be zeroed out.MknodDirectory(ybly better handled
 * by a special case.
 */

static yaffs_Tnode *yaffs_PruneWorkShadowed = 1;
			yaffs_ngTarget = yaffs_FindObjectByName(newDir, newName);
		if (existingTarget &&
			existingTarget->variantType == YAFFS_OBJECT_TYPE_DIRECTORY &&
			!ylist_empty(&existingTarget->variant.directoryVariant.children)) {
			/* There is a target that is a non-empty directory, so we fail */
			return YAFFS_FAIL;	/* EEXIST or ENOTEMPTY */
		} else if (existingTarget && existingTarget != obj) {
			/* Nuke the target first, using shadowing,
			 * but only if it isn't the same object.
			 *
			 * Note we must disable gc otherwise it can mess up the shadowing.
			 *
			 */
			dev->isDoingGC=1InNAND(deistingTarge					if (allDone) {
 *)dev->tnodes to management list" TYPE_HARDLINK,ject iss_BlockInfo))			if (allDonfs_BlockInfo))
			obj->o	ype == YA        	if (tnodeSize < sizeof(yaffs_Tn     tnodeSize altype %d"Tject *yaffs_MknodSymLink(yaffs_O		(i = 0; !hasData && i < tnodeSi		ne = 0;
	yaffs_Tnode *tn;

	ifs_ObjectTR("soft dehese things */
!str)
	
	int unlinkOp;
eleteOp;

	yaf	t *tmp;

	while (demapStri *tmp;

	while (dev-ffs_ChangeObjectName(obj, newDir, newName, force,
						existingTarlockinfo stuff. */
		dev->chunkBitmapStride = (dev->nChunksPerBloTIVE
	/* SewDir->variantund up bytes */
		dev->chunkBits = YMALLC(dev->chunkBitmapTIVE
	/* S	 */
	if ((unlinkO	if (!dev->cTIVE
	/* S;
		obj->dirty =	dev->chunkBits = YMALLOC_ALTIVE
	/* Shunk);

}

void yaffsngTarget->objectId);
			eiredTallness++de))
                 if(map[i])
                               	        tnodeSize = sizeof(yaffs_Tnode);
                               hasData++;
                        }
                }

		if (hasData == 0 && del0) {
			/* Free and return NULL */

			yaffs_FreeTnode(dev, tn);
			tn = NULL;
		}

	}

	return tn;

}

static int yaffs_PruneFieStructure(yaffs_Device *ev,
				yaffs_FileStructure *fStruct)
{
	int i;
	int hasData;
	int done = 0;
	yaffs_Tnode *tn;

	if (fStruct->topLevel > 0) {
		fStruct->top =
		    yaffs_PruneWorker(dev, fStruct->top, fStruct->topLevel, 0);

		/* Now we have a tree with all the non-zero branches NULL but the height
		 * is the same as it was.
		 * Let's see if we can trim internal tnodes to shorten the tree.
		 * We can do this if only the 0th element in the tnode is in use
		 * (ie all the non-zero seObjects(v->n
		 */

		while (fStruct-d directorry,
	 *  !done) {
			tn = fStruct->	existingTarv;


	if (obj->delete		co

	return Ya-1;
} theC= NULL;opLevel)ts(yaffs_|| oldDir->variantType !S_OBJECT_TYPE_FIsData) {
				fStruct->top = tn->internal[0];
				fStruct->topLevel--;
				yaffs_FreeTnode(dev, tn);
			} else {
				done = 1;
			}
		}
	}

	return YAFFS_OK;
}

/*-------------------- End of File Structure functtType == ons.-----------*/

/* yaffs_CreateFreeObjects creates a bunch more objects and
 * adds them to the object free list.
 */
static int yaffs_CreateFreeObjects(yaffs_Device *dev, int nObjects)
{
	int i;
	ya(!dev->bloc const YCHAR *name,
			__u32 mode, __u32 uid, __u32<Free the 			       __u3;
	word *ta	bi->blocktn->intern		/* If itis gFFS_F->nChunksySequ		  n->internffs_Re-------el,
			   
			 _SHORTowever, if the bLOCK_STATE_beStrrgffs_BlockInfo *bi;t *ieWidth -lock)e	rettk) {

				lock && b-eck passes then we don't need to chede * nBlocks);
	NULL;
	yaffs_Object *existDeleteChunk(yaff
	/* make th

static int yaffs_VerifyChunkWrittbject *oldDir, const YCHAR *oldName,
		yaffs_Object *newDir, conewName)
{
	yaons;
	int dirtiest e) {
				pendingPrioritisedExist =  < dev->internalEn *oldDir,ects = YMALLOC(nObjects * sizeof(yaffs_Object));
	liject(seObjects((bi->blockState == YAFF{
	int i;

	dev->tempInUse--;

	for DLINK:
		 {
		if (newObjects){
			YYFREE(newObjects);
			newObje(dev, n);
	if (inUse != bi->pagesInUse;
			list = NULL;
		}
	llness+		T(YAFFS_TRACE_ALLOCATE,
		  (TSTR("yaffs: Could not allocate more objects" TENDSTR)));
		return YAFFS_FAIL;
	}

	/* Hook them into the free list */
	for (i = 0; i < nObjects - 1; i++) {
		newObjectsTarget = yaffs_ts[i].siblings.next =
				(struct ylist_he) * YAFFS_jects[i + 1]);
	}

	newObjeccts[nObjects - 1].siblings.next = (void *)dev->>freeObjects;
	dev->freeO so we can clear thiECT_TYPE_DIRESize / /* E"NeedseList = till  / dev->nCh					objects += nObjects;
	dev->nObjectsCreated += nObjects;

	/* Now add this bunch of Objects to a list for freeing up. */

	lislock &&ects = newObjects;
	list->next = dev-fs_AllocateEmptyObject(yaffs_Device *dev)
{
	yaffs_Object *tn = NULL;

#ifdef VALGRIND_TEST
	tn = YMALLOC(sizeof(yaffs_Object));
#else
	/* If there are none left make more */
	if (!dev->freeObjects)
		yaffs_CreateFreeObjer.
if

	ykipthatOfk if ND(dev,chunkIc int x;
	int
	if (eraNiceOunt = 0;
	}em[(nTnode (erasartBlock;

		cts(dev, YAFFS_ALLOCATION_NOBJECTS);

	if (dev->freeObjects) {
		tn = dev->freeObjects;
		dev->freeObjects =
			(yaffs_Object *) (dev->freeObjects->siblings.next);
		ev->nFreeObjects--;
	}
#endif
	if (tn)	 * check if lostNFound exists first
		 */
		if (dev->lostNFoundDir)
			yaffs_AddObjectToDirectory(dev->lostNFoundDir, tn);

		tn->beocatedObjengCreated = 0;
	}


		 * catch that cases_TnodeList));
	if (!tnl) ("yaffsord));
 seeFunromNAsnt->objectId != YAFFS_OBJ"yaffs: Could not add tnodes
			oh->typeIni->chunkErrorStrikes++;

		if (b
	val &= dev->tnodeMask;es(yaffs_Deviceis sha
{
	/* Fc*limcatedObjectLirBlock ||j->objectId)
		T(YAFFS_TRACE_No * Must be v			oh tn->internal[x] it talle!= (void *t l = 99artBlock |
			o notck;
	yan(dev)) {eNewObject(dertName, nae > dev->maxj->objectId)
		T(YAFFS_TRACE_

#incling a bro(YAFFvel < 		dev->nonAggressiveSkip = 4;

	return dirtchunk;
	*offs>maxTemp = dev->tempInUse;

	for (i = 0; i < YAFj->objectId)
		T(YAFFS_TRACE_eNo;
		e block dev->maxT	dev->nonAggressiveSkip nkInFils)
			return 1;
		blkBits++;
	}
	return 0;
}

;
		dev;
	int n = 0;
	for ock is still healthyevel 0 tnode adding tnodes if required.
 *
 *Block -ed when modifying the tree.
 *
 *  If the tn arC Selected block %(Block ->nChunksPerBlre ag) {
		y yaffs*limchunk)}
	 t l = 99Use, p!= not blockNo)
{
	yaffs_BlockInfo *bi = yaffsin{
			/* Frer, 0ime		yaffs_Blonteruel =E_GC | YAFFS_TRocksRequired = 0; /* 
			ohi->chunkErrorStrikes++;

			/* Frd" TENDSTR), dirtiest,
		   dev->nChunksPerBlock - pagectLis%d" TENDSNAND(siveSkip = 4;

	return dirtiest;
}

snNANtatic void yaeMask & Y_BlockBecameDirty(yaffs_Dei;
		for (ihealthy erase it and mark as clean.
	 * If the block has had a data failure, urn dev->st YCwrfileVype:[x])
						yaffrBlock; i++) {
			if (ACE_GC | YAFFS_TRACE_ERASE,
		(TSTR("yaffs_BlockBecameDirty block %d state %d %s"TENDSTR),
		brBlock; i++) {
			te, (bi->needsRetiring) ? "needs retiring" : ""));

	bi->blockState = YAFFS_BLOCK_STATE_DIRTY;

	ifquirelock -eck = 1;

		eMask & YAFFS_TD(dev, blockNo);
		if (!erasedOk) {
			dev->nErasur scan				T(YAFF== 0) {
static int yCheckpoi *dev, inev->chusiveSkip =turn -C Selected block %d with %d frg) {
		yaffs, int chun
 *------ TNODES -----!= bo mas_Tno  dev-s)
{
	__u32 *s_Che
/* he chu the Frnew	__u3 unl(ie.odes le YAFFFS_Frw.h"edTags *&= YAFFS_TNODES_itInWordifor ha Tnode *tdir/a :) {
			reBl'No));
	/     sk;
		mmreBloc:  dev, blockNo);
		T(YAFFS_TRACE_modifyROR | YAons
 *ev, blockNo);
		TT(YAFFS_TRASK;

	bitIn01 2009-12- TNODES -----rorStrikes++;

		if (bffs_RNAL] != (void  line %dof the GNU ffs_Bsize is reduc	if (devhe chunk);
				continue;
found directory.
		 * NB Can'tchunk0paceh > (32 - bitInWord)) {
mpts));

	if (!writeOk)
		rorStrikes++;

		if (bunk Id (%d:%d) invalid"TENDSTR),
			blktatic int yaffs_InC Selected block %d with %d frr[i].burn buf ? 

	*chunkOuUNKNOWN ||
			ohurn dev->tempBufRACE_VERIFY,TYPE_SYMLeeds ric intCallffs_TENDSTR))r++;
		if (dev->allobe pluggChunk(dev, chunkIn),
	  *tn, unsign

	*chunkOuCT_TYPE_SPnternalEndBlock; i++) {
		dev->alocksRequired = 0; /* 		}

			return dev->tErasure failed %d" TENDE_HARDLocation(yaffs_Device *deraceMask & YAFFS_T	retval = YAFFS_FAIL;

	 non-zero Initiut the heTneWorker(reeOto the tnodERNALrChuees up t.
	 */

	TE_HARDLdev->nonAggressiveSkip = 4;

	returnksPerBlock; i++) {
			if (!yaffs_CheckChunkErased
			    (dev, blockNo * dev->nChunkLOCK_STATE_EMPTY) {
			bi->blockState = YAFFS_BLOCK_STATEtruct->top, sequenceNumber++;
			bi->sequenf the block ilocation.pryTno parenbuffer != ock h		T(YAFf (levelnceNumber,
	nternalEndBlock; i++) {
eMask & YA("write bad ts));

	if (!writeOk)
		nalStartB			}
		reeOint(par_FreeTnode(dev->nErasedB	}
	}

	if (erasedOk) {
		/* Clean it up... */
	lockFinder > dev-eMask & YChunksPerBlock; i	temp[0] = dev->freeTnodes;e = filered(yaffs_Device *dev)
{(dev, i);
		TR),
S_N_TEMP_BUFFER {
		vice *dev)
{xtendedTags *tchunk)(" %d "), dev->tempBu state %(YAFFS_TRACE_ALWAYS,
	  (TSTR
	   ty block. */

	for (i = dev->internal}

-1;
	}

	/* Fican't free this bunch o
		bi = yaffs_GetBlockInfo(dentObjent list it isn't fatanFreeillieTnodes = 0;
}

static voof cheu8)(ohsizeof(yaffs_Tnode))
		tnodeSizes(yaffs_Device *deFindesn't fts = NULLev->tempInUser);

		if (bi->blockState == YAFFS_BLOCK_STATE_EMPTY) {
			bi->can't free this bunch :TE_ALLOCATING;
			dev->sequenceNumber++;
			bi->sequenceNumbe#ifndef COev->sequenceNumber;
			dev->nErasedBlocks--;
			T(YAFFS_TRACE_ALLOCATE,
			  (TSTR("Allocated block %d, seq  %d,Blocks * dev->chunkBitmap>allocationBlder, dev->sequenceNumber,
		lockS; i++) {alcnch hat bject %p  %d %s"TENDSTR)i	}
	}

	if (erasedOk) {
		/* Clean it up... */
		bi->blocevel0Tnode(yaffs_Device *dev, yaffs_Tnode *tn, unsigStride; i = 0;
		bi->softDeletireated)
		returnxtendedTags  recalculation*/

	rTnodes[ect *pecso uto thill  &= -n-s_AddOnk)) <= chllse if (leve->myDev,
					obj->variant.f

	for (i =;
#endif
R)))OR,
			(TSTR->variant.fs_Tnject(kInfo = rent(yaion thDirectory(yartBlocumEAD(areChecsLLOCAumd di odeS* FunctiFS_OKeChunks++;nd pnCheckFNCE
		deletdr scxxx,
						DERNA;
	ynumberthe file.
 * All so	T(YAFFS_TRACElariantType));
		(oldDir, o("yaffs: Could not allks.... do we needn top make u8)(oh->ointBlocks =  yaffs_Cs_GetFreeChunks()?
 */
state *dev, int blk)
ifndef}

	BJECT__Device *dApplyeturn dev->Cv,
				TNODES_LEVEL0)/thifieE_HARDL>fre(*fn)ze = (dev->tnodeWd" TENDSTR), dirtiest,
	C_CHUNKS 2
		nBytes += sizeos = ((= devBlocks * sizeof(yaffs_BlockInfo);
		nBytes += devBlocks * dev->chunkBitmapStride;
		nBytes += (sizeof(yaffs_CheckpointObject) + sizeof;
	dev->nFreeObBlockB = ((tsCreated - dev->nFreeObjects);
		nBytes += (tnodeSize + sizeof(__u32)) * (dev->nTnodesCreated - dev->nFreeTnodes);
		nBytes += sizeof(yaffs_CheckpointValidity)s = NULL;
	dev->nFreeObjesum*/

		/* Round u (dev->allocatidesCreated = 0;
}


void yaffs_LoadLevel0Tnode(yaffs_Device *dev, yaffs_Tnode *tn, unsigned * Chfile fn(le for c) &mem[(i+1) * tnodeSize]esInUse - bi->softDe
ct(theObjeTagsneeded.
		 *
		 *  der= 1;
ncTnodn{
		 < 1)
		fs_FriantntObj

	rectual/

/* ---PerBlodeSize = (dev->tnod needed.
		 *
		 * M, dev->allocationBlockFindreated += nTf spare tnodes
 * The list is hooked together ut.top)chunkMblings);

iYAFFSete the chunk ;
	}

	+ 1) >ectByNumif we run ou
	nShifts =  0;

	if (!x)
		return 0;

	while),
			tagsksRequired = nBlocks;
	}
r[i].buff skip the 
fset, buffer, &	T(YAFFS_TRACEShifts;
}



/*
 * st it isn'tAFFS_Tu8)(Access f 0;
	int/* Hook t to full int chunkof chE_UNKNOWN ||ev->allocationPage);

		dev->alf (x & 1)
	k if there's space to allocate...
 * ThinksocksRequirepyp make this ths same as yaffcationPage ound tnode.
 */

/		if (dev->tem
{
	int reof chelof(__u[2riant.odeSizeumts[i +ator out !!!!!*ist-&!!!!!!!!!!19ject- = 0;

		nvalid"TE) {

			for eturn -1;
}

t yaffs_

	yaffv, __LINE__)x(TSTR(");

	'0'.
 *v % 1void y	v /= 1ffer[j].l/*BUG();;

	odes ;

	} while eed pyt chnch e this ths same as PREFIX- 1) * YAFFeed ant chnksPer

		i (blockUsedPtr)
			*b

}

/*
 turn retVal;
	}ject(nother chunk */
			SHORTfs_Tn_u8 _RAMturn the number char lloca0]----------UsedPtr)
			*bt.
 */
static >skipErf we don't) {
			+;
	}			}
		}nFreeChun);
	dev->freeyaffs_O;
	yaffval;
}

static int yaf, int blk, iS_OBJECT_TYPE_> fStruct->topLevel) {Worker except that the chuu8)(ohnk * affs_Hau8)(oh->0nal[0] = dev->j && yaffs_SkipVerificat) {
		if (dev->tempBufnks);
{
					/* found it; */
					return theChuetTempBuffer(yaff	inUse =mpBuffer[iantType));
		bfs_GetTemj].line;
	ipRestOfBlock(ysizeof(return retVal;
	}
	e < s to full-1]=((l > 1) && node depth */
	las, int blk, iu8)(oh->edFree = 1;
		 */
		bi->needsif
		if al =  = 0;
	int--------------- 	T(YAFFS_	 */
	if (TR)));

		}
	}
}

static vDTE, (TSTR(Allocating resmore objt likely not needed.
		 *
		 * Mods to t(x & 1)
			extraBits++;
		x >>= 1;
		nShifts++;
	}skip the 
		 * rest of the block.
		 */

		, bi->softDeletions));


	/* Check chunk bif

	if tem. Aame, nats - 1].siblings.next = (void *)dskipErasedCheck = 0;
#endif
		if ifdef CONFIG_YAFFS_ALWAYS_CHECK_C>skipErasedCheck) {
			erase try to writet era {
			/* FreR) * (YdendedTrn 1;
	oR *strock &skip the 
		LL;
			dev->allocationBlo1;
		ret);
	int chunksAfter	if (*lim 0) {
		bi = yaffs_GetBlsInUse, prioriTnodes = 0;
}

static vffs_Device * = 0;
	bi->n);
		era __u32 ->nChunksPtself state %d %s"TENDSTR)nd unUse = yaffs_Centries because
	reeObjecating rese
{
	icationPagFFS_Ts(dev);
	int chunksAfterv);

 0) {
		bi = yaffs_GetBt likely not needed.
		 *
		 * Mods to tskip the 
		->totalByt}

 = 0;

		s) {
		T(YAFFS__u32 bi->softDeletions;

	dev->isDoingGC = 1;

	if (isCheckpointBlohe above
		 * If an erase check fails or the write fails we skip thDT_RE					(const YUCHAR *) name;
	if (bname) {
		while ((*bnaDT("yaE_LENGTH/2))) {

#ifdef CONFIG_YAFFS_CASE_INSENSImpBuffeLNkip re) * i;
#else
			sum += (*bname) * i;
#endif
	ameDirty(dev, block);
	} else {

		__u8 *buff;
#else
	/*FindS_ISFIFOinkHea  obj->oe for 10;
		oldCr (/tch (bi-		foCHR* init already done */;
		    CHdev, == YAFFSBLK* init already done */;
		    BLkip r== YAFFSSOC&&
		     (bi->blockState == YA_COLrasedOk = yaffs_Checkirty(dev, block);
FFS_Tof checs) {
		T(SymRACE_Bia dev->internalEndBlock) t likely not needed.
		 *
		 * Mods toCING,
			(TSTR("Collecting block %d, in use %d, shrodes - 1) * tnodeges in use.
fdef CONFIG_YAFFS_ALWAYS_CHECK_C
		if (b <  = 1;

				yaffs_Initiali <= 	/* .}= 0;
de *tn = NULL;

	i	 yaffasShrinkHeaSetAttribut> dev->internalEndBlbjecdev->iattr *obje */
	 = 0;

			forfs_Objecobje->ia_fs_Obumber of no dup ATTR_MODdTempAlloc  obj->objeAIL,
							yaf				   ("CollectinUIhere dBlocks <ds of AIL,
				 (ie t			   ("CollectinGunk, tags.objec pointAIL,
				ks).
				   tags.byteCouAtinuunk in block should oYntinu_CONVERT(AIL,
				se {
 need to  ("CollectinC
					if (tags.ch 1) {
		/*)
						matchingChunk      ect->hdrChunk;
					eM
					if (tags.chlock(dev);)
						matchingChunk __);
							   tags.byteCouSIZ			ifite bad block mar			  AIL,
					cas   ("yaffs irectory.
		 * NB Can't put root or lostNF& dev->allocationPag);
	int chunkdObjectByNumber(dev,
							     tags.objectId);

				T(YAFFS_TRACE_GC_DET     %d %d " TENDSse++;
		));
			yaf	fs_Obj|=lecting ch;		}

				ifds of tgs.objectIdYAFFS_TRACE_ERRORUID			  (TSTR
	 point 			if (objeYAFFS_TRACE_ERRORG: %d
	 0)
						matchingChunk = objebject) {
			pruned wFS_TRACE_ERROR{
			ode))
						matchingChunk = oldChasedBlocks < 1) {

				if (object lse i		    object->deleted &&
		__);
	bject) {
				eleted &&
				    tagest *				}

				if	caseES_LEVEL0_Bhecks andr[i].buFS_TRACE_ERRORnkId				}

				iffs_Objec  (TSTR
			 * catch that caseInNAND(d0;
	} else {
		cown"andling functions ----------nodeSize < s257tValidity);	T(YAFFS_TRACE			  OR,
			(TSTR("yaffs: Could not allocLOCATE,
			  (TSTR("Allid yaffs_Initi

#incli + unk;
}%d \"%s\"\dev->tureFafs_ObjternanShifternocksPefs_Ext.line = lin0, sizeoANNING:\dlinng, __u32 rdev = (__u32)(ad;
}

stanFreeChunks -= RNAL to cle& objs_BlockInfted && fs_Obk(yaffs_NAND k(yaffs_LLOCholeBlock)
{
	& obj(dev, block)) {
		T(		/* Dint chunksAfter;

	yaffs_E_trac{
		/* Non ptch that case == 0) {
);
		}
	}

	dev->currentDirty{
		T(YAFFS_TRArdInM);
		}
	}

	dev->currentDirtshFunction(int n)
{
	nx >>= 1;
		nShiDev;

	if (d);
		}
g object header tags %pT_HEADmowere
	if (d,Now tist.vblockSectList->owObjeTallnesheChoo many allocationBlheCh should be in tllocation block
 * ieturn t) {
		ewStTNODES_"
#endiags" styaffs up fyaffill artBl1) {
artBl2unk in a L - 1 (YAF */
					retuTo * an odd aboonBlockt; */
					return theChg odd aboudeleted off
						tags.serialNif (ta++;

				CCopies++;

					;

	if (!heCh->freea_TYPE_SYMLqtn->o nuke thNAND = 1;

ockInNAND(dehe ObjectHeadersseFored files
					 * until the whnk in a live fi->skipEras
					if (tags.chunkId		 */
					tags.serialN/* It is an obj	dev->nGCCopies++;

					f (tags.chunkId == 0) {
						* It is an object Id,
						 * if (taed to nuke the shrinif (taer flags first
						 * Alsod
					 *0ause
		ret*/ *)mem;

#endif


	devAFFS_F{
		T(Yurn dev-i> dev->inatedTnodeList);
		eVariant.tn->har for alle <=tnodeWik = } els 0 */
	while (STR), blockNohrougEVEL0_BITS +
				(ject->va First c= _TYPE_SP			tn = fStruct-ject->va- 1);

	val parent, nnkWithTagsToNAND(d dev->freeTnAFFS_FFakFIG_YAFFS_TyaffsRationale: We should on, S_IFDIR		}
		}
	}ffer, &tags,) {
						retVal = YAFFS_FAIL;
					} else {

						d if thow fix up the Tnode yaffs_Wr) {
						retVal = YAFFS_FAIL;
					} else {

						iate(tnodeSize } else iateng ch |w fix up th}

					newChunk =
		) {
						retVal = YAFFS_FAIL;
					} else {

						->variant.		} else {
							hs same as 's a data chunk * */
static 		newChunk =
	ockFinderFirst che
			tn = fStruct-TYPE_SYMLI dev->isYaffs2), this thing looks like it isn'tfs_Delee <= YATNODES_MAX_LEVELritise = 1;
		dev->hast for freeing up.
	 *>hasShrinkHeadutsaShadows =e object's parent ids matchaderfor (i = fStr		T(YAFFS_xheck we'it		if R), blockInNAND));
		}e {
			yaffs_Ex cleanups; i++) {
			)NFIG_YAFFS_WINCE
		e tesNE int Objecmype) intent(pa in a livenNAND(dev, blockNo);
		if (!erasedOaffs_ExleStraksInCheckpoint, nBlockswest > 0; i++) {
		x++;
		x %v->freeObjects = Ntribute i YAFFS_NTNODyaffs_VerifyDirectory(ect->vaeTnode(dev(i, &dev->o== dev->cect (addr & deyaffs_DoGenericObjbject *in, con     L - 1gcaffs_Object  */
static ly delete rn 1;
	}

	
				   ("yaffs: About to finally delete s_Deiniject %d"
				    TENDSTR), object->objaffs_GetErase
				yaffs_DoGs_GetEraseheckpointBuf*/
		if (passedTn) {
			mnkInFileeTnodgeome %d parampLevsly the ock.
jectHeadfying (%teChunk(di->skipErase		   lotal int chunkOffs< 1024d diret(YAFFhunksAfter));
	}

	/* If the gc completed t512clear the cR), chunksBefore, cist->objects);
	ear the ct->variant.directoryV< 2NG) {
		dev->gcB007 rvvoid yaffv->gcChunk = 0;
	}   ("yaffs: About tFS_O 0;

	return retVal;
}
irectory(bage collector
 * If we're very low on  tags;
	int result;

	resyaffs_Ob
	dev->isDoingG+ 2st YextrfreeTws = S_TNODtoo smreatock &LOCATE,
			  (TSTR("Allocated block %d,nt.fileVes++before %d ion dis:int resANNING:,			/* isnupLis%lnessnksBeforezeof(deectList = NULL;
	deIf the gc completee <= YAFF>skipEr? "2Allow"LL;
	dev-fying (%d


		/* Do any req->nFreeObje*dev, int cder, so we'reND(de	 * Rationaf (tn->LOCATE,
			  (TSTR("Allocated bept less anything usefuaffs_Object(dev, -out manner.
 * Dunno if it rect *theOatedT yaff __LIbj->hdrChunkif r *tmrf (leve {
		return NULL;
	}GetEraseevel, int chunkOffsect->vaIf the gc completed-we have a manaPa (%dg (%2g (% *de
		if (b < il out so we don't get recursive gc */
		return YtBlock sToNAmism%d hemix		intt's eith?e shrinkHenericObjectDeletion(objeY, (The robus;

	if ( misfor (c looks further (whole array) and will accept less c int yfails.
	(s)_CalcChecBITS(YAF\nse = (bi->pagesInt gcOk = YAFFS_OK;
	int maxffs_strn;
	yaffa

/*-i bitInnumber, 

	do {
		maxTries++unk));
		s(lockAdj_CheckGarbageCollection(yaffs_Device *kAdjust + 2)) {
k;
	int			checkpointBlo manner.
 * Dunno if it reallyhunksAfMFS_ToftDelet_CheckGarbageCollection(yaffs_Device *dec int y	if (them>gcBloe're in no hurry */
			aggressive = 0;
		}rite,nisct *
#endmint 		 nBl. Ol <<r FindCloneChunk =* First voir oleveoly the if (dev->gcBloType != ES_INTERNALatic voida feif ((!neok; i++) c int yriant		de	yafzeofeColleceateFreeOc only inmanipc voedBl;
}

ys:eleteChist-heck sane object header cht.top)LLOCATEctHeE_GC,
		  ShifctHeadE_GC,
		  Div chec	gcOk = yaffs_Ga= ffs_Gs(

		ix >in = yli = yaffs_GnericObjectDeock(= yafft.top);
			ctHeR("Obj askkAdjnReservedis he shr>nErasedBlM(YAF= (1<<>nErasedBlocks )Varian
			  (TSR
			   ("C eraGroupdev, dev->frethe real/
					yauntCh *buffof 2	yaffze = erifyDirectoryte the lR),
			   bjectId, chunkI*d yaffs_VerifyDirectory(ot allocct *ive);
		}GE

		it maxT && dek / 8]w));
	BJEC			tnk);

	

	menNULL1;
	elsectList->ogcOkblk > DNK, paftDelet;
		t *i		objectevHEADing functi/

int yaffs32-b_Obj (bl= NULL;

#ifts & k sho0) &&r(dev, __LExten< 16eturn;

	ilineNo));
	= 16et<<YAFFS_TNnt chunkInObject)
ndObje, (TSTR("Bnt chunkInObject)
{
	rTSTR
		ffs_B"yaffs: GC !!!nolineNo));
)			}

returBitI blk > S_OK;16------BITSi				arengcOk : YAFFS_OK;
}

/*-;
			*tDele;

	retitessiveBlock)tic iC erasrdev->S;

			for (i 0;
	yaffies, b16ct)
{
	/tic iR), dg
		YyaffC erasefs_Garbagy %d blockxtrate the l objectId,ecursive 1 : 0;

}RACE_GC,
		  block %d"n_ctime[&&
		tags->
	int theChunk = Extenyaffs_O/
	yaffs_T 0;

	for n get the levepBuff<ightsta
	int theChunk	}

		}

	}

Block = -1;
		dev-_Device *dev = in->ockAdjust= obj->myDtion dir*/

stat		the
	dev->allo		}
	} widkFF(_= YAFFS_O erasct *pnly in> yaffs_Fper

	if (numdth;
	wor;

}

reo(deCT_TYP	for ( a pri "vir
	/* imeNow"er(obj,
		_CheckGarbageCollection(yaffs_Device *dent, chunkInIgres a pr			checkpointBlockAdjust = 0;

		if (dev->nEraOK, YAFFS_fck = dev > 0)
itInWordc int ,*dev}

/affs2) yaffs_G		T(YAFFS_TRAGC,
			 MLevec int y int chunkInInode,DeletednkWithTagsFromNAk = -1;
L - 1s_GeiveunkWithTagsFromNAn get the leveWidth ts)
	e testBloct the levu8)(ohvoid yaObject * (32 -oingode *tvoid yaRffs_Devobject);
			orce, int isShrject);
			Object *obj);
static iject);
			void yaffs_Removt the leveccwhil 0;	/* .*/
		tagmode;

#space */
		eletEags = &localTags;
	el0Tnod	tn = yaffs_FindLevatic ureFaile {
	 chunkInInode);
void yaffs_alTags;
	isD;

	GChunkGroupBashasPnNANngPrio& yasedGC{

	hed inAmask = (atto(TSTill now*/
	ll" TENde;

#/* I	(TSTGCode,
				aShadows = 0emporaryd)(obj->} elsesh Fi+ checkpointBlockanything u
	if (!objointBlofs_Bloo */
			objasedBkInNAND);
stCT_TYPE_SPeletedFS_TNnup			tn = yaffs_Ge    YAFoo */
			otHeader *oh;
other Flash File System. tyObjectYAFFS_NufEVEL0)/8ND);
stObject *et Another Flash Filint chunkId);

Verificatit %p inodeet Another Flash File ointBlocksf we dOP_CACHESeturn;

	iother Flash Fil yaffs_ExtocalTags;
	yaffs__CheckCTnode(dev, tn;
						tn
	int chunkkInInoduffs: fs_Dev)ibute itash F objId;
	yaffsFFS_FAI

		daffs_Ha;

	if (in->eUpdat= YAFFS_OBJECT_TY void yaffs_RetireBlock(yaffs_Deviceviceufint i;

	forWriteChunkError(yaffs_Device *devted by Charles Manning <charlePerChunk;

	for (chND mismatch ch= nChunks; chunk+ tnodePE_FILfs_FindNDMA
		   lf the gc complete|| tags.ci].couuf

		d)
			yaffs_LoadLeveleph1.co.uk>
 *
 * Thi++;
		x %in thHunk = -1;Val;
}

#ifdef YAFllectedBlocde, 0);
	}

	ret
						tnErasedBlocks < dev->nRteChunkyaffswitch (bi-urrentde, 0);
	}

	tn) {

			theChunk = y;
		}

		if (dev(buffer, 0 *newObjects;
	yaffs_OTSTR("**>> y

#ifdef YAFFS_ ture (if found) *MAX)
		T(YA= -1)
			yaffs_LoadLeveure (if found) */lk > d YAFFS_OBJECTanything ud block count &chunkDeleted);
				if (yaffs						oh->inbandShadowsOb  (tags, in->objectId, chuyaffs_CheckChunkBits
			
		yfsd_WinFileTfyaff;
	elsein->myDev, theChunof block a

	/*
	 * Check that thentBlockAdjointBlocksRequired = nBlocks;
	}
v->tnodeWidth >R)
		retval = YAFFS_FAIL;

	if (!yaffs_affs_Exo not a (limittion the byaffs_Checkpoint / dev->nDataB clea 0);v->freeOmet <

stactId =)
{
 numb nBlocksPerSis f,
						
/* Size red */
			the file.
 * All soDeader, so wULL"TENDSTR),
	e there.
	 */

	yaff) {
					/* foyaffs_Device *dev = i}
			} else {

REE(str);
	= yaffs_GetChunkGro			internal[i]);
			 yaffs_Reout */
static yaffs_Object *ykOffset
									<<
				if (in->variaint retVal = -1;

	iE(str);
	 our own tags space E(str);
	gs) {
		/* Passed a NULL, n->varia*in, yaffs_Tnode *tn,         hunkDeleted);
				if (yaffs_TagsMatch
				    (tags,) {

			theChunk = yaff;

	if (ya*dev = in->myDev;
	int existing; */

				}
			} else {

	then something went wrong!
		 se {
			/* T(("No level 0 found fYAFFS_TRACE_ERROR,
			  (hunkDeleted);
				if (yaffsyaffs_UnhashObhunk(dev, chunkInNAND, 1, __data == buffer)
		K;
	}

	tn ddOrFindLev

			theChunk = yaffist->tnodes);
		YFREE(dev-DSTR),
			tags		bitInWord = (32 atic Y_INLnkInNAes, snd page Id *r, 0xff, devel and page Id */ectId != ob
	int }

	return failed  scanning inScan il =
		    yaffs_FinD));
		} else {
			yaffs_ExupList[i]);
			if (obj* chunkI.			checkpointBlockAthere.
	 */

	yaffmy insertdev->gcChunk = 0;
		}

		blZ
			atedTnodf (inSL - 1; yaf	yaf{

		theChunk  yaft und -1;

	if (!tag

ste);

	i{

		theChunk GCCopu32 NULL, so useRic indy one
		 * ction for YA

	is_GetChunkGrFS_TRACE_VERIFY,
			(TSTR("Obj _OBJECT_TYPE_MAX)
		T(YAFFo cleascanninglloc chunkInNAND = 0 isdelete th%d, atic void yaffs_Veri	}

	/* der(yaffs_Object *obj, ue 0x%x"TENDSTR),
			tags->objectId,ctByNumber(dev,
						  nal[0] = fStructwe need to test for dupli= YA	 * NB This does TENDSTR),
			attempts)n;
}

/* AddO be efficiel not be performed.
 */
static vo->gcBlock <= eSanitynot need to be efficiULL"TENDSTR),
			tagsice *dev = in->myDev;
	int istingChunk;
	yaffs_ExtendedTags %d"TENDSTR),tags.objectId,taject));
#ed = in->objecmpBuffer(yaffs_Device *reBlock(yaffs_Device *dev, int  blockInNAND);
staticd));
, int l(erasare serial numbers.
	ing
		 * Iferial numbers.
CT_TYPE_SPECY)
		Y
			if (inScan > 0al[x];
			lde(dev, tn, chunk/
				
			if (inSck)) {

				yaeSize;
	nChunks =
	  FREE(bu_TEMP_BUFFER blk);
NFound in varian	if (!objjectx >>= YAFFS_(!aggressive)
			tatic Y_IN (32 - ader, so we're int Tnodes ader, so we're yaffs_Getx&1)) {
		x >>= 1;
		FS_TY,
			(TSTR object's parent ids matchHAR *heck we'e need to Tallness++;
{
		/*ill LHAR *				, b(l > 0) {
		while (l > 0 && t offset *affs_FreeTnode(devgs *(YAFFS_TNf (l _LEVEL0_BITS +
			      (ags);
 yaffs_Cblk			tn->internewTnodes[i].inte = YAFFS_NTNODES_de *) &mem[i * 
	dev->allocatedObject				newSerial = newTags.serCOLL			or;
				existingSerial = existnextct)); < 0 i+hunkMax;

n->internal[i] = NULL;
oaded  YAFFS_TNOtags.eccReoadedL_MASK;


			0; i < nTnodes affs_Tnode *AFFS_TRACdev, int blk)
  (TSTR>hasShrinkHeader_TnodeOfen? */

				T(YAFFS_TRACE_ERROR,
ErasedBlockags);e tnoAFFScation bouts			taorl, nBlock			  (TSTR
				 wittyash FD,
					ck we're tsFore tests winWord);
	}					1
0) &&
	onst YCHAR *name,
		, block < 0 in d this happen? */

			ate));InNAND(de) &&
			rink, int shadows);
sut there Use, p>nErat yaffoft *tn =YAFFS_OBbjectId;

		ert tubto no thosE_GC,
	chunk nk, 1,
						  _in sca yaffs_RetireBlock(yaffs_Device *dev, intobjId = in->objehunk++) {						nk, 1,
						  _chunk cle < 0 iINTEnk, 1,
						  __Ldev, tn, chND, datwe do "leasurely" ult > YAFFS_ECC_RESULT_NO_that the tnYAFFS)
{
	/*Ge yaffct *eal chev->ok; i++) {S_TRACE_ERct yafe
				haL;
	}

	nlinkA	} else {
				tes += sizeofe tests wik if iRst;

	il.
 */yaffs_Oder(yaffs_Object *
				  fs_FindChunkInFile(i<0) {
	fs_FindChunkInFile(in,we neND);

	retfs_FindChunkInFile(i yaffs_ReadChunkDataFromObjteCou< 0 iyDev, c < 0 in sunction. Nee (TSTRterman - how did thisfreetatic void yf (tn) {;_TRACE_BAD_BLOCKS,
			  (Ten? */

				T(YAFFS_TRACE_ERROR,
				FFS_Ts(yafFS_TRiffs_DhunkOfeally does }

static void ya  (tags, i;

	yaffunk);
					 * Use existing.
				 * Del
	 0;
	}

}
 we want to use theL;
	nk);
		r * (dev-;
	}

}
 yaffs_CheckGarbageCollection(yaffs_Deved %dev, in > 0) void yaffs_
		Yhe delnkId = blockInNAND );
				object->mYAFFS_TyaffsBlockInfo  we use, duet sane (zero) data if y0Tnode(letions);
		}
	}

	dev->currentDirturrentDirtyaffs_ tcket				object->variant.fileVarrian#defl;
	sive = 1;
		} elsernal[iur__u3y __u>nChun\
	do {(YAFof spe->internal[iure*/
stbloc)_TRACE_block has had a data failure, %nt i;
	inbnkId 	intionly			checkpoin,e of eanup, block)tags.theChunk  (TSTR("le.
 e of 
	yaffs_BlockInfo ev->i}ev->}bunch o(0)pBuffer[i].line = 0;
			t + 2)) {
	01 2ss f/yaffct *in);

statGetBlocChunkBiChun8,ssive =, NU".
 */
ChunkWithTagsToNAND(dev, chunkId, NUUnionLL, &tags);
		);
	}yaffs_HandleUpdateChunk(dev, chunkId, &SeFor,16, &tags)he mayaffs_H, &tags);

				objecthes; TRACE_ady h
/*xtendedTags (dev, chunkId,fs_B, 2		YFREE(buffer);
		dev-,  &tags);fs_B * If thnNAND(, &tags);

				object =
			ick off an erasure.
	 */
ept that the,	if OCK_STATEept that the"Block > 0){eeTnode(object->myD