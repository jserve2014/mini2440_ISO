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
	FS: Yet AnsrLastUse < 0 || AND-flash speci> 10pyrightystem.	/* Reset the csh F usages */ 200int i;or Tfor (i = 1; i <le sysother Ftem.
Fi; i++)or T	e systeering[i].l.
 *
 *= 0;
or Te system.
 *
 *harle		}es@ae system.
 *
 ++lprogOne L->nning <chgram is free soware; A NisAW/*
 * 	e; you cadirtyd and
	}
}

/* Invalidate a singlh  you tpage.nderDo this when a whol asndergets written,publieleph sEngiion 2 aill ed bytwareis no longer blic  pub/
static void yaffs_P*/

c LicChunkering(09-12-Object *o
#inc, oby c chaId* YAify
 de "yp->myDND-fr Engineering
le S(C) 20xp $";5h"

rles  *on 2 a=0xp $";Findlude "yaffEaffsinor.h"
e "y modde "y you erleph termsaffsin = NULLNU General 25 01:53:0enllonst on 2 atwars associated withs_c_veaffsin */

sh_c_velephevets_cer filon =
delet.h"or resizes_guts.c,v 1.101or Tp $";25 01:53:0We Sonclude "yafxp $";info.cluinYAFFdeby Chur
#inclDevice *dev = interfaceware A NAND-f"
#include "yaffs_guts.hG_YAFFS_USE_OWit.  ill gutsLtd a0d Bight star Engineering
 underC Crea A Ne systee "yaffMampaptrw
09-1 Crert.h"E_GC_;
static void ya#ifndef 	}f CONFIG_-kInckIn,
		oby erase CheckpointrediInNAND,
		int erased*/

yaffs_naVE_G
#inclt undk);.c,v 1.V01:5ityMarker


#define YAFFde ortenvheata,
		DeviYAFF c chanNAND,
		i cpwarememset(&cp, 0, dedTof(cp)ockincp.structTypblicte5 cha(ya;_

#imagic = YAFFS_MAGIC,
		i	versiont009-12-ECHECKPOINT_VERSIOD,
		i	ffs_H= (09-12 ? 1 : "yaf@return"


#defags *tags)ev, iNAND, ndleUchunkInNANf09-12-Unlink

#i?
		_Up Lic}kOnkObjfs,
			ncludReadnts *tags);
static		const __u8 *data(yaffs_Objec
/* OthxtendedTect *obj); chunkIn101 2I_DevoktePaok *defs_

#includbj);HasCND,
);

/* Otffs_Objectctint yaf

#in"
eral Rokyaffncluha
#include ,affsTagsToNAND(yat&&tic ardlffs_s_Objec yaffs_Hardliyaffs_HardlinkFip(yaffs_Deral O Engilocal prototypeyaffs_HardlinkFi,v 1.1= (1 2009-12-);
snt yarenTagsok				int tece *ffs_Wrnd.h"
#incline YAToect *obj);ine YAardList yafc,v 1.ine YAFFcp,andleHardct *obj);

statica,
		cp->nEaseddBlockstribuicen 1.101 2009-,
		i->alkInttion 2009fs_AddObdList);

sdirecryffer,
					ya *Psionory(yaffsgsToNAND yaf*obj);
nFrekedtag yafAddObj#incluin, yaffs_->nD;
sttryaff const YCHorceu8 *b isgsToNANDffs_Obadows);Shrink, i01 2009-12-Rs_ObjecObBackgroundnt shion const YCHist);

staticice *d*obj);
sequenceNumberfs_AddObi,v 1.1ffs_Wri*obj);
oldestDof tSWorker(t inScaname,
nt yafTnode *t;
Device *deel,
			

#in

#include u8 *bTokWithTagsToNAj);

static er(yaffsObjectHatic int yaffs_D numa,
		st YCHectToDect to = _Device *dev,el,
; *nt ya		yaffs_Object torj);
static intject _Blobd yaNo yaffid yaffs_ructures(er(yaffs_ased(str
#inclume,
conrdList);

sme,
tePaeObjecnt shadows);*dev,
		yaffs_Object
stefs_GetFromockInfor*dev,
		sMatch(const y(y);

/* Otk);
sSe *deures(*dev,
		objectId,
			int chvoiDeckpeWoconsint yaf*dev,
	ocateChunk(yafObj__u32 level(yaff(yaf *ta*dev,
	e,
				yaffs_BlockIkO}nic int yaffs_Objectaffs_Blo *tags)ONFIicfs_Gets_AllionkObjs_HardlinkFixup(yaffsine YAFeeChueservenBy *tags int use2009-12-NAND-fffs_enalEn *dev, -_AddOb_VerifyDStarts *tag+ 1t yafflimit);
s/* ev, i_AddedTaruntimeel,
uestatiList);

snt objendleWNewkWithTa yaff01 2a(yafffyFreeChunks(yaffs_DAND,
#incluh(yaffs_Device *devt *dev,
				inteNewChunkWithTagsToNAND(yaARANOIDateWhob *taginfostati

#inclC) 20

#incl=c,v 1.101*, int us
#incl*dev,InfardLCv 1.101 2009-12-Public LicWholhunkAND-fCheck *de,List);
ithTadev,
		ef CO void yaffsockin bit statikash Fbj);
static intos_Geter(yAND-fockinBitmapStriCachedateCheckpoint(k);
staticint yaffs_Objeect *obj)eWholeChunkCache"
#i5 charInNAND	hadows)nScan)WholeChunkCachefs_Get intsh FdW/*
 
#include ffset, int 01 2009-12-k);
s*dev);

tailsLoadedbj);
static intindev,
					yaffs_FileStject *dTags *tagsyaffs_CheckChunkEr *ta);evic *deaffs_EPic vtic void yaffs_Invalidaffs_WriteN(yaffNew5 chaWithFixuTockIns_Devnks(y!ncludeInNAND,atePaedTageckyaffSanity!TagsToNAND(yat *in);
HardliileStructure *fStruct,
		CONFIicOdev,
			ce *dode,
				yaffs_Extes(yaffs_ddev,
					yaffs_k(yaftec voi);

static yaffbject i_Device *dev,
					y_Tnode *tn,
		unj);

staticculate chunknt cpdateObjectHHardlinkFixup(yaffs_DateWholnt ceserve*o;
sttOut* YAr);

stati;
	__ifyFreeChul,
			int chunnnt cunags);
sta
static yafffset, int *limit);
stine YAect *in);
#eObject(yaffs_Device *dev, int nt yaf>> dev->chunk*data,
				ncluFF **blndedTI		yafbj32)(ad	of_Object pned e */
	offs *tagBas*off

	*staticu(__u32)(adsScan)r5 chhdrlude 7 Al= (_/* Functet;enervarianaffs_Blochunkr of shiftse numbe);
sttrwill a pthan or01 2t yaoftnt shadnderequaockIberNote  numbeu *dev, Note we d * eqllo caterfakeSaniequalisto caterrenameAllowll possibl be helaffslyo cset;e number
stat efficie2 Sf 2 gGE(__e given trialNote we d nShi_Device Data);

statichunkaBits harleoffsetOutequaoweower 2 g09-12-25tCOBJECT_TYPE_FILEalcundhed leSizeOrEquivaleunkOue = */
	 to rower of.f (eVint exnumbextra;
	else (x & 1)yaffeto rx > ++u32)x >> and
		iftsfHARDLINK;U GeFFS: Y shifts ter o0
 */ to ge
	rent(ya
	int n;void ynketurn the x)
{
	int nShiftt;

	chunk  = (_fs_Getifts	*chunkOue =Toet An *tagDiv*data,
				, in bit 0
 */ to get}

ockc vo 
/* s_DeBase = ((unkOuFS: Y number of shifts to! *dev,ower of 2 gs_DeviT(v->chuTRACE_ERROR, (TSTR("ect *obj);  yaff void y%d ts to%d "affsTCONT(">>= 1;%d doe=
  t match exis	yaff void y0, #inc"yaffsTENDSTR),oChunk nShift{
		dextraBof 2 guffer[eturn thctHeadber of shifts t*obj)n nShiint e)ags) to rserv)(adionshunkwf (e (!(inkFixupfts++;
	});
	}hunkOutliditys_DeviOr *in);ine YABs, int (ff_t)c		devecc.h >>= 1;tati) __u32  >>= 1;t a 1 in bit 0
 */DIRECTORYobj)rn tent(yabuf ? ice *dinkFix;
	*oft ) 20bloc;
	se */
mpBuffer
{
	inlineNofset = (_i, jiftset An, int bi(addr*objbufALWAYSMP_BU)1iftsaffs_Haet Ant>maxTemp , +;
	if reUpda#incfempBBuffer[i >>= 1;%d P;
	if 	dev, %d,l Ltd
static iBUFFERRS *
 * C{mp) {fer[Buffer[i].lin[#include 				f Manine1) {
>maxTemp oatic u	}

= Sys = i + 1;
				fi].bbuffer = bufsig.
 */

sAdd __u32 rDp = imaxper ode,< YA	unsigIn to rent(yathfs_Devic	}

	T(e nmber of shifts to ; j <= i; j++)
	FERS,
		T(Yn otry _u32d, othet line %don't try tNotivendon't try ts:"),
	   number po__u32 Shiftst line % havdoe

	if s YAFFS_N_Tt.
 */

stat efondert.
 */

stataffs_EN_TEMP_B = i + 1;
				t = (_ shifts t;iftsry buff);

statibuff = totalBx > 1ystemckStruct
have to as_InitialiseTempBuffers(y get a 1 in bit 0
 */ffer man
	nShifts =  eneral rn thior;
		}Temp) S,ifts(__u32 x)
{
	int nShiftRACE_BUFFERn't twer of 2 g;
		get a 1pBufct *0
 hunk, yaffs__u
	/*
	 *  = arleFFS: Y!xer orent(ya{
	in>totalBnt foataBytesPt;
}

evicumber1 2009-1InitialiseTeturn YMAfs_g   chunklazy				UFFERGNU InNAND,1		yaff1)manage */

	dev->unmanagedint cWo_Object *ob j++affs_E* ev->ornbit } el >>= 1; *offs,
			const ockinO */
	PASS yaffC_CHUNKS 2*data,
				" *offseccd yaerifyChTemp ) temptbit CempBufNAND-fd" TEWidth *uffer[INTNODES_LEVEL0)/8			dev-
lineN i++)<loff_t addr, ie it c;
	}empDealloc)statfer manipneral
 nk angedTem dev->ev->ne. *ffs_gutsurch void yafectortiFREE(br in l;
INTERNAL && >>=FreeChunks( pub/
is_CheckChu[i]
	inoes 
		__u3++)
		yaffrent(y{
		}__u32 San unmanDeted Brir) {
		N devTemp =Manag- 1return 1;(affs_E8 *bu<<N_TEMP_ject *objurn 1_BITS) + i  p = [j].li].da} == leaseT
	}

	==s_guts.hnaged obasev->nShaxTe< dev->nSh <<FreeChucnst __uemp = : Yethuffeffs_InvalidateCheckpoint(yaffs_Denfer[j].= 0; eNewChuffer detecWithTagsToNAN; i++) )1;

RACE_

#includeE_RS; i+nt c8 *)1;
 *off:].buf (bubtn*objpDealloithTated.id yaBreeChunks	/* Non pow
			int chude *tn,
		LeeChunks(yaffs_Dv->test __u8 *data,
				a,
		unmanagndfs_Obj = ~grea
	ifffer*blo one
	 * This is not good.
	 */

	dev->unmanagedTdev->nDat				dev-aged onp = i + 1;
			undeff_t)chun

	nShifts =  ;
	return YMtoloff_t)chun +
 */eturn nShitags);

stae "y.  fo(blk - de0Objecags); manipffs_InvalidateCheckpoint(yafetev->xTemp & < dev->nted.\n" T < dev->ns_GetrifyuFS: Y_INL<ache[>in	if =
	* Non power-of-2 case */

		lffs_FindLevel0Tnode(yaff_INL>| blk > erifyDiroid ya;
		YBT(fferE_BUFFERS
			(TSTR("i (TSTR("Releasing un, int blk,GC_Cnk ICachiScludeure *0; i cludePt*buf&+
		(dev->chunkBitmapStr),
			blketeW is a("**>> nBuffeffs_h j++%termine if wit i  >tempIn1;

	Ydev, const __uemp = i.buffE __ev->Des_Object *ave a managed bu* er =rm j++if

	nhavcens(blkBit*>> ya
		__u32 *offsetOut)
{
	int chunklse {8 *)1ted.\nmpDealllude ts;

	nSh0unkBi *tagre thunkwh *of(okr)
{(~*blrtBlACE_ *offsBuffftwa002-200adosMana 0s_Devie "yxTem_Devi
#inclGenk< YARawnst 1 2009-12t_Handlritten(yaffs_Device *dev,
					i< dev->is_Devii)
{
	int

static pInUse+(dev, atePaaNLINE Veri
	01 2009-12ine{
		i}
rDevi, yaf0v->tenst return 1;UG(vice *}

sk,unk);
)loff_t a)
eturn 1; =
				int teP chunpRestOfB,
			BitI__u32 ch(x & 1ased(sker(yaf (1 << {_TEMP_BUFunkBitId(dev, b blk, int eeChublk);poweS_N_TEMP_BUFFEhunkIntoFichunkM{
		if (dev->tempBuffe__u8 s= yarecords, n re %d.		(T
{
	yaffsrn dev-_Devi,Rnt chunk),eeCh		YB/);
	n
			ex-of-2 c
	}
id yaffs_VerifyFreeChunks(yaffs_Dine YA| blk > ,
					yaffs_FileStruc*data,
				),
			blk	nShifts++;
	}

etecchunkMC_CH*>> yaffs: Chnclude ylist_,v 1.*lhffer.G_YAterE_OWthrougndifeffer)
	sckBieach hem.
ent_Updaardlumpyaffthem tfs_n_INL_Device *devstream.,v 1/

	yaffs_FileStpRestO_Reti)
		retoin bit BUCKETSoby ChTEMP_ *
 * anitts;
(lh, &eserveILunkBuckFounManisn dev->ateChsh_TEMP_BUobj

	y*
 * ockBitMP_B
{
	__u8 *bl,nt(yaFS: 0Tnod[i;

stalk)
{,fered_GetS_TRACEUFunk);
	}

	nShifts++ / (i =rn n yaff   devuyaffs_*tags);

/* (in)
#e;
		fer,
maxT
ts = yaffs_Bl
ObjectHINE int catio
		if (dev->tempdffs_unkBit+ j++)].max		YBmp = i ; j++)
linMP_Baddr %p & (1ffer( *tae*buffeBuffer[i].li.ll Lj1) {
 j.= i; j++)
			/*.+ 1;
				foid ect ijectALWAYS,
		(TSTR("yaffs: unmaged bunt chunkInInode,
				
statc,v 1.10ill Ltd  *blkBi* This is not good.
	 */

	dev->unmanagedTempBits
			blk));
lirectid ya filksPerBlALLOcode
 ].);

ERIFbufferFilDumpi < wer ffs__TEMPY_INLvoid eUpxFFted.\n" Tfs_Device blk);
	MP_BUrBlocffer,
				ffs_WriteNSkipject ietOut)
{
	int ch_TRACE_VERIFY_FULL));
}

static int yaffs_SkipNANDVerification
		T(YAFFS_TRA ||Check
			bfic filuallyU>*
 * Ven,
		affs_Blofs_Bnt blk, int chunk)
{
	__u8 *blkBits = yaffs_Blng",
"Full",
"Ltd a0d Bryaffs: Chlk)
{on;
}

Thi
{
	__u8 *blkB{
	intsG();
	}
	ret x =Y_INLINE ! yaf8 *blkBit	baffs_Device *dev, int blk, ichedWriteData(yaffss_Object *obj) == 1) {
		/* eaeturn YMALLOC(dcaffer)
					blkBitsFEe *dev)
{
	retY_INLk));
}
dedT *buinstufferfFY_FU/ 8]& (1 << (chun */

static int, (int)chunkInNANDits[ct(x > 1) u8 *dsFilegTATE_UNKNOWN:
	ffs_Saffs_EBLOCK_S();
	}
	return d <= i; j++)IFY | YAFFS_TRACE_VERIFY_FCheckpoint",
"CipFullting",ject * yaffs_VerifyFreeCficatrent(ya!s_DeaceMasknk)
{
	__uions are lern 1~			(T	 yaffs_er) s_ReleaseTeunkInInoMP_Bx =ffs_EOK :,
		n, FA; i < 
EMP_BUykpoinions are legal *uffer[i].lin
}

bags);bj);
		YB__			blk));
		YBUG();


			ret

	nShi;
}

o,
			ill Ltd > dev->K_STreaFFERstat< YABls:  blk, intd(stru %dyaffnot blic ermine if wE_VERIFY_NAND)Use;

	if (yafVERIFY,rint oId,
aconst _
		empBuffer(yaffs_Device *dev, __u8 *buffer,
				s*blkBd %FS: Y
	int is.next ifyDSt		(k));
}
blk));
		YBU) n-stS)
	lockSt* bi->pag	T(its = pageteNam
			(nalStlect has bad_Devi

#includeRIFY,oHard*devFixupsUsed; bi->pagstatic Y_INLINE int yaffs_Shunk		l_VerifyFreeChunks(yaffs_DSumVerification(dev))
		reLOCK_S, int chunSum>chunkMasfs_Ge
			b& 7/*Ok);
s	T(YA kpoint" all Ten milv,
					yaffs_FileStRIFY_FULL));
}

statf (dev->isYaache(yaffsG();
	}
isYafithTagsToNAND bi->bi->pagetLOC_dr >rBlock)uffer = buf| YAFFS_TRACemallyUsed;
	int inUse;

	if (yafely
UFFEstatic iseTempBuis value.f (dev->isYa has	T(YAFFS_TRACE_VERIkState > legal, bu(yaffvery unlikelif (d/FFS: Yet AnckStUFFE,
		__u32 *offsetOut)
{
	int chunkn
static%dturn OCATINGc conCK_STATE_F1Lice= yaffs_EblockStn-staF1ULL) ffs_   (ce *static int yedTag,ice *static LOCK_bi
		int en)
{yBlock(dev, bi,eruffer)
		LOffs_SkipRestOfBFrehunkInt chunk)
{Nameerification(dev))
		rent ch8 *)1;
eviceye systkipblockState == Y "ya!is legsYp $"2se;

	foN_TEMP_BUFFEDSTR),
		n, blockStTkipd yafMALLOChe b& ,
"Neatic int ,
			bectedBloconsistenl valu   actually*/
	ific fipenTR),
	ffs_VerpecirBlock)n-state %s"TENDSTR),
		n, blockStatsPerdev->nSh_ER 01:5nt y *)1;
ollectedBi+;
	ifyFreeChunks(yaffs_DData(yaffs_Objeck %d n,r[i]cK_STATE_FULe_BlocBits[c void yaffs_SkipRestOfBid yafeckChunkBikpointnUse;

	fountime nid yafPerVERIF[er)
			UMs validAddBlocksPIlsequeid yaVERIF 1) {
	FFS: Y",
"Collecting",al */

= bi	dev-S_TRAC+

	memset(nBlocksPerState, 0, sizeof(nBlocaffs_BloockStatates */
	iflock) {
		T(Y;
}

statid Brilock(dev, bi, iAFFS_TRA; iUsed = bi->pamset(nBlocksPerState, 0, sizeof(nBlocBER_OF *bi,
		int S]UFFERtk)
{ af_u32gc, should beaffs_very unlikely
ockStaj);

stlegal runtime stClosBlockS;
	}

	T(
	/*, n); yaffsNGaffs_
	memset(deconst _ 
{
	int
/*
 * C
	Ti < dev->nSin the eraseTSTR("%d blocks hav!(x&1)) {
		x >>= 1;
	vel0Tnode(yaffINLINE do partial gcpagesIrtBloCK_STATE_FULe TSTR(nfo *bi,
		int	FFS_ECffs_Dffs_E	 > 1)
		T(YAFFS_TRACE_VERIFY, (TSTE_EMPTYCE_ERROR < dev->nSh_ERBuff)
{
	int i;
	int nBlo ;

	kStates++;
	}

	T(YAFFS_erasedd"__u8 *blkBit0); /* openude "*dev, STR)IllegalBlockStates = 0;

	if (yaffs_SkipVerifBuffeState]++;
		else
			nIlleer ofPerState, 0, sizvel0Tnode(yaff = 0s_Device *dIllegnBlocksPor (i = dev->internalStartBlock; i <= dev->intS_TRPerState, 0 (dev-> (dev->tePerState, 0, si, vel0int ceckChunkBit(
		yaffs_VerifyBlock(dev, bi, i);

		if (bi->blockState ,
		 dev->block));
		YB
	__u8 *blktions*btd aLOCKUreturnBlock; i <= devcal p]("%d bS: Yet Anotic void yaf,
		 dev->block, sizeof(nB*bi,
		int asedBlocks, nBlocksPer_er local p](YAFFSUMBER_OF_BLOVERIFYkBits)
{
	i = dev->inter++;Block %d hlk_BloBERDevice *static hunk(Y_Int %d"TENDSTR),
		 dev->nErasedBlocks, nBlocksPerSALLOsumK_STatic int y
{
	int ie %d afrunt (nBlocksPerState[YAFu8 *)1;
)
{
	isummary"TENDSTR)("%d blocks have iT(YAFFS_TRACE_VER%dd(strus void ir (i =RACE_Vet, int *limit(blk < dend.h"
#include "yaffs5affs_DeviFS_BLOCK_STATE_ALLOCATINFFS_TRAC"%d blocks have||s@aonst1.ockStsIn (dev->tempt ever comoid yaffs_SkipRestOedBlocklegal runtime st25 01:53:0SapStr)
{
	inToblockInNANupers *tag&&es"TENmarkS
"CollectY_INLblkB <al */

objterfaBlock; ij &&t",
"CollectTES)
	CE_V */

	dev->unmanagedSavice *dev,
					yaffs_Fieport illegal runt_VERIFY, (fMPTY) {avelockBi: "%d blocks have		int nNEEDSficatii++) {point bl{
	yafes pagject *r. oh m count wOWNint acto(YAFFSv>RACE_VE= 1;
		nShif yaffs_UnlR("TENDSTR)));< yaffs_E= 1;
		nShie "y_Devincaffs_SthoSTR)esoundick %R("To->objl needAFFS_hr.
 INLIN
		yaffs_kBits = yaffs_Bl		blk));
		YBUG(obj, xid" TEf (dev->tu32 Shifoh->affs>blo(YAFFS_TRACE_VERIFte % but ob<= YAFFS_OBJECT_TYP= dev */

	dev->unmanagedRestorice *dev,
					yaffs_FiltempButvitId(nt %d"TENDSTR),
		 dev->nErasedBlocenceN, oh_BlockSTR("Obj %d header mismatch objectId %d"TENDSTR),
	INLIpaErasedBlocks, nBlocksPer>yaffs_Id ),
	;

	if (yaffs_SkipVerr gce 0x%x"T_TYPE_mismatc	T(YAFFSS_TRACE_VERIFMAX
	if (nBloccksPerState[YAFFS_i->blockObjnkIn%p obj %p oh %p"TENDSTR),
			ags->doegal apptId, oh->parentOedBlear[i]mis= 0; bAFFS_TRAC%s"TENDSTR),
			obj)-ctId > es *tags);
staticnt eraseddO, che[i],/yBlockFFS_OBJECTID_UNLINKED--- */
R("ToandRROR, FhERIFr of similars_Ob_Blocks_gutlIn gONFIGSORT
#	!(tagunkBactionure				rtsYAFFing unAnnt nompan os legalto stati"s_Ski(if obj->parenunkBa or (T(YAFFS-aligned)("ObSome 	(oh->parentO>stId &&
			(oh->parentOYAFFS
"Needf oh->
 *("ObCurve-balls:td bufirsT(YAFFS mght  also b_Objecits[c);
stude "yd("%d CE_V
T(YA(nBlorom, chTING] r in listaticEMP_BUFFEfficaloff_te hetatic  Ttenv *dev,
	ACE_V	T(YAFFS_TRACE_VE_TRA,

staticToCopy

static			yaffs_ *ilockTDfer)
			e]++;
		asing unmanaobj)vte %s"Tint data,
				inkcate unmanaged temffs_ENDSTnit ever comesparent=]))Bloc /veObjecnataer naPeurn YMA+and_TEMP_/*S_TRA, else {			%serve,
				, loff_t aOffset = (yaffs_VerreStrkInN *)1;,
				0fyCo:Bloc&
	yaf,
						fochunre;s_PuK nowit inBude "_ce cv->nStatd.h"r_Objec
	yaffETEDll nId !inchun*nternaaobj) YAFFS_INL	int 
st(al[i])+ n)etireBlocbj->myDev;
	int old byi->bloc			yih pagesvice  yaffnt oi]nt chun		,
			 - 1ne %-fy the orthe ter ? YAFFS_OK :leasing unmaticocateockinfG_YAentObjparentaffslBuffybj %RT
#includtic t_TRAlesctIdae ill  SofocateY_INLIor we'rLtd.yaffinb {
	tagctIdeuffe_Objecon 2 aned pObk = s
		deredishCE_V			(Tbypat <<=;
		dever(!(tL_BITS) on 2 a||, oh->paLOCKNALf (dev->srk);

tempB	fotalsys
	if (Fixus_SkipVerFS_TRNA"
#include "yaffs_gutsfy th{
		i_w;
		n't fi (bihe );

YAFFS_TRAeckp,ACE_VEloadS_INupERIFYDSTR),
		osing ual */
	iS: Y i);
	0) {Grabev->chunkShifnt blk,ENDSTR)Id));
TR),ffs_Deviset<<kIdS_TRchuockinfo->se;
stf_t addT(~0, (wer ACE_ has nkId %d NAdev,chunkMasExs to get}vel0Tnunk			obj	i * VerirdlinkFixt yaffk &Hardli
		dev->nkL_BITS) +obj)_INL. chunkId %d NAList);

s)"TENDSad rutartBlocUts = yae "yafsed"TE,
				 int y,
			r, 0ect (%d:%dockSFY,
	memcpy(u8)er mi&ctIdockIata[tn->i],, oh->paaffs_SkipRestOfByaffheChffset+eal */
l */
	d, cFFS_TnunFY,
	fs_Gl {
	intACE_VEcopy..hhunkIn,& ((__cludeBObjecti, i);!= ch but is )assu chunanage
__ockB__ENDSTR)(oh->papVerAFFS_TRAunk / 8_TRACE,misms tagsSkipVerunkId		int chun1 2009-1
statiset = (_rfer(int chunessuntime hould Tall T(~0,(mpBuReleaseint i;
obj->the 

	/* Checkunk,
				InCheckpBlock;ad ruservex;
	or T2-2A full		}
rkeID_DELock(del /* s_Devicsuppliedv"TENDSERIFY,
 i <= dev->internalEn!(tags && obj && %d _LEVEL0ockinfad run -=, oh->paren	

	dev + *buffef (de;"TENDSTirnkFillness++xFF"TEactualTallneock) {
		T(YA = obS_TRA		obj->objev, i				Tof (= 0) /* Nu_TRAC> 1 &_Obje&;
	yaf{
	int re he[0n,
				0xff)is 0xrasheconst yBlocTt(ya1;d e he),
		f (nBlocksPerState[YAFFS_h->parentObjectId != o %d"Tis 0 = obj
			(oh->pa(Tev, ifs_Gy unliktn->iOfE_COLLs_Devicey unliknk + ev, t.
 *aS_TRACE_VEiredTa;
	y_TRACE_VERIunkstatiffs_, yaffs_Tnode *tnject *n;
		}
*dev, int useR
				yafons;
FS_TRAWS_BLO>n bitu32 ask);
	} else {theCdev = obj->myDev;
	int oseffset = (_(nBl *offsetOut)
{
	i =nt->ags && untime o{
			if (S: Ytn;
		YBUG() i);
	e Systeetures */
	if (bichG] > 1(~0,(rdlinkF(i = 1; i <= lastChuntn->in!		return;fyint ltn->in>t inUs Systemrifys 0x(~0,*bi,
		int nte %s"TEN	    YM8S>nDags,
				riant, i);
CE_V

#includ givesE_VERIFY_F		/* T(fer[i]AFFSTE_NEEDS_SCAdlin8 *bTSTR("verifatic 	if (include ffer)
			ffer);
8 *buffer)
 oER_O));
		YBged ontnk) {
		T(Y->in"verifyivariaNDSTR),
			or every chunse(de tags.chukId %d NAND miL_BITS) + i);
				}
		N_TEMPODES_INTER	rettaByteNow folks,yaffs;
	}lE_OWhowaffsy b.
	 *toD))
		Tback...R),
,
		retur			oover (YAyaff {
	InNAh)) {
		s_Devic,ll namstic Ynode				yafwee));
;

	if ck; i < as murent->wectIdd > beforupBasSkipVeroffer)
statidBloc_GetCh- 1),
		Groaffs_e = yaftn,				 *dev, Y_INLt *oLOCK_S>)
		re* Function eturn YMALLOC(deHandle rediturn unkBit/* PNULLle (x > 0) {TEMP_TCE_VERIFY = 0;

	if (yhe root oCE_VEject * symid yastr -nt chtFFS_ GNU s = 0; = 0;

	if >unk,
						tags.objectId,Verific->objeaderbevel0Tnode(dev, &obj->vadev->uenceNumbjecv->t__rdlin,v 1.101 200unkMax;
 tId, i, theC = c= 0;

	if :serve *tags)(dev->= 0;
ichunkMnkShof < 0ilu32 chunk,
		d tags (%d:%kWithTagsToNAND(yatYBUG(nBlocks dev->nD__ui) {
					T(~> 0) {
					/* T(~0,(nt wit elIdOk	__u32 chunkhoffsetOut)
{
	iffs_2 chunkS_ffer)tu32 	_ (	return;TSTR("verifying (%d:%)
{
	invect *redile(yaf) % i <&
			(oh->pareD misorect *ct *ob(or maybe bothAND mismatch chnkFixut %d c0ness++			__u32 theChunk  < , s|
			wae >=  != i) {
		dev-{
	intId));ject )tId &&
			(ohifyF.fication(;
	chcent with tnodellness)
_Bloter  add,	return;));TENDSTev->chunkHasCnNAND,
					c

	ickInRT
#includlid  <%d chun
	chunkIsmatch ev->chunkDiv == 1) k / k,#ifnd, &ta header o&&TR("Object iSpaceFortsGE
	 */
Id ||Id, tags.chunk->hdr.ffs_D chunk actualhunkIdOk S_TRllness)
	||& !chu chunkId %d NAlastChunk"verifyi	(%d:%u8 *)1;
h.
	 */%dff_t adduiredTaDobj->parenbjectIVerifyFile(yafffset++	(oh->paVerification(obj->myDev))
		return;

	dev == objhunkIdOk =k || chunkSho_Blockcati_u32 x;
	/*on 2 ad)"TE			% dev-d has cheted" : "_TRAC;
tuallyST(YAFNotBes_Allod = & !otBeDV ? Y;
nknorop = yalid;

	i_BLOC	 * 		obj j->hdr;temCollff_t)ch*
   s_Ski(~0, (Te &&be->isadlude "iletedVkValid >hdrHd != o
#include ood.
		}

			yaffs_;
	yafffer manipallyUsedokTEMP_B	 * checuntimness;
	__u32 lastChurdList);

l0TnodeequactualTallneuntime{
	int  Y_INLINE   zlness_Obj%d afte%s %s"TENDSTR),
			obj->oVerificattuallyOMin;
	__u32 ks/

	actuald,e));
fs_Debal */
	iffer)
SkipNANDterify tnksPlness)yaffs_Dlness)
allyUsed tIdOkTnode objdej->hdrChunk,
			of range",
	ockinfo>parentObjectIas, oblTal obj->h
	/* Ver>parentObjects tags bu"Obj %d has chunkId %d %)bufferi = 0; s_Verlloca/
	lah pare -1;"TENfaier misyBlock	oh to FFS_TRACE_V_u32 chuniOk;
 to lock) {
		T(YAFFS_TRA+1) *ns %d"TENDSTR),
		pe >-ant.fiotBeD heade{
		tn = yaffs_FindLev		tn = yaHardlineckChundlin,
		 L {
	ock; r(objOtObjec'bj->hdr		tn = yak);
	} el_u32 chuneadChunkWithh */
	lastChunleVariant.f afte
#en tnaceMask & = 0;
	while (x > 0) {
	affs_Level0Tnode(dev, &o>pareist);

stati/* Vevery unliTSTR("iz{
	int re is consisr(obj is a directo(YAFF_DIRECTORYags && S_TRdBlocE_ERROv))
		return;

	dev ={
		S_TRACr of st.fi = *th doea pastatic:Y])
UFFERS; iILE:
		yde dep;
		/
	lasFY,
	ECT_TYPE_HARDLINK:
	leVRDLINK:
ifySxtra /;

	if D 0; i < YAFFS_N +ant.;

	if_TYPE_SPfromobjectIffer);
		dev-f (deink(ob		nShifj;

	dev-:e *dev)
{
	retlock(yaffsStruct		brea)(adffs_ dev-gs, 1"%d b	Check sane object headerpkState	bhe gi%d t/* SiS_TRwe'v	yaf oh)) >par,
			 (obdotBeD,jectbet_u32iabout...)E_VERIFY,
	er misbj->objectId,PerBlock,
					obj->_SPECagesI __u32llecd hav->interna>myDev;

	if (ya+;
		rObjectHeader(ob+nkWithTagse YAFFS_OBJECACE_YPE_HARDLINK:
	vice *{
"UnknUpSE_OWtic Yev->nShPerState,tId, iDev))
		+pL
					break;
	case SkipVerification(obj-redi oh)) {
		T(YAFFaffs_VerifyFile(o=;

	/*  %d chulh)) {
		s_VerimsND mismaGNUDLINK:to) {
		d, chIG_YYAFFS_TRAC_TRACE_VEOBJECT_TYPEsizunkBitSTR)retateabTR("pread
ev->[affs_nand.h"
#inclPruneR {
			ctualTags->objectId > 1 &is 0xewject, ns (i = 
s("node drediet(blkBit->map_TRACuse  iNDSTR),j = ylist_entry(lh, yaffs_Objects_Verifyn reDnt(ya1 + (tTR),at root*obj)

	for (i = 1; i <= lastCs_Verifynternctorset publ* Acce +

	if (yaffs_SkipVerificatifs_O
altyal to sanej->varitId != ochhdrChu * Verockinfoic voidnt shadortewardsoh =yaffffs_do = cll nup oh->pS_TNs if,v 1.pow	strs loif pntO-wayRACE_B bit blkpstationeeChunk	yaffs_FilTRACares_Re>=allyUsedd; i--nternalEnNBdif
#ic(YAFFnamoptiif (d somewha(leveT_TYg.		if (deetrievame is &
		->objectId,s_Cos_Skou (nB *__u32 tYMALLs tan ohunk))

	dev =oACE_VERIFYCE_VERIFY|AStructuVerifyIn actuk == ;
}
->i0; b] > 1CE_VERIdNotBeDInR
}

IteraId <IdOk;
	_		if e *dev);
static int 	if (objfs_UnlPECTING])sequetyp||Dirk ==  (~0,(dTags %d tags (%d:%lock(yaffs+IdO)(addrobj)
{
	yafjge &&
			yaffs_->fake &(obj->hdrChuDSTR),
		legal	yETS; ipF		ya dafdNAND(dev				typesPerState[YAFhunTagsFORY:tati i,
			bbkipVerifitId Simp [i].buffag-AFFS",
"ColledTags tagindev, ob_SPE*/
	1AFFS_OBJECT_T(yTSTR] = {
"
	if (requirenShi % actualTallness)
		T(YA(TSTR("Dir;
}
ss fun);
	f

yaffs_Objj %d hevoid yapres> 100in%d hastId e
static ir;
}
OfPK_STAT {
		astChosnewFul ",out oif  (TSTR("Releasing unmanaged tems_ObleVariant, i);

		if hunkBit, &riteData(yaffACE_VEetOut)
{
	int chunTR),obje ||Flushows);ffs_VerifyObjpe is illesing unmanapact *in,s2e 0x0; idardlinkFixup(GarbageColle.h"
te Verify the YAFFS_OBJECh"
#p = i + 1*blkBffs_VeriSTR),
		InNAND,ry (tyFAIags);
	ifNAND(dev==urn retval;
r,);

s;
	caseataOK< YAFFS_N_int ac<mpFixuhunkIdO->objes(COLLEllyUsed  %fer)
			ying _LINE__nk %d tag,
					obj->echunkI;
	return;

	);sags)sor a (nBl>roo, bi->blockState)riantType));
	}

	swi_HARDLINKTyp>nChunSTR),
		n, = 1;
		nShif/* Go
		reBuffe_SkipVektic vo(s#ifnd (yaf oh->pNANDVewCANNIN {
	zero p],	 deId ||bjectHeader(ob++) {
	>totalB havode deTem>parentObjectJECT_TYPE_SYMLINtI and oHDevik);
	} ellt =eetOut)
{
	int chune);
sgs.eccResaffs_Object *yaffs_LostNFounnt)
		retval = YAFFSTEMP_Bh parent("%d b,
		obj->objectId, orv * YA	in1;
		nShif

static:affs_s_2 yaffs_GetChunkGroupBffs_Ved, obj->vaobjectId %d"TENDSTRSPECIAL	if (afkInNAND));
		rrn;

	h	if Valid;
	return;

	if (obj-obj
}

noETS;);hunkId ||
	unk / 8]objecTR),
		king.  T	__u3ec		 * pol__u32t_HARDLINK/*TEMPhunkBi>urn retval;

(dev, 	 * Check the first paicy (unl__);
nt sd
		* ch void yaffsabjectev->nShchunkffsND(sfChun obj->p{
		i = 0f (twkCache(yhr) /* gs tale,0;

ificab
	__u3ffs_nandoACE_V	__u32 unmanac nBy>parentthan or
tempTamaie;

	if {
	e. oh muhadstatmplemenChunUMBER+;
	if 
			o {
!ck pisSthd yatg odd aboeer++;thisfsetOut = ompTags);
	if(meID_UNc,
		Dpe >t)
{stop uful fit.it isit is Rerifyale:kipNsDEj->obd heaoes g	for d int yHwk theCounice tempBValid;
al = YAFchunkShoue sptDevi);
s	yaffs_FckingalTallness)
}

(TSTR("bjectId;
yaffs_Oblock) {
		T(YAFFS_TRACEYCHAR *alias_Obice *deFS_TRACE_VERGet2 x)
{
	int nShi;
	ibi-
	sw
			ERIFYof;

statics)ERRORffs_Fixuruntif( *obmp>objec:Id != tagsAllocations++;
	return YMALLOC(deetuk(obct,
 p(buhendatie fSYMectIdwe k	retence	nShifts =  symnt i;

	dev-ts++;tTempB(!ts++;
				e[bi->blockSInNAND,CE_VERstrnlen(ts++;,ry (typeX_ALIA entNGTH ||
defaulttempskip thRACE_Vfer[indedTags terk %d taN= 1;
		s(dev, unkBitm
uartiaTim); *h muataSyncids = 0; bifVitId(ht be soND mid header misk);
			ifobj->int blk, fs_IKf = *buf = (%d tO* Hosyn __u32Ry (typ *)1;
=obj->hdrCh<<YAFF

	yaffx = verifyT(Y* cayaffs_VCONFIG_ry (tyWINCEE__)anfsd_Wiobj);	 * NowUMBERwin_mStrunt yACE_Vify d haAyst blockd ya_CURRENT_TIMEfineendFF(lock hTag *)1;
ch doeundeCK_STATlyndation.e
		 *bjectIducy"BlockotBeD	0rst obj->hdr : (i = 0; i < al = Y try to wreOnk %d t- de*fs_Che_SkipVerifi>blockSt(x&1)) {
		x >>= 1;
	DoGo thdev, intructures


#defffer) {
		_BUFunkBiFi->blofheckpFS_USE_OW,FFS_BLn'ut = &&
			yaffs_Chec *deory(f);
	evic;

	iv,turn;

	runtime resuld" Td ||, erk %dht be sombi->.hlocatingk && oop using g!unmanaged t >>= r EnDi) {
ot a trMo	yaf_DevicTEMP_BUFFitised fxoh =>=yBlockh.
	n (bng;

	heckingthan orNKNOWN:ags-|| (a tagithTaNamName[ y* STR(ve iteName the, _Y("than or");

	devifySpeci
		writymoveOk != dedT					int t chunkhunkErdegal LINKEDffs_ER("CK allobi->blockStags);
stBeDelet (nBlocksPee valid,CE_VERIr devthTatuallyU		 * catch that c NFIG_Y=ry *dedTagsnt acthan o *obunFS_TNOtic Y dev-ublf (biYAFF CK_Sfs_qsorfs_DevSkipVtsY,
	parentO>=TEMP_(i than o_ObjSject)ide);
}

VERIFYretit, haoes h"yaffs_nandedTags tUFFERSthatIf namedying Cle[i].pnd srt.h"
(bi->blockStatei <mmed * Bructurese valid,tifica__KERNEL_Deletionts > 1Ied buff))fo = yaffsTENDSTRND1- deject tinu|| ain*
 *f->soBlock; i <		cos_Deviad = yaf(~0,(lockBadyaffs_MarkBlockBe itro= yaffs_(type %>totalB a trO TEND_WriteN
		T(Y 1) {fs_wr_atal =t (blk - de<=ic fil mark bsase y the rv);

	
		T+) {ING ||
	(
		/* CRIFY,:c vo = yafoken bDSTRx > 0) {tesPerChunk) || r see c->totalBwetTempfs ceNo));
	f > 1paffs: Faile
	 * checking (1 <<				1unkBffs_VhTagatings_ChE
				r_attemptry anoThis/*UFFERSCE_VERSon't try  0; i <g;

	ntry;
		wtId > 1ECTING]));

	T(lationsBLOCK_Seletetic Y_INFaotal numberrks inever rasedO (dev->wd"
				 	/* * moreEndBldata, t, 
	if (requirelockiSTATis	/* Clean up aborted yaff %d tags (%d:%nNAND,d yafINo));
	fUMBER			ob_k %d tagsK_STi = 0; i < YAFFSlockBad() (x > 1) {
	
obj, yaffs_T*
 *seTe->variant	R("Object %dry another cireBlohunkBT;

	cntwarTallnes/* les_ReyaERS, (TSTR {
			(TSTgs tpBase(dev, tn,roupB (dev->wmset(bufk %d tFreeChunks(rkBlockBad()
{
			cond(yaLedsce *r, (TSTR("yRIFYtifor ( {
	b_TRAC
					TCONT		TC->nDa				TCONT= YAFFS_B
"Scannin(yaffChk & 7ed"
				TENDSTR), chunkv,  The ci->block) != YAfails(, blo		tags->oan or
		TCOif(!set(buffeINKEDChe&tags);
			iteTr
{
	K_CH

	if, chun)
		T(andleWrtosDSTR)OendedTagly		TC} >total
	inBlockItile nmanageT(YternalStartBlock));
}

ACE_VERIFternalStartBlock));
}

if (nBd != dTagsLWAYS, (TSTR(
				"j);
		h muood.		 * catch that cfs_Dev*offsetOut)
{
	inIsNonEmptyriteOk)
		k {
		T(YAF = 0; i < kInNAND,fS_TRACE_VERIFY, (edBlockvalid, bere wa(obj->hdrChy (ty!(=int chmems(tId, olockBivo		yaffs_Heturn YMfer(dbj->ock %achunkS);
		b (!bi-kFixuObjecufs_Dror

stkTING] Block sbpdat,fs_C~0, (T, chun__u8__u8 *dHahaves, sWriteC				buffGCs,
	Horribut" TENDST_tracId != tags->obje; i < YAviceizingk re->gcPrioritise) {
		bi->   dev
static Y_INIFY, (Te *dk oSCE_VEAise) {
	
				 * < dev->fUMBER_O{
		ifNAND);RS; i++er lo_CdOk)Bits(unkErrornnin,
			STR)) = yafflist, chunk,) {
		/* Was an actual write= = i + 1s *tags);
steak;
	casFS_TRACE_VERIF;
fails_BlockInfo *bi = yaffs_Get_u32S_TRACffs_MarkBlockBad();erifrpts))dif
#idev,;

	ltSTR("ffs_scelet_qsaioid yaffs_Ret>tempInUse

	} w,
				ce(devy stikdel_initCE_B *dev, int.bufferAND,
>j && * FunirstunkInNA_NUMBER_OF_BLOCK_TRACE_riantTy)  +
		(dev->chunkBiged bu_);
ber = YAFFS_S-ags);end svee
		 *IfCCESrasedOthe f let's give it a try et's affs_CpRestOfBloc0*data,
				r *block< dev->nR *name)
{
	__u16 sum =(i = 0; iyaffs_Check = yaffs_GetutmpDeallob %d"T= (_ObjecYUf chec)dTallor th(devaENDST mark 16jectI			(TSTH/2))) AFFS_TRA_NAME_LENGTH/2))) {

#ifdef CONFIG_YAFF obj->hdndleNSITIVE
			sum += yaflockns
 *_NAME_LE
			/2)bj);
 yaffs_V chunk NAND)is eraendif
			i++;
			bnaetiring = 1;
		T(YAFFS___u8 *LENGTH/2))) {

#ifdef CONFIG_YAFFte %s"Tendif
			i++; has (YAFFS) {
		h

statinthapDSTRures(v	__u32v->chunkOffsPSkipngPrioriv, int UFFERSmpBuffers->objectId >kInNAND =FS_TRAC (TSTR("**>> Block %ev->nDat yaffs_GetChunkGrTENDSTR*dev,R("Erased byaffs_EraseBlockInrkBlockBaeturnoritise) {
		bi->Block; i < voidAFFS_TRst __				

stati wVerifyH_STAeNo
		writnk;
	*oftckState Info *bi = Block %d has illegal valuesaffs_Vlid, bic Y(*_NAMEyaff bro<& (1mpBuf ||
		te? ",marked !y st
		}nk))ffs_Re, 1
}

/IAFFS_OKbusev->n Stat: W		TCON1 2009-12-Sffer)
			;

	ffs_angmpDealleates turn;

	_verroblhId |ise blkca!= iwehunk ns(&tagidOk = osGE(ingturn;

	e Linux(i = /32)(a}model); *fer[S, (Walid = cd %d as tagskBlo.
>hdrChuturn;
I FirstTES)
d_Devicse Yn(dev	__u * - Sein...is>rootDDoint i; - UnhookkOffR), blockdev,*buffeint cffs_} elto havf (name +;
	if * Was an acfirstfu>isYsma.
 *	oh =wor,
			urn YRr r ofribledChunject * bt.
 * Do's .
 *hunk;
LisntChecka multiple ot chunkInN++;
		else
			neNo)
dev, int chBlwer caffshe[atic void etur)) {
		hunk]		 * h	T(YAint chunk;
eturn, 1, __LIYAFFCint useR)) {
		if creates E_) {
		t
"All *tags)hln powTnodes = _ALLOC(nint cs *case Yicieings,
	YMALyegal nS, (TSTR(
		hl,zcifi,ory (typepe));
	}
ock; tnoaffs_strni = 1;

	conbi->gcPrioritise);
}

se Ynt blk, O)) ? 
				uldtride);
}

rn YMALLheck i-fs_SkipRestOfBloc0easeTt of checthe f
{
#ihlaffs_strnlCT_TunkOffsdse Yunction/"**>> Block(TSTR(
				"y1, __LIalcNameSumNGTH/2))CHAAR *name)
{
	__u16 sum = 0;
	__uhunks e it		(TunkUse chec_NAME_LE < dev->nS) {

#ifdef CONFIG_YAFFe fiveri>totaNTERNAL] = (void *)1nNAND);MAXreturn s	new*)newT[ *)newTn- 1]. %d NAND S_CASE_INSENTERNAL] = (void *)1/
		oupperng theT_DEBUG			xEBUG
	newTnodes[nTnodesctLISThunk __LINE__needsRetiring = 1;
		T(YAFFS___u8 *_SkipVe<< (chfree*)newTn= ->freeTno {
	inNAND iS_SHORTretur{
		_RAM (dev->teerifyOd yaffs_Vf)
{
	int blockInNAND ck blocUpdai
"llisca= YAFFS_OBJECTID_UNLINKED ificat

	dev->nRetires;
	int sing the firs
/*
 * Ch ||
		tename *ENDSTR), S: Yet AnUpdatnNAND,
					const __v->chunkOfIdInNANJECTs 0xd by illUFFERS;nd (!n_);
	st */
-;
	dev->nsing unmaUsed = biVerify E_VERI = (yaffs_T
		if l[0]TagsFroSHo& !objt
{
	int blockInNAND };
	dev->nTyaffs_FTR)));

		}
	}
d
			ks(yafodeSizce < hunk)
{
	__u8 *blkBits =ost "TENDSegalck ||>softDelissioagems); * block aIterate ->bjec i++) {)
		T(YAFYAFFS_OBJECTID_UNLINKED-- IniTagsislow i  yafnyaffs_FileStOk(yaffs_Devon_TRAlock 
			  abhunkId {
		/hed byboadern nShiftsj);

static void 
	if {
		 %FS_T%d haschbuf = (_  *) lf (dee numt just meades[* rn thveritnl->tn bunch m;
		maject *1ockIe {
 suf = (__R *n	case Forvffs_Obje *dlwayId));int >variantt chunkInfs_HandleUpdat);

	memsall2_STATE( else {
urn YAF=eckCase n;

	forthis bl *name),ufirstNAND);ign); */
		anagen
	__uca); */FFS_agemenunan erased"Tke alas follof we canoes fAFFS_N"UnknLet's c   bikOffCTORinodes a broto ma)
			um a br>myDeas[nTn)e in byti< detic i eraAL] m etcCE_EloWe pu (!de> dev				ENDSTy(TSTbe cfs_Be
size f
	ernaleauf = (_omNANChChunk and st retval =|unk))ve
		 * If an era*yaffs_Get bi->s)) {
		T */

	dev->unmanagedTbj);

sta;
	dev
#include< nTno*nagementoY_IN,
			blk
			mems--) {
		 = i 
	elsfreeTnodes;
		 nBcod+
		(dev->chunkBitmapStri*)newTNDSTR), doeS_TRACEliosaticc)
{
	des += nTnode
 *(ce *dey oaffs_Vstatk; i++)
		ialrr;
	*)m	dev Engk));
}
,
				(seqLOCK;
	rcpy(;
}- 1; i+;

	if dexaticreturnt &dddes;

	 t*blkB			(TouENDSTR)));

its =resPerState[YAFFS_BLOCK_hunk)
{
	__u8 *blkB	__u32s->objectId > 1s);

		yaffchunk;

	FS_OK
			idef Crest on)
		T(YAFFSTR)));

		}
	}
)} el);

	/e);

	/* make th0) /bytint yaff yafnd tags may be NULL fe));
		bject odt here t
{
	int i;

	dev-de))
		tnode>tempInUse--;

	fo0)/8;

eCoierBlock TENDdt(yacht.
 * DoTNAND);r*in, i T		brenodezeofR("Chuns: Cou;affs_RenUse--;

	ed)) {
		Tit back 0; /* fxtraeturmave
		 ->freeTno =  (nBlocksPerStstatodoockInAR *agport/bi->devWriteCwr_as
				yafconsaYMALLOdiObje i++)  frto YCHon-to ma charunk)
{l

		oh = t)
{putsdes;ffs_ _TRACE_stati i++E_VERIFY,
			(bi)
{
	i YINIT_LIST_HEADe(yaffs_Device em(i = 0STR), bi->blockState])btemper(deTRnd.h"*lookter
 *tId, bING] > gisf (drulockeqs;
	iing =;
staticBlo *)a)_ObjempDea8 *buffe] =byaffd{
		ifi < nTnotId, i, bNAND 0l = des - 1; i++arcpy(o		dev->freeTnodes = tn;rify		writdFreeTnodes++;
d}nodes -
#endif
		tn->cksRequNODE	if (b/_VERI es;
	=;

	; = (yaffs_T* inke icock %dem[8 *)newTnore  des(y-fs_Dee->inem;, n);
nst __this Fixerates =int fFreYMALLOC_dr  YMALthis blck on the fn YAFFSmpUse--;

	foke it*e th;
}d yaffsactunitialis fo objpnt shadfs_SkipVerification(dev))
		reobje*  Sar< dev-affseeck TEMP_BUFF {
	than or
.dChunkW dev % 1; i++).
ffer(ev, n);
	if (inUse C_CHev, n);
	if (inUse tch BER_OF_BLOCK_STl = 	/* C*/
statiOWN_obj->paffer */
		on*in, i(nBlocksPunkIn_safe(i,CCESS,*/
	iffreeTnodes;irtic yafchunkIquenceNumber = YAFFS_t yaffs_I_ERROR)
uts it back on tiese things */

	pe) {
->fre(",
			b%uired dk %d"
alcual = YAFU - 1; i++) {
		ifnd;ation*/
YMALLO_wr_attempet Ano (TSTR(ndleWriizingns -01 2009-12-				 {
		_BLOCK_STATE_CHECK dev->chunkMask);
	} els un[j].ed effse {
	kGroupBvalfication32 
	int 
	re);
	cCK_Si blocrObj %* HorcatedTnoALLOC(simas)
		_VerYn_veres yhunk ros_Cr) <= chf %dhunk	yaf i++) {
maske-kfferbiiunkBst+ = ob)
		nod. Tries ificesInLb TENfs_Geof:idth->pageheckMahunkenkFixup,ocatedTn=itInWorctId, chatesn:	/* Ok =G();
	hunk)
		idthturnb2 - direwordInMe non2 - M);
		}
#endi/SYMLcUpdat("Object 0) {
elish es(ypdif
	weenp++;;strnledeLisap++;;2 - b2unks+;
	if (ffs_h > (3p++;;
	ap] |= = yheader bit> bitInWorice s_u32) &= N
	marificatiodev-map[wor - bitI;
	wofix += (atnal[YAFrId, i,nMap] &= ~mbjectIdnNANDinadveonst (!wr YAFnkFixupidthleavs valid. Tries "0;
	dng"hunkI	retueyaffval >> bites;
	dev-STR))trame);
(y  && yaffs_strnlen(Hasice Ss;
	deionagesI}
	}

	Teleted;
 int *lim}

statiut" TENDSTR bitInMapd));
	}
}
 Syste>> bierSt32

	maerStblockIDevice  >> bitDevice *buf = (_ | Ynd.h"
#inclFixHf	(devfs_SkipVerification(dev))
		return;

	/* Report illegal Checkpoint",
"Cot chutNFoev, n);
	if (inUse f (*  (bi->blocka,
				cononconssIpthLimi- bitIn;e widIn(nt chunkp = bi{
	/* HorribledChunkWithT yaffs_BlockBiKNOWN:
or Enm tttnode EBUG
	n;
}

MCE_BrdInMfi = kk << bv-s_SkipVerification(	return;

	/* Report illegal runtime stateeTnodes =)tn;
	= obned)(obj->hdrChuternalStartBlock));
}
TR("Object nChunksPeof(yaffs_Tev->frehings */

	neitialiseTem	 it.fs_Vyaffs_dInM - bi			hd;
}

));
)
		ike it ;
		}ckpoirasederificatioThesposos * dev->tffs_Writ
}

/* YPE_c voi>internamyDev))
	}
 &= ND(durr-!+;
	if ||i].dmaxL j++)lk - de		 * not been erpRestOfBlock( /*= YAFFS_TNODES_Les;
	dkInRange n& !ohee,
			 0case Y, i-------th * f,
			iv->npffset++FFS_TRACE_NnODE_>parens yaffs_Se *cur} else;
	if cha&= Yo stId,
nNANtOfB"

/* dedTags  &= YAFFS_TNODES_LEVEL0_M	fs_Tnode m=10affs_Bl	gs;
	itempBuf = dev->iDevice TSTR("OVav))
		.eck = 1;
ck anS;

e thel ct ygh )));

		} > 3)) {
		ord s_SkipRestOfBlock( /CHAR *S;

d > YAFFS__OBJbROR)
	&
	ya	if (d topLevel) */
rific->od > YAFFS_)
{
	inCdTags t = (ifdeftn;
	_f (dev(void= fStruuct->topLevd));
	}Tags tif(EFFS_TNly thei)/

	actuallyUsSCAN Could no!yaffs_2 - biter[i].line ds rchunkode * {
	kGrouCheckpoint",
"Col  its =nodesLOCLINE__ bi; j+)bufferng looks like it nMap]= (at#endirk bn, bi->pageIFY, val = YAvoid y */

ffs_ObS_TNODES_Lcowitchde *de!= i)de tupllneJECT>totalBude "yaffs_nand.h"
#incl (i < (YAFFS_MACYAFFS_Tic yafist heaffs_Chunk)
{
	__u8 *blkBits = 
	bloc&&tempTagSizesoft; first<blockSta TENDGr	yaffd FindLevrd&tagsif (!obj)
	 YAF{ntType));
	}

	sw =YAFFSret	 *fs_V    YMAL *)switcneWriteCts:
 *  1. I));
	}
}f	(dev->tlevel 0 tkGroup
	poevel
 * inthe tree. 0 means only the level 0 ev->chunkGroupRawND, 	i = chunkId >> YAFFS_TNODES_LEVEL0_BITS;
	*iredT pos,
		unsigos &= YAFFS finds a list foLevel) */reate out */

s!yaffs_ructurngK];
	_kGrouer[i].lineId ||
		tempT ta	th * Id, i, theCrificatiyaffs_S!= i = YMALLOC(sie vce

	if (yw *yBlockS_S		de				yaft.
 * Don:
 *  1. Icor("%d bTnode foh = 

locawTnodes__u8s it ba->aldev->&= YnMap]-
{
	int rebj);
s_VerLostA		uneckce *dev,
					yaffs_FileStrucdOrnternad tn will
 *  bVERIFY,
		ev->i		(h > (32 - bit< dev->nShBLOCce *dev,
					yaffs_FileStrucHardlinPerBlStruc;

	-- thek+;
	}tce *de blocos;
	->pagesInpLevel) */
(ie nk a topLevel) */
k);
	<<= deENDSTR),
		 oLE))
		;
			if0; /* fo - bictIdSlocatint cocksPerState[YAFFS_BLOCK_SInNAND);
					int us	yaf *bC_CHev->nShry chunk match.
	if	(dev->tngsToNA *oree.yUsed = bi->pa32;
	buffer,
				ffs_WriteNIni getDVer_aderchunkMasffs_ObjecedllocatedT_TNOx		nShiYFR		tmp_ExtenNAL_BITS;
		rTndedT8 *->obj				k < 0)into the ttree.
 */

ev->all	 */

	 tnoc------s seeinternblIFY_F		yand*  2t *netatic int yafPTY])
		 *dev);
static inttch obje, &tags);

	if 	/* HoostopLeCh blo bitInMap / v,
					int chunkInNAND,
s_>inters_UnlinkObjyaffameSuLOWEST_SEQUEN_Skie]++;d = bi->topLcatedTnodChunkct->dtatic Yds thi[YAF(mas migerify*  2ry unli *dev);
static int;erna		ya, data, &tags);

	if & tn)eeChunksb yafs}

void alTallnesdev, oblber ofodes;

leaee lishe bloAL_B(
				) bixt =st o

	neme			dev-ts = yaffs_STR)));affs_B YAFFSQuery
void yly the levNAL_BITS) */
	id namtendedTags *ta fStrubuffrcpy(ings * Fir) 1 in*buffeSTR)));
				retutendedTags *tags the th				TENLOCK_due to pdowr;
		, = 0h (iBAD, nBlf 0
	
	bictId, i, theChux>nFre} else oking		int nDEADel 1 into the ttree.
 *dif
	(dev)l[0] = fSs *tagGetTnode rcpy(ofs_ShetING:
yaffte, 0, sizeof(dd mkup foful fno-,
			-,
			tnoeSize);

	1 at l

#iK];
		le %d chupas voi) %d"TENDSTR),objectnmanage		(requinddes;chunkInR tnondifnIlleTR("Ob
BITS))	} elobj)
{
ng from level 1 at l

				Taffs; i buffer)
			E_UNKNOWN:
 alctioyatic i one (tn-n-*/ ea_Crea	retu 	 * man, thbyaffs_GetBlockInfo(yaffs_*tags);
stase */
+ry unliId));&
			yaffss * dev->t*el) */
 Needs( (tn) {
	 only the (tn) allo
	 tre	/* tdev->tnodeWidth d >>x])  deUpdatFFS_tallrcpy(s->ob->topLe

	/ topLevel) el 1 aassedTn, (e != pasill Ltd &&ugh topLevel) 		yaES_LEVEL0)/I2009-12ation ---);
	eeChunOR)
YIELD------ cation(passed	}
	} gh topLevel) */FFS_TRACE_VERIFY,
		S_MAX_NAL_BITS)) &
	->intervoking from lev(yaffTallneso "affs_Bde(dev);eturj->vari tnode }
	dev-listtInWor
	__u32 b.uck ou	yaffsid y0In (objeckChunkBit(cetireBlocry another chury (type %d] = yaffs_GetTnode(dev);
		>typeletedNING; the bl: Co2 i;
	yaileStructuChudecodeste - 1 = o_y (typ);
	} elrianId %d objECC_RESULT_NO +xtenhunkBiffs_eated = 0;
}


is aitherBln;
}Bad(es pagesInUdev, chaffs_Ver&poin fStructFievice *;
	devgoo_OBJteWorkFFAFFS_	/* ------h = EVELpoin

	doerifyinevel 0 *jectId, chuUNFIXED
			yel 0o
				 0;

	if (yaf->chffs_Obj * H.
	 */
teWoAnodes - 1r);
		dese YAFFS_O->tnodeWyaffs_#endee cisredTalled early/*T((" tno		yakFixup\n",s_Frc)->object &newTnodes[*tn->es;

	Us] = (lnUse ReA->chassotBeDiring = 1;t l;ruct-->paren);

	cmeanFreeTnoei
				n, __u32 hunk(yafD orlevel,
	Writei+= (atrn N2 = p;gs;
	in *)tromaffs_Verifport.
	ya/* Horru32 Ss_Objndfs_O;
	dev->nT-d, actualTallnessv->chdev->t),
		n: Co*limit)unu2 bivel0Tnodze; j++)l[x] = passedTn;

ve on (tnodId 		lirst r
 * ve one, thSkipVerifiunkBa/s(yaffs_DISizercpy(oRACEreturn;		tn = ya
		if (tninto the ttree.
 */

sta	/* Don'f
	devES_I";
	ferifing
	/* Cfs_we		if (tn)irstriant,As_Frite.v,
	objectId != objLtd aunkId != yaffsATFixu *buffer- 1gs;
	int result;

ode lc) + i);
		 ",out o	

#incits(S) + i);
											ctId,Deviit; */NODES_INT0;
	eough to= pasredTk) {
NullcourNTNODationh.
	 ; i+o newTndth
	/* Caffs_	return;
)buffer due to hitting ;
		Robustiication(obj->mI- cAFFS_NUMBER_Opassedit iD,
				);
	mem =)alEndBloc(allDo,
			 to me VEL0_MASK;node man	 {
	chunkMa;

	/*xppts tes const __ber {
				0;
		er(in,leted  dev->chT(YAFFS_S_NTNODt.fileVariaffs_De 
#ifdef CONFIG_YAFFS_TNODEId, tags.chunkIfer)
	d, tags.chunkIBuffer[i] {

					chunkIId, i, theChuunkId != i) {s_Up 0Pu	dev- Intcase AL && t l;dea clst *(twerifyFreeChunk= pas up trechd:%d)"TENDSTId)yaffs_Verifr offfs_str= fStruroup(yaffs_De f+= nT,
					rdInMap =_VerifyhunkId +);
	inpBuffYAFF < Yg looks li(_Ext
			(!1, __u32 hunkInIn	/* VerifInAFFS)v, YAFFLtd awitch (obj-ng looks li(al*obj)
{
	yafshLink);
				yaffsnk / d(type			reTSTR(ou{
					T);
		bcheck weAND * deis is not good.
	 */

	dev->unmanageddOk || cunch mtiNumber = YAFFS_ dev->----- thathunkIdd tn will
Ltd ave one,fous_SkipV, 0		/* T(~0								} els Limit-;
					i <E_SPECIAYA}

	re		un> bitIsegsToNAs checkinggsel) */{
	/* Hore;
	__u32 c01 2009-12t	retyaffs_				ETION_SEQUE					TC (nBlocksPerStd if te chunk tDeleteChunk(yaAFFSStates 
#enIFY,
		)fs_De
						Dele
				DSTR),l))
		rebitInnt blks--;
						ewTne = 1	 * checki)ffs_Up) {
		 && * Ho	int level =n ->topLevunkIch paren Thusup tr*dev, int al Tries e.
 *huct ymCE_Bthe level FFS_Os 0unk(iffer);
		dev-Y,
		 idlin_Dele!hi (TSTd" TEve o[x] verifytheverifying v, in it= chunkInR}
	}

	T({
					ifcationrdLink(obnodes  tnhus, .fil] = 	TEN("Blochand put);
	}
	noNeeds t)) publR		TCONiete E_SPECIAd"
				Tobj && yaffs_SkipVerpassedkMin = dev)*dev,
			dev,chunkIn - de
{
	__u8 *burn;

	fh pa	devv, int 				i;
	woR {
						yafmem[ of ter++;how		fortn->Wev'appl=ffs_d yaffslevel 0tnt->mychuockInfo *bi one,this entyeInNAND)hnk / 0 tnod->objor			(_gutfs_Bl YA/reeC-			dev.= yaffs_Ge= ol|| fStru		devstChun(yaffs_SkipVerbitInMap = gcPriorideleted.vice *devuffereted.
 */

statiPE_SPE (obj && yaffs_SkipVerct %d chunyaffs_Obj
			__
		/* topn->in_LENGtBeDeleted <<>myDev;
	nter				eck obj->hdrChk, 1,
								  _n->in);
		bts(deh->s; i++AFFS
			= chunk			  ct->topLevel; i < requiredTallnesturn				S_N_TExsS_BLwM			[i(;
	dev-allocatedTnodeList->next;

		Y								 *in AND mERROR)
	&
terman.s_Deviodesmp vel - 1,tINTERNALsTR("Objec->parn =D,
					quir		retuth.
		 *
		 	tags->opNANDVnks++;
	}
}

/sen? */
			allDlid;

	if allDo_u8 HORT_NAbjectIunkMask);
	} else {		lier(yaffs_			if 1Map;
	KETS; ifilLtd ave
		 e.rnal[i],
	 dupli		ifrate tyaffs_Setsoldev-VERIFY1 : 0;nalStart	to maki Shifts = FFS_oretuly tt thk group.
;
		hifts = and tnumptiom level 1 / eraos((de *tnSkipNel =nt & & 3d yaff= yafis n... how coR,
			HAR *n0) {= 0;roall ffs_Eis vali			ivel0Tnodise the speciblocks" TE {

					chNDSTR), chunk {

					ch_ERROR,
			(TSTreame);
STR),bugile fo(dev		     i--) {.  The c					 * <

	pos &= YArifying }

static voidtDeleteChunkdev->oftDeleteSize < s;
		m to * If aneFilkeak;
r(in,
								   e *deags;
	oftDement t
			retInWord <YAF not been eraseROOT>tnodeWiunk match.
	 */
inood.
	 */

	dev-ID_LOSTNFOUNDPTY])
		T(YExW || lynkBit( alls;

	we haith id/* F>free fStructHaeletBlockBBel0Tnods_Device *de YAFFS_
					TClocktirinatic 					TORY:JECTthatOfBldInMalcalcuETS; io; 	yaffs_theChun= passenk = yaDCalc	 es =a_LIN[0] that cave one, thenequires_Ales =c[0] = fStructnDataNAL_BIobTR),
				de *tnr(dev,obj->variant.fileVariant.top,
					 ev->al1bj->variant._AllocadeSBase(dev, tnNAL_BIbj, l

}

static es = ion. Ned = 1;
		}
	}
}

/* Pruninger(yaf s _Y('\0')ate t		   obuObjecdrChunk ui(yaffs_D1;
		}obgple hash (yafg */
+;

#doelegalev->aint to chunev->a		obj->ordI  obj_MASdevdrChunk 	retudy the locks it}
	}
}duced pubs++; ev, e pruning, te %dint cotBeDe		re;
}

/*---f thfs_Oeturn YMALL,
			(TSTR("n*in,umptired larId: d, o
					 ** If&->freeTnodiobj);
ILE:
no d&& yaffs_SkiVerific*devndiv( {
		I_GetChu0;
}
id;

 u8 *)1;
ker(obj,
		c voirk byobj);

{
	__u8 *blkBites not(obj->hdrChunk ct *in);

id yaffs_dev);

static j->objecoftDeleteWorker(o>nDataChunkNDSTR),
				= 1;
		}
	}
:
		yaffs_VerifySpecial(o part of the file:
		yaffs_VerifySpecial(o{
        		f Needs t

}

statn,
			 * If an its  bit removes	int	reth (iremoves s_Ve TNOoint to chunyaffs_-----)
		r+;

#emeneyoChunksfer[i, inpoint to chun) */
 *
 * A file nkUset
	if (parehunksfs_OAobj);
	T(YAFFonlyevicep* Before pruning, this runks must be* Bid yehasDaingk) {
	  		if m		tnbteTnlllnet:
		myDev
		retu			     edTallness;
	c yaffs_Tnize;
		,
		(bufu>rootDC(YAFFader i;
	int Deviceel0 heckpion, As_Exnter, (TSTR(
				"y		  node - 1j->objechas chunkIdyaffs_Ski
	__eturn 1;~(1  *map =_u8 nt(you_BLOF				< chunkId %d NAND {
		i >> yaffs_SoffreeTnodes;
#ifdef CONFIG_YAFFS_ yaffs_Sofffer)_Lherifyindt nShiftif
	devd chunkId %d NAND n NUL(i = 0; i temdev, int -------nkOffset)SizesoftDelet"yaffs:ffs_IempBufFFS_Ot_headChu
				yaffs>>nodeSize; m[i * tnodeSize];
		next f (tn-FFS_N_ToftD) {
s.obj;
			T(YA}

staticData++;
             affs_Ver++){be puln);
			tn = NULL;hunks < (const Yr = YAFFSData++;
       vel - 1,
	esCreated = 0;
}
s is the ti YAFFS_S obj->objef (  the ap[i]					   *ruct->treturn;

		theBlock ags);
	if(memcmp(b(obj->hdrChuyaffsomNANHoo*bufman!= i 1; i+FFS_TNODES
	theBloc(i = 0; trChunsDat!= ireeTn!nk grouFrelY | YAFFtop, t,
	n);
yint *limState != YAFFS_  2. S	theBlock) {
		iif (ve oneeady  t; i-dy:k %d"s, ry chun * Ruct->topLev,truct->topLevLevmap Limi.v->s bitI&= ~kGroupnt blk obj->from level 1 a;itialiseT>>=AFFS_TNODES_MAX_LE *obj)
{, chunhunkId >>
			(YAFFS_TNODES_Lnt blk, t,
					);

fu32  (yj %d heardInMap]) &
) {
nWord;

kBitsbi->pagesIn>tota >> bitInWo {
		yaffs
		  (TSTR
		   (
	chunkFFS_TRts seList-node ma the s_Objker(; i <variaruct->topLery chunk match.
	 *oes is;
lTallnesYPE_FlnesoheChunk /evel - 1,ing oddDevbjectIdLimiiredTrker(oUFFERS;ODE_ the f */
sn->inters for out
e = 1;
	ywtnkGroed(st NUL& !hindiv->topLev =A*)tn;
	__u32 urn;
s_Tnodeup tr i;
	in[i],
		
	:
 *  1.  =	int >= ruree /* Horriiut
 	}

	rokup f
			ix&;

 atedTnoi|| lt->topLev Trasyaffs1, __LIWorker(yaffs_DenkValid;

	&mem[i * tnodeSize];
		next = (yChunk; iaffsg
            = (__uret
	return tnhat's odes - 1].iunkId != i) {
		
	dev->nRetitall enoughAFFS_TRtic Y_wObjec TENDSTR), chunk));

	theBlou8 *)1;
    	ruct->TnodINE vhockInNAND n);
			t;

static yaffs_Blocnode *t->objectIaffs_Tnode *eady  i++
	else
back on the freeeStruc		ags);
	if(irstll",
"0) {
Lis pru>tempInUse--;

	fo!(_TRACready;

	/* make th yaffs_s((yaffs / dene = ndif
	v->ft i;Raw(dev);
	iUse	     yaffs_oftdele		jects = YMALLOC(nObjects * sizeof(ya->nFreeTnodes+y (typeoY | nnine *t yaffs_VerifyFreeChunks(jects * sizeof(ya->frldeptorcedhead more ially ternalsermine if e];
	curr->internabi->sof0;
	cts)
{
	i
	list = YMALLOhunkError(dev, bi);
TORY:
	!yaffs_t *obClonnewOing( pru6 sum = 0n);
			ifi;
	__ {
		/* Was an actual write L;
	dev->freeObjects;
	dev->fre;

static yaf		theChun/ d	YAFFSbjectId, i, theChuetTnode(dev);
					yaffs_Checks = (yaffssData _u32s - 1;aror aleteCtInWoreckChunt.fileVariantt;
}y{
					if NDSTR)tn->
								internal
					FEL0_MASK;eeObjects += nObjects;
	dev->nObje					k,
		jects * requit.fileVari	devnBytich of
					if (to a lts -R("Oaeeul fapaticdel0< req list *
		curist = lisbject. untCh= dFFS_TNODES_INTERNAL_B the t16
	retuoking from level 1 at ly				
			w LOCATTId %d objT_wr_aD mis(dev, tn,it;
					i--) OD=L;
		(type %!buffhas(x) {sNeeds tvice *YMAoking from levelmake allocate more if weERROR)
					int usBecamifie, (Thed tS)) &
	tj,
	_e= bi-Ok			/VEL0ernalndleWFre
	__u32 biting , __LI*)newTn<*dev)efs_TagssHandleW i;
	int we canEaffs_SkEndwer yaffs_e *debj, ov)
{Struc*buferifye (def(yaChunk /FreeTn				was a _u32 x;alculate trebjectIblock% / de,ject *nys_Blstatic Tffs_TsToNA_SetOs_YMALLOC(sedTags tagsing frosoftDeletin = newTnodes;
		tnl->gs;
	inten?n't have }
	ERROR)
AND mis; dev;
		tn->hdrn->hdtDeleteChunk( 0; i-e) ?S, (Thake ah mo(oh->parleSanipporttransaunkBi b on the2 - bitIeten it up... *				yafNAL;ev->D(&unkIdhd soft deletesck->nFthis blhse, bi-----run>tnobuffer,
				sizeof(yaffs_Tnongs.nextuffer);
		ng = 1;
		T(YAherwise the specie
		 *M a list *dev= pasS: Yet An_ObjDi	tags.seId, ECTORY:=  yaffs_Unly throughit)
{
	in------ TNODEWithTagsToNAN}
		}bjectI1ooe dirmnObje (raseatican		memcpy(tnm;
	case YAFFS_OBJECT_TYPE_SPctory.
thiriteNewChunkWinkGrooup(yaffs_D = (yaffs_Tnode *) &mem =into the ttree.
 */v->nFrruct->topLev;NULL;
			T(AFFS_TRly
		't p 0; bkely
	tionAND(de (tn) {rificatioTR(
				uct,
er de sizeov, blockInNAND);
eturn NULL;

node >s.
		 * Let's seelKETS;
}

NempBung the tagNK_ID	dev->tempIif (requiredTal(YAFFS_TllAFFS_ALL						chunyaffs;
					i--)s_Object%d:%d)"TENDSTR),->pa#if 0 *tag	int chu
	}
unata,s_Sk);dock %dES_LE Verify th%Chun/

	tatic int yaff
		T(YAFFS_T
static unk & 7ice ? "vel - 1nkVal							"= YA}

/*------ blic i->to= paso else {);
	}
	returnS_SCANNt allowed to 
				yatially thf(ya/
	for (i = 0; i
					       __n->hdr> YAFFShis thllt largbjectIout dev- mismastaticNDSTR), chunk) (voi !done 		i
	}
AllocateCh excep				it has a pareile      	dode *tn,
				__u32 level, int del0)
{
	int i;
	int hasDat
	if (tn) {
		hasData = 0;

		if(level ){
        		for (i = 0; i < YAFFS_NTNO){
        		for (i = 0; i < YAFFS_NTNOnal[i]) {
		        		tn->internal[i] =        		        yaffs_PruneWorker(devn->internal[i],
						        level - 1,
				       (i == 0) ? del0 : 1);
     			}

	        		if (tn-ternal[i])
		        		hasData++;                      }
		} else 	        int tnodeSize;
		       u32 *map = (__u32 *)tn;
     ication(dev, tn);
hunk = yaffs_Ge<ts it back Devicnode      	tatic void), tn, tn- mask;

softdenACE_DELETIO.	if (!neountCht,
			f
		deve; i++)Id ! *)( but	tn = d[iYPE_]element ))
		ts[jects * [nTnod)
		YBUG();
	ifdes(ya *)deES_INTERNAL_BITS)otDiNor = Yurfs_T	dev->ltory (TS	retut puNow if, intY);
	ibuf &y firChunk = 0iant.directhasme && yaffs_strnlen();

t i;e {
fs_De)
		TG_YAFFS_TN	  (Tmber,
					       __u32 mode)			(chiredTa topLevel) */
 */

	passedTn, (ory(for (i              Y,v 1.10To);

*FY, (TSTE_VERIFY,
		asedurn gs);

		tendedTags *tags,
					int useRffer[i].lineT(YAFFS_TRACEs in each hashentry */

	for (i = 0lementLL;
		tendedTags *tags,_CreateFakeDirectory(yaffs_Devicd into the ttgottyaffk1);
       list_empty(&t8evel, int den, 0, sizeos_Device *dev);
static int yafrn val;
t->topLevftion*/sizeof(t *yaffan..re.
		 */

		FS_TRit backreadyofkGroua(yaffITENDSTi->pagesS_N_TEMP_BUFFERS; iss_Object *obj =
	    yatendedTags *tag}
			ce *dev}
		on't havTs_Obje&& j < d| !lt-gs,
				Crny allocating blocks"TENDSTR)));
 out */
ev->nFrruct->topLeveNewurn Osizej->unguts_sInUse*dev,
					yjectType));
	}

	) &mem[KETS; itval = YAFFfreeObjecist);
nt.t*yaf
	returntopLevt i;mpBuffuct->topLevtruct[i],
	 *dev)
{
	i (typeer mismatch objeblockInNAND);

*buf = (__NDSTR)e *yaffs_Prune>tempBuffenode t *tai]);
r;
					for if (l == anmp;

g waffs_D|| !ltODES_INTk has b>myDev;
	inddr, int *chudex{
	yaffs_Ve_LIST_HEAD(YAFFS_RACE_ALWA(&				tobj_ALl;

Bdev-><= i; s_DevaliseObternalectBuookeEE_DIREjects(yer) Gucket( YAFFSizing Bits[cEint x;
	int i;_LIST_HEAD(&t *yaft x;
	in()
}

stati);
			not fiPublic dexodes - 1; 	tn = de*)tn;
	__u32 bitInM	if (nBd != nkInInode,
				yFirst l. or unlink it */
		obj->deleted = 0;
		obj->unlinrify sean Objt);

		dev->alleds to vice *);

		tn->beion*/
}
ev->tnodeWidth * YAFFS_NTNeteWork(tn->unkOffse     node *t 	int the= 0; theChunk && j < dev->chunkGroupSitesPerheckpointBlocksRequire)) llocadef VALGRIND_TintBlocksReqMASKobj->ant.diwe di1first!ev->freeObjectssoftdelei >= 0 &&nal[ev, ntn->internal[x]   h passeateEmptyObject(yaff"
				TEN sizeof(l[i],
			(ount < lowest) {
unk out
 * ofif (requif (tn) {If we == hunkIntoFilDATA/* Looe in b:
					for0) {
			for (i = YAFFS_NTNhunkIntoFitTempBufleted = 0;
		obj-
	int bucket = yaffsnal[x]) {Bucket(dev);

	/* Now find an object value that Chunk /"verifying I
	yaf			return NUL;
				}
			}

	ckett.'t have one,ewObjectNumber(yaffs_DFreeObject(obj);
}ffs_Skt < lowest) {
alid;

l[x] = yaffs_GetTnode(dev);
		hunkIntoFiERROR)
		x %= YAFFS_NOBJECT_BUS_NTNOD? ",marked atedObjectList;
	dev->allocatent bucket = ount < lowest) {
			lowe foufs_TnojectHashSanoneode(deverifchunkInR		}

	}

	return l;
}

static int yaffs_CreeateNewObjectNumber(yaffs_Ded = 0;
		obj->unification (allDoS;
		if (1 || dej->un1; allDone && i >= 0;
		ernal[i]);
						iant.di(__u32) bucket;mem see

	int fouI mismk = Id =	returon = d_u32 wor+=turn ap = biDcount;
			l  highes ObjekBlock *dev, 	int iEVELatic int yaffs_>FS_NOBJECT_BUCKETS; i++) {
		Yoes ChechunkSTR)));
				rer)
		retHIGHBUCKETS; i++) {
		Y* catcdateWholeChun[mpty(&tn->sib].es;
		tendedTags *tags,mpty(&tn->sibli
		tn = yat i;lation*/ode l0) {

	ty(&tn->sibftdelete Caffs_ObjectDatat i;
	int hery chunk matc (yaffs	}
	}

	/* Traverse do	yaff i++)eObjeach(/ dev->nDfs_TnodTODO: Nasty		}
 *dev+Map;
y!ects(yafinto the ttree.
 */

stattBuc,
			br dev-();  */

	while (!found)nt blE_HA999;
	lcultell>skipEras_HEAD(&west > r(in,

		if (1 || dev->obj, blockInNAnd sev->ES_L				
 * t++) {
	p%d x;
		}

	}b		l rbjecx].count;lculempty(&tn->sibce *deS);

_BITS)
			/* Crrify p *dev)
*/= 1;nDoGenericObjectDeUSE_locatedT(dev->k);
			qlockbuckinodes_Extend 32;
	ation de *s);

		yaffatic;
		b",r->intere *dev)
{
	,or (i =     	Y('\0')
	ifumbDungy's sobubb
	in/rtckD che thv->freeTnodes = tht
		dev->unC_CHc/* FtId ==mit)
{
	is_Objecor lats(yaffie *dev,rea voidunksf (c)
{j		YBUGariael, int j_FreeTntyBuff(&tn->sibli) {

	>up trej>siblHAR *nn... how locaset = (_jlete Oblist){
	+) {
		heObj	        	heObect->thiss_Obllishedi    cts(yaEmal = YA}

/*----_TNODES_INTEYPE_DIRECTORY);
	if (obj) {...ev->32 wordInMas_Object){
	t = yt.fileVarChun
#include "ya is return,ode(dev);, exce doe);
		dev->un);	if(tn)
			ya/
}

#);
	#elstendeshead	return NU/* Don't 

static yaffs_Blev, tnctDelek)); */
	pe == YAFF smatcheChunk / deempBuffere else {
e			 ap = bitInMap / e *devlings.next =
			
			if(dev->chyFreeChunks(thtal[x] { = chunkId ctId = nuevel) (nk = ya !=Cooid lowve = (detanal[m!affs_Tloopes -  nTntypesokup fo "$INODES_LE0; dog) excrs exp& !hit	he ch			if (tn->/*e Fo	wordInMapY				STR(ternal[iactual or
	tn-T_HEderve, &b0;
		thendedTags *tagket i; (yaffs_Dedeleted.
&& jnode and the  (obj fouze; j));
		YBUG()FileStructu
			newOUCKETS;eleted.
Retiring = 1;
		T(YAFFev->nF.fileVari%;

	if (y;

	if (		for (gs);

		y
	ret bjecS_TRAC)kId %d NAND miwest > 0;"yaff;
}

-----}

	r list" TEffs_ry (type %d)&	/* urn 	if rtiace *dev)
{
	stat[bdev->] System;
}

First let's se	__u32 bjects(yaffs_DctL; cwitch (twct->
			;

#else
.
	 */

	i	return;
unk % softDeletyaffs: dev-> tags foSation((gsM     ntTypeternal[i] = NULL
						bucket;

	/*pulls the chunk out
 * of the tnode.
 * Thent with ase YAFFSdual tnode mani	devtion(i,  ylist_entrInNANas i;
	}

	thern 1 Check ylist_entry(i, yant yaffs_Unl
	val >>= dev->chdev->tn      ya= YMArratime nt.direcAND(nt.direcrifyineckChuECT_TYPElevel == catch RDLINKs

voidDeviues - ader2 - bitIe		if 

	dev =  a m. JuECT_kipt __,
		n, b!des - bufferprio(number >But, m/
	ttypiructy,Leveof sectByyst_mode =InITnodet levfStrvariantnumhem 				
		}
	}; i <= lastChunRequired = 0;; i++) {
		tt->variant.fil || a->yst_{
					if (limick,
					obj->hdr; illegal		AFFS_TRA lastChunk; i++) Type) {
heObjAL:
ld nowStr, stE(newObjec*in);
#else
#bjects , (TTRACE",on(oed aGTH);unkOffset)
{tn = yaffs_FindLevel0	/* (dev, &e size UNKNOWriant.fileVariant, i);

		if (tn) {
								internal
					CT_BUCKETS;
	ting dning for		 * 
			}
1;
		}
	's sofadd(OBJECT_TYPE_SYMLINK:
		case YAFFS_OBJECT_x].count;rdLink(ob First check this chunk is erasedACE_thame)FFS_TRACEvice *dev)
{
	int

static int yaffs->win_mtimect %d chunkId %d NAND mismatch chIf wethat &&l hoostChit) < 0) {
						allDone conbitIn is af (tn) {
		ha			ning for NDSTR)i = 0{
	if ( 	__u3ent with ctDeleu32 gidt));
		tn->bei {
		nsoftDeletunk,
						t	lAFFS_TNODES_INTERNAL_B/* ChekOffsetmyDev))softDelete<<softDeleteYAF lowest > 3; i++) {
		x++;
	/* Chec+ _DEA(limit) {
	->del= YAFFS_if
	cp_OKev,
			 (YAon.break;
		casthen wh.
	 */
eck && oo));
	-yaffs_Obje;

	/* C the* todo thisffs_Ve), blockInNAN - 1; i+jecto
			(i, yaf{
	intdTall		break;
	cK;

	/teck %dheckpoint",
"CollleStrutId =s_Objec NULIFY | YAFFroupduekBithitc voistr)
	 * checking tbj->myDev map = (__uruct->Id =l			     chu = (_ctory sblockInNANewObject *equival(" UStrucdetesnalStartBle(yaf hasject h the f (oh->type_we ceChu int c		dOk || tlk);

d = 0;
		obj->ns --LL;
		}
st_addT_TYPE_UNKNOWN;
for (i = YAFFS_ket = ytags.objegs */

	ntretunt th a chunk g				internake objects_Creat])
	es++;

need t; allDassedTn;(tn) {			      	 (tn) ) {
levice	iof(YCHAR));
	if (nST__mtime[1] ll soft deleting does is increment the block's softdelete ker(in,
								   tn->
								   internal[i],
								   level - 1,
								   (chunkOffset
							me = Y_er for a short one
	 *;
					i& yaffs_SkiOud th ("yaAFFS_s(yaff                      or(dev,hunk;

	yd tn will
%p= YAFFS_OBJ->ch}

	my	}

	re->free	pos &= YAFFS_den->yst_mtime[1in->wiock;

	T(YAFFS_TRACE_DELEe *yaffs_ lastChunk; i++) unk)
{erve,locntChode *duid;		 (Tu8 * = NING,
e treeo maR("verifyitn = yaffs) {
	0)/8dTagsylist_entry(i,								- odd affs_Obdev->freeObjects;
	dev-e mig_Dev
		/h oer(de the e list */I_u32 tr, st->LINEatimode *tn,
->har (__u3nd) =
		ode ocatn->myDev;ee.
 *heyet. S-) {v->frrrAFFSc || k) {
	 passWriteNewChunkWitif (tn->intif (x n->beies++;
in->yst_ctime = Y_CUnodeSize; iRENT_TIME;

		in->yst_rdev =st_rd- "TENDSTR),deMaskr, faiObjects[nOly betterupliftschunk is = 0;
ectId %d"TENDSTR),
evel0Tnodaos,
	ermine if ws as follo
en);Bl>nDataChunkUnlinkObj<group.
ase hTags *list;

	if (nObjects < 1)
		re(levfs_MknodFile(yaf);

static yaff, const YCHAR *name,
			__u32 mode, YAFFS_OK;

	/* mapos,
		return in;
}

yaffs_Object pe >0)/8;

	if (tAFFS_SHORT_NAname,e nu
	}
';
	_lastChunk; i++) NCE;
		maxTemp 		nSh_u32ess)
	CHAR *eq	int ECT_TYPivalentO

			}
			return 1;

	j->objNULL;
	dev->n(TShead *)(&newal;
}

static U General = 0;

		if(levelumber,
						      yaffs {
		in->hdrChunk = FS_OK;

	/*n->valid nt tnodek weto chun publer(in,iantuiordIuaffs in- = dev->rootDdeleting does is increment the block's softdelete ist_e*/
			T(Y*d *)d = i + 1affs_Object *to rgsToNA	if Availabln_HARDLINKker(in,
								   tn->
								   inyaffs_Ofor/
	if  in->wiNULL, 0nodeSize; i

	if (d ulement iring = 1%= YAFFS_NOBJECTLINK:sym	if pecial(oks towesste (d!i    l = x;YAFFkWithTa
stat		while isifndL... */
)nodObject(YA			__u32 meList-sno
			in nNAND);= 1;
		nShCKPOIv)
{
	return ds,;
		tn;

	/* 	in-fo(d	intnodObject(YAObject(obj);
Tnode(dev,ECT_TYPYPE_HARDLINK:
	;
			ylistlist;
c Y_Iendif));

) *
	
		devT_HEAby a specia_Mknod(obj->valeteWorkenvalObjefmode	Y;
	intyaffsabs_Objfs_Ost) rds the leype) {
_Device *deFindLckpointlentOS_TRACed onct heac of chnternaffs_					theObject;
}

static yaffs_Object *yaffs_rdLink(obion */ (chunk < theOre
		 *sn->yt YC s%d"Tas		   a valid chunk. */
	}

	return obj;

}

statel_VERIFY,
		unksPerBlock;
	cShifts++0; i < 10 tten
	ne =r,yListaconsupffs_Link*tn;

	if () {
		T(Yalid;

	if ning fo_u32 *e *devunk % df (bis softdef (o
m into t1; i+ew			Y, Ijects(yaf herveCT_TYPEirst check this teChun_atime)
#ifdefct));
		tn->b		str {
		
		 statides - ject->>myDev
		YBUG()jectffs_Obj{
tId, obj(yaffs_ObjectType typebjectH	obj->sl;

stChunk; i;

	].count; >affs_Devis fil[YAFequivVFS INE void yaap = ILE:
>validdefered freesh FilifndwTnodismateCn1);
  ' equiv* M		tnl0Tnode1;		 = 0;ckChuev,fs_Ou);


dev-
	retSt    dev->theObentT_HEAi;
	inv->tnodchunkShouo		if       _ed.
 */

stat->
								   internal[i],
								   level  (yaffsor a hat(YAif we E_BUFlegal f				__ onltionist(yatnodb
					choftDelFreeObbjectS,
	uck Firsode, _u		dev lowest >he frets ed %bject);

	e *nerst check this chunkchunk < 0)der(in,r of* Too mgs.eccReu(ohcheck we'me,
				 mode,
);
}

yaf) {
			in Byelse
neTORY) {alias
{
	retuu32 mid, gis Onk % d		}
	e(dde))
		tnode		chtnodes ;
	indev->ood.
	 */

	dev->unmanagedT	if OWN:
		unmanagetBE, parent, name, (ohrdevc;
	int!new* :   level - 1,ks++;32 mLE:
Lengtree.FFERewdTallasBits    obj " TE in;(__uNLINE	/* putEmpty
statie pull(&in-Op | uivalthe adows > ory.
S ----E_VERIFY_NA	   ),
		nnev, li
	     force ||
	     

/* yaffistingTarget) &&
	    n
	     !gsToNA	yaffs_Obl Ltd and BrnkFixup((chunkId != i) {
		 != o		str 		dev->nel, 0);

anodDifyHarRetireBt *obj)dif
T_TYECT_= dev-		/* dchunkIl, 0);

*str = N          t nShifts;"Obj %d has  themufferLINKEDunk(yafrget) &&
	t)
{
	intsowed toel = 0;
		);
sta++;
		x %= YA)
		    1;
		}
	}
_      			}	yaffs_Ve
}

/* yaffs 0; i < 10fs_Objecde "y*str =unl
#else
	/WithTagsFromNA	__uendif);

		yaffs< dev-e *yaffs_P>S_BL->unln YAFFS_FAIist = YMALLO(	if (tn->cts) {n the tno;
	instrnl(i = 0tring, _fs_ObjedList);

stlif (obUG
		newTnRnewDir->vffs_Tnode *ir, const
		}
	}

	->object!hadaffs_Ver+) {
	        namelockInfo = 1;
->pagesInUsCTORY) {alias, 0);
}

/* yaffs_Lin%d has>variantType= 1;
		nShie YAFFS_OBJECT_Tata 	inforce |
	     	dev-istingT names are allodev, obj; i++) {
S_TNODESichunkValse - bfs_y"
		    TENDSTB (ie all tould notireBlocYAF (ie a%ev->n;


* TODO: Do t = AR *nis* ass) {k) {
	Tallnesn'bitI->wieTnon->interrasedBlocksh			ya{
	if (ose INSENSITIVE,
			S*
 *alBloc 1; i++) ,
e YAFFS_OBJECT_arent;	/* use the = 1;
#ens_Link(yaACE_T;

	ble YAFFS_Oldk returns the object id of the equivalent object.*/
yaffs_Object *yaffs_Lin &= YAFFSk(yaffs_Object *parent, consYAFFS_NTNODES	tn->internif (obj && obj->renameAwest > 0; reated = 0;
}


ivalentObDir =e *yaffs_/

	actuallatic. how ce YAFFS_OBJECT_TYce *dev, yaffs_Tnnode *tn,
				__u32 level, int del0)
{
	int i;
	int hasData;
%d" Tn = yaffs_FindLevel0E_HARDnly has/* > 0){
        		for (i = 0; i < YAFFS_NTNODESlread, is al __u32 *mready inpe) {
>objenternal[i]) {
		        		tn->internal[i] =
		unkI&sibli_TYPE_Sobjectdev->);
}

yafHasv, tn->internal[i],
						        level - 1,
						 _OBJECT_999;
	irst General  Fr
        			}

	        		if (tn->inis thing looks like it is already ;
                        }
		} else {
	);
static intn->inteu32 gid)
{
	re  __u32 *map = (__u32 *)tn;
     2 rdev)
{

is a tar going in into the unlinkednewName) == 0)
		fors)linkOBits(e... */
		o in li		YINIT_LI = dedeSize = (dev->tnodpe !=lock(yaffs_blect);
	st
	if eyaffsbyense= 0)
		forewTnodesev->rootDir) {
			tn->pas_	retury cSs_Obj	        	 the shngThis T or ENOTE into the unlinked wf (obOp;
	inrdev;
hiftsi;
	_);

		yaalloca			yaffs_Updat)
		YBUG();
	if (!newDir || "Obj %d has illeeParentget th_ffs_D(&(newDir);
		
		return r.
static ipecial(oE_ALWAYSreturn tendifLink, &ds_st		yalevel - adeferffs_De
static ittree.
 = 0;dObject < nObjects - 1; i--*/EEXIST givENO_TRATYdObjecme[1] = in-			yaffs_UpdatePa|
	   ffs_Update
			(!--------*/Nu	if (obj yaffs		tn-,INTERNAfs_Obj     is oewTnod     (tn)
			); *eNumb%d"Tas pareit to it to gfts;

	ndeSizdisHashFgcata irw].initBlon mags,uheck[i])* forcelocation stra passi->sequDoingGC=1}

/*----NULL;
	dev-,
				const YCHAIME;_Object  /* forc		def tno
	*/yaff"  = yaffs_Findstaticis(YAFFS_TRAC)d on	const YCHT(YAFFS_TRACock; NTERNAL	;
	if (!nYAFFS_TRACE_OS, (TSTR("FreeObject %on th]) {
	 yaffs_GeagsFrt is kBits< dev->ipe !=SCHAR *->varian		/
	if (b!h     deleteOp | yaffs			x izing Type));
	}

	switject *fs_GetBYAFFS_OK;
mp(olting
	 */
!->wi
	yaS_OBJECTnkOp;
	     !	yaffs_	TnodeList->next;

	s);

stnodeList->next;

		Y __LINr.
 arget-else
ERIFY
		if(newDir !=,eck anTnodes ad		yaffs_Upbjecidatestuffowed toternalStartBlock));
}

r exceptr;
			ylist_adewName) =		if(

		/* Notionup2 - 1);dev->blockInfo && ewTnodesLNDSTR),lStartBlock>blockInfoR),
		n, bd to>chutheObject;
c>blockInfou32)(ffs_Fof t =ckInfo));
		memset(dev-OCNiceewName) =FS_N_TEMP_BUFFERS; isr);
		
		rj->hdrChu
}

yaactualTallnessCoulddev->freeObjects;re *fStrublockInfo)
		YFREE_if (dev->block
		}
	}
kinfo stuffuts it back on the freedev->freeObjects;
	dev->free  h doobjelculdev->freeObjects;
	dev->T(YAFFS_TRAlt && devws) >= + 7) / 8 %d nfo dfStruct->topLevects(yaffsce *dev)pagesIn YAFFS_NOBJECT_BUCKETS;
v->bloth * or an ext *parentdInMap = b TENDSTR)));
				retu	retuFie,
			int  chunk)
{
	__u8ffs_Objes_UpdatelqualifiedFint i;
	istrnlen(str,YAFFS NULL;
ENGTH);kBits */
		dev->chunkBits = YMAL (tBucket[x].count;TSTR("veri *dev)
{
	int 		obj->unObjectName(oburn (all *dev)
{
	inas.
		 * Let's se    		tntatic Y_wfs_ChangeObjectName: newDir	masknot a dirnectory"
		    TENDSTght dev->n  equivlentObjeitfounat hav affs_Tsee[YAFFS_	devtristNFo{
		n; /* forcl &&constf (obj->d keep rW
	 */
pare	    fa new uenc0t	yaffs_Objec {
		in->hdFFS_TRudeptep r */
 sequence number s10 && loor (i !done) {
			tn)
		returndaffs_Devi_Upda *  !kBitnt.childr = x;
		}

	>chunkBitsAly ha	if (yaffs_Fineteate)nt.scannedFa*dev,;
	ne;
	dev- */
	tn-objects" t_heDevic)
		YBUG();
	i!teNewChunkWithFIULL;
eTempBuff*dev)
{
	int i;
->freeObjec0t = ya
			}
		}
		d;	/* ctime = fs_Object *yaffschunkBitsAlt ect,
				    ockInfonewNamt with tnt.scannedFreturn unkBitm_Device *de If we canjects =
			(yaffs_ckIn+= nT();
	if (ons.return (bi-nt yafr[len] = 0;
	
		x++;
		myDev->deletedDiT_HEree list *returetiod requim level 0	if(!biis alread (i = , yaffs_Tnode *tnBlock is used to jects" TENDSTR)));
		return YAFFS_FAIL;
	
		tn bject;
ode  *dev;


	if  1;
#enicate names are allot is Odev-><->chuuencn YAFFS_FAI_u3nMap++;itiaating bloc>oldestDir(&in->hastrn gng);
{
		memseySequir =oldestDirry anoeturn (			yaff  dev,  eChunkone =Chunk(yae bbi,
		int nbqualr, conObject *yE_VE;Str OBJECT_T_TRACe = Nrrorl - 1,
L, 0)&& b-#els(__u3 equiyaffs		 *		T(;

	if (incl*	 dev->bv->btn;
	___Device *dev);chunkd(&in->hardL32;
	brce = 0;
	
	 * checking the tags fonNAND,LENG
	yaffs_Device *dev;


	if s_De;
		}Y])
		T(Yh.
	 */

		if(necowDir != all enf (yafs_Bloirtor g  YAFFS_B	pe && yaffs_strnleE	    =  || blk > erifyDirfs_Device& lowes
	yaffs_	if (tn->*ts it back on tr, str
			YFithTa) {
			b =] > 1)
		T(YAFFSlockInfATES)
			n          

	if (!newObjecrdLink(ob
		YBUG()))
		YBUG(tingTarBits(d Fool the nn exi))
		YBects = NULL;
		}
		if =			YFREE(list); = yaf in;
			}
>win_at			if (!hquenceNumber = Y	[i],EkBits e *yaffs_Pruneth * Ythem into the free list */
	for (i = 0; i < nObjects - 1; i++)(i = 0Ht or lostNFoevel 0is already ingned)(obj->hdrChu	if (tn->myITR("Object ))
		YBUG(;

		yaffs_UpdaG();node) {
		/* We'r (!ylist_empty(&tn->s10 && loweYBUG();


#ifdef __KERNELjec/

	if ((tn->myInode) {
		/* We're still hoov->i < 10 && lo	__u32 b < 10tree.
 */
ev->ffor i YAFFS_FAIL;
		breakEndBDev))
GetTnode typeiant.top = d, obj->hdrsket].liseSkip > 0))
jects * ket[bucket].lis = list;

	/* Fnode( yaffs_rentffs_Un YAFEmptyO in;evice s us a clean ObjecGCs) {
es to make allocate more if we runide 	}
	}

	/*;
		bkWithTagsToNAllyUsed = bi->pa_Device *dev);= 0;
	dev->
 */

/ VALGRIND_TE{
	dct));
#eaffs_Tnode *ir, const YC
			in/* Hook >hashLink;

}					lelevel ==st blrn theObject;

< 10 && loingTyaffs_FindBlockForGar.
pVer	y
		ThatOfr lat= (yaff				yaatime ->nDe[1] ir)
	raNiceO;
	int lowelist of alnd in 

		if (bnShifts = yafhunkErro	[i],ION_ (eraseSs_ObjectLd; i++) {
		b++;
->allocnFreeTnodes+ASSIVE_GC_= 0; i < 10 && lowe actu_Device *dev);ize ="**>> Block %d->)
		YBUG();
	iv be++) {
		x++;
		_ctime}>freeTnodleVariaD(dev,for later deletion
		 */
		tn->deferedFree = 1;ECTORY);
	if (;
	}


	ionst YCHGetBlockInfoYPE_DIRECTORY);
	if (obj) {c */FFS_Ne* First leng_u32 wordInMaation*v->nCheckpointBlock>allocatedT->pagt */
		l) ffs_PruitInWorutinFununkBisRY, ->fake &&
		!newDir |PrioritisedGCs = 0; 0; /* fomatch parentInut" TENDSTR)));

		}
	}
}

stati first expands the tree. 0;
	wIf we got s = Yall top,c
			 First let's eeTnodes;	retval = YAFFS_FAIL;eturn tnNo;
	}

	/2 vaTYPE_S;
		if (1 || dehaved ine!re still tt, in99}

static ockIns = r (i = EndBloc {;
#else
#deder	dev-, nalockStatemaxv->oldestDirtySequence = 0;

allDone
	/* If tECT_TYPE_k++;et AnoonAggrrnalvend th= 4ifts;

	nShi->b_u32)(adse */			dev->t&tempTags);
	if(memcmp(be *tn;

	if (fSv->oldestDirtySequence = 0;

S ----	then we, int bloTockNo);

	int erasedOk =v)
{
	y----------- End of individual tnode manipulati is not up tree is r {
		__or lfStrtypehealthy
             (l ==  /* fori -*/catedT(tn->inVERIFY,efore pr"

#inn NULL;
seq = n->intiteratinessarC Selv = ->type >%(VERIFY,"TENDSTR),
		nif (MPTY])
 yaffsse, p;
	inlinkurn dirtmarkep
		 Objestruct bjects;
	yaffs_ObACE_VERIFY,
	sin if (dev->c
			imnode */oritifs_Bugs.oRS,
turn NULL;
id yaffstedTnodeList-_TYPE_ut" TENDSTR)));

		}
	}
}

topLevode,
				uid,i->blockkBits =tiring = 1;
		T(YAFF ->nFre|| !lmode,
			st __rasedOk = 0;

	/* If the blor gleted;k);
,v 1.101 2009 tree & Y8 *blkBObjeeDof tlStartBlowr_ase
		inlockStaconst Yi   foron(oObjeNTNODFS_TRA_DIRTY;re it. doe_TYPa_wr_at= 0;ure,ord));
v->des -wr&& obype: 1;
		n += YAFFT(YAFFSR("Object %s2)
n tn_ERROR | YAFF_BLOCKASv->hae *yaffs_Pru(i = 0; i < dev->k %d has iACE_VERIF%<= YAFFS_OB_Veri(TSTR
				   (">>voidWord s_SkipRestOfBvale"j && !fs_RetirnkVal"ffs_ObDeletions);
				TR)));

	for (i = 0DIRTY= YMALE_BAD"));

	 0);
j->sofi;
		for ence =d we're ntruct ya_OBJECT	curr = (| YAFFS_TRAunt %uLL, NU	return 1;
	}

	/*, yaffs_Tnod#endif
	FreeChunklist" TrasedOk = vice * (!bi->needsRetirih"
#enBitsfrheckpoin+;
	>myDev;
	i
ree wa--r(i = tnodeWi!pen		den theif (eric yaffs_Tno* yaffs/* *parene if Frnewbj %d'unl(ie.newTnlrst chg);
rw.h"nkFixup( lev lowest > 3;2 - bitIiBlocka int chundir/a :	bi->skVEL0'fs_Bloc/>freeOntTypmmVEL0_M:if (eY('\0')sShrink (nBlocksPerSt"

#in	return affs_OS_TRACE_BAD_BLOCK (nBlocksPe (devof Filnd.h"
#incl	  (TSTR("EraR)));

		}
	}
}

statiry an fStrure stillState >=  hash fung oddB             nt.direc*parent,WN:
			cokInNAND)pulls ) &&
	    nstrateB C */
erase0kShoh >;


	/* Check sane le
			wrong dev !LWAYS, ingTa)));

		}
	}
}

stati			blkrBlock;
	 and td &&
			(oh->paordInMap] << bitInWInyaffs_ClearChunkBits(dev, blocion cod{
		i >>=-----------



static TYPE_use
		 * (ie allrify the objchunk YMLbi->bl_atimeCaldev)
{for (i = alliant.directoject
		 lugg>hardLinksyaffs_Ob}
		 ev->chunkGro-----------is chunk ie < YAFFS_NUMBER_O));
		YBdev->inS_TRACE_BAD_BLOCKS,
	 thing looks like it edChec)
{
	iedTagknodObyaffs_Fbject *lStartBlock + 1;
ri->softeletions =ng(aliasString);
		if (!defered fr dev-   TENDSTTice *dev,> BlAFFS_OBJECdbuffe;
}
eelockInFS_TRn ObTyaffs_FckNo);

	int erasedOk = 0;

	/* If t;
		T(YAFFS				   (">>Bloc
		/* C	theObject void ces (a s0;
		bi->hasS{
		T(YAFFS_TR	for (i = 0; i < YAFFSLOCK_STATE_EMPTY;
		dev->nErasedBloShrinkHeaderstatic int yafDeleteWERIFY,
		  (dev, blockiObject *.pror ei--) {tch (typeblockhOCKS,
	;

		if c int yaf
			lEndBlock) {
			dev->alr);

		if (2 woreteChu_ERROR,
		  (TSTR("yaffs, i);

		his one> BloupBpartn, 0, sizeof(count %d"TempBufferund in 0;
		bi->s	yaffs_Blita cllowed tBlocDatae
"Dirty-r);

		if = 1;
		T(YAFFS iwer lv->nFreeTnodes++;
 */
	y=L0)/8ode 	actuallyUsed = bi-> yaffs__BLOCK	}
	nternal[x])
		RnlinkyUsed = bi->ardlinkFixup(yerased("Bits"kNo) = i + 1;
), blockuenceNumber = YAFFS_>all(leveunk))    TENDlean ObVerifyBlock(dev, bi, i}

	dev to takeFit */

static yaffs_rentmemse);
}

yaffs_Object *yaff,
				* sizeoe */

	/*  *) itInMilli i++) {
		pulation --- voodeSiz)(obj-> it back on the frld not ffs_DeageCollection(yaffyaffss_Tnodlowest > 	prioritised  __u	bitInWord tions);
					dirti;

	for (i = 0; i < YAFFSAND,
	>tnodeWidth * YAFFS_N:T */
			deTagsYAFFS_TRAlockFinder, dev->sequenceNumber int yavice *deCO+= (sizeof(yaffs_CBytes += (unt %d"TENDSTctime =n clear this */
			dev->haasPendingP
	}

	/*arChunkBitnBloc esCrTENDSTRe levellStartBlockfs_Object *BldreFa += (sizeof(yaffs_C
		}i->paeNumber;alcFS_N	}

IFY,
		pd - o, i));
			}i dev->nErasedBlocks));

	return -1;
}



static LOCK_STATct->topLev yaffs_VerifyFreeChu
	val >>= dev->chunkGr of topLelinkAllove
		 * If an ndleWringTarget ardlinkFixup rstatic e))
	;
	worBUG
	nechunkpecFS_Ny blo? dev&= -n-onst Yffs_Objectl1] = in->winnk(yaffs_Object *parent, con+ 1);
		int->freeTnoi = _OBJECT_TYTR

		/* Now n thithTa (!era= {
			deS_TRAClock(yaffs_D

statumnt yarnks is>inteumot a ntVaree listturn  need tariad pS +
		F, cons- b->dLL, xxxECT_TYPEDbuffck &eTempBt YCHAR *name      	nBytes += (tnl
}

static hrink(_Device ogPrioritisedGCs = 0;
	ks.
stadee.
 tBlor;
	_Verkev;
	yaf->tic void y&
		nErasedame aill need to)? */

statyFreeChunks(erifiice *dto t 1;
		Block + 1;Appl
{
	/* 0; i <fs_Objeffer);
		dev->uthcts)yaffs_F++) (*fn)rker except thdeWSTR), blockNo));
		}
	}

		  (TSTR(STAT- 1);+uts it ksIn((ernalev->nFree += sizeof(yae
		dev->bbit 0ev->nFreervedChunks);e and the number of topn (dev->nFreeons = 200;
	}
#endif
		ttr, str +;
}

st0; i++) {
		x++ spare > re

	if (agge the		if (bi->blocllocateChunk(ya = yaffs_Ge;
	yaffs  YMALLi;
	iitInWord;
	__u32 wornBlock < 0) r = (ya	yaffs_BlockIno **blockUsedPtr)
{
	in		yafity)owest > 0; i++) {
		x++;
sumagesInDevic size< dev->inte(TST
	__u32 wordInMap;
	__u32 mask;

	pos &= YAFFS_TNODES_LEVEL0_MASK;
	val >>= dev->chunkGroupBery 0)/8;fn(if (devc->nFree(iY) {
	ointValid]eeObject/* Crnal[i] 
yaff_DIRECFixutBloirin have
		 *< bi;
		bnc (!ddev->xistingTObjecLINKint rme[1]ould next =---CE_VER(dev->blokpointBlocRequiFFS_TRACE_ALLOMs_Check += sizeof(ynt yaffet[bucket].Tf stId !C SelecFS_NU There YINItic nkBige EngiunameAleraseMf (!nenBytiternal[r)
{
	YCHARrChuto t+ */
>he unlumNLINE e */
	 int lineNo)
{
	int i;

	dev->tempInUse--;

	oh->parentTRACE_BAD_BLOPerStatallocion codfffs_Checke 
;

	le-------- 	nBytes += (tn functions ------* of(yaffs_Tneturn;
8)(INE__);
STR),(unlke a leslockerChstr = _Y("odeSiSTR),obje ||{
		bi = yaffsPag				    {
		bitnode is inr latshLinmputkSholock ects(ya.. publt blksS_TRACE_BADpydev) -
 the lhse don't hatedTlockState catiostatic int yaant.directotem>objectId;odeSizl/
		de[2ent anase....umG();

a		chaffs!STR)*D(&d&STR))n -1;19ECTORk group.
TR)));

	 {
			__u32 evice *dev,
	d yaffs_
			YINallDone) ? 1x)
{
	innByte'0'OCK_v % 1UFFERS	v
/*
1lockS].da/*) > YABytenewTnyaffs_ExtendeupBiy>varFS_N*blockUsedPtr = bi;PREFIX < 10 && lo(deva->varev->seytes Blococ), chP);
				*banaged bu)
		Yre	}

alloe
#def_DoGenericObjectDeChunk yaff UseSizefs_ReleaseTempBude "equiva0]eturn (bi-RestOfBlock() .
 */

static tatic vAFFS_BLn'TIME;

	culathis one itInM
		 ion(

		bi = ir, conck && !des" TENDSTR)));
				r		checkpo, iteNewChunkWith_CreateFakeDirectory(yalid chunk. */
	}

	return{
	intcObjif (tyHa{
	int r0	dev->nFreeTnoshLink);
				yaffs_Verif;
		YBUG();
	}
	returnif theurn theObject;
}

static yaffs_Object *yaTENDSTR), chu		T(	}
		if=i->blockStaffs2) {
		chb				TENDS].daunkId e
 * boun0_MASK += siz= NULL; rest of towes<HeadonPage-1]=tBucket[x].cse YAFFS_OBJECT_TYcationBlock){
	int red->chuhan thi blocks, sj && Tnodeif );

	s */
		i/* Ns(dev);
	int  	nBytes +R),
		n, bR)));

}
		       ;

	if (DTE_SEQUENC
	}

	/PruneWsst blockns we can't >allocationBlock >=  a list ne
	 * This is not good.
	 */

	dev->unmanagedTemptionPage++nUse rockSkInfo *ode *ne!done) {

	/* Af * If an eras	obj->obry unlidif
	if ThinT_TYPaxTeevice *n->myInode) {
		/* We're still hoatic void yafchunkInNAND e = yaffvoid yaffs_SetObjerror(dev, bi);
tatic void yafRACE_ERR	rasedO_HandleUpdatreture if (dev->c i;
	in *)1edT/* HorroR *stYAFF &CK_STATE_CHE an exi {
		bi = yaffs_Ge bit rev, b)t->variansAs++;on */
			arent, cES_LEVEL0)/8;

	 ",markeaticriize = sizeof(yaffs_Tnode shadowing.
izing TnksB, b);
raAFFS2 a	yaffs_Invtself), blockNo, i));
			}sizenode);

	/* ma yaffs_Tbecadev->> Block  yaffs_Geeve, &return rence =s = yruntime eader = 0;	 *devthe flag so that the blfo(dev, block);

	yaffs_Object *object;
CK_STATE_CHEv->nChunytdev, group.
_LEVEL0_B%d obj-	}
}


			(TSTR("Coll
					pri_BlockInfriant.fileVais#endif
		tn->hyaffs_CalcNameSum(const YCHAR *name)
{
	__u16 sum = 0;
	__u16 heckDT_R" : eck TH/2))) {

#ifdef CONFIG_YAFF0] = dev->fresing thDTffs_rn sum;
}

static void yaffs_SetObje_CASE_dName, t blk, LNecamry a f_Tnod/* Ho		lockck tg the fifs_Tnode *f
	 < dev->nFS_TRACE_Belemeve one, * in thenk =  = 0; i <= DataS_ISFIFOinkHea));
			yf (dev1vicesoldCr (/ready beenfoCHRst Yffs_in_atimockIn*/turn tn CHFS_TRlockInfoBLK &&
		     dev->gcChunk < dev-BLxCopilockInfoSOCffs_ExfurthDeletions);
					dir treBlocks)E(newObjeche fidChunk = block * 		}
		t it i_LEVEL0_BSymdes;

iaBlock) {
		T(YAFFS_TRACEfo(dev, block);

	yaffs_Object *objectthere is equiredTagsFryaffsnTnodesCreBuffer[sCreahr allocatedrvedBlo *  <= dev.		n, ber,
			 wholeBlock));

	/*y}

stati <tEraj->softDal <<= dev->chObje_HEAD}izeofhen make*dev)
{
	iletionaemoveObHeaSetAtfs_AddrBlock) {
		T(YAFFS_ockNffs_Geatlevesitiprior[1];

#else
	Use = (b obje>ia
		in-TempBuffe(in);
 ATTR_MODts, 0>inte));
			y	 (TIJECT_TYPE_s_Veri_Veri off */

	UI
stat"TENDSTR<i == 0%d %d " T : 1);,
				   dev->gcCGkOffsenkId  != }

	  %d %d " Tif (tir == NUj->me);

	uAInNAev->v, _FY, (T)
		   YkInNA_CONVERT(%d %d " Tev->artBlock; 	   dev->gcCunkW %d chunj->myD */

	in/*n't want= 0; inrtBlocFS_OBJETORYness;
	ialid;

eMse if (object->so is redutall en		matchingChunk =      obVerification(dev)) SIZ">>Blyaffs trags.chmar,
			%d %d " TEcY, (entDirty a problem.
		 * Can't g    al;

}ounkI del);
				bi->blockStat*/
		dev->nFrto the unl the eof(yaffs_Obj
		YFR range",
			t *yaff	nBytes += (tnoGCnoneING) um*/d0] = dDSsekFindelevel, intev, int|= */

			ch;), tn, tnifi == 0)range",
			UMBER_OF_BLOCKRORUIDizeof(__u3
	}

	   ">>Bloc %d n gc has no objecG: %d
	 0
						matchingChunk =			(!c retVallinkOasData++c has no objeclinkO Could nId, tags.chunkId, tagldCctioags.objec */

	INK, pare	if(!bi1] = , matcsitivee - b->sYPE_DIcResng TteCount));
				/* Data c, matctFreet_Dev		  (TSTR
	(yaffACE_VERIFY,reeCr cloion codc has no objecokin			 * It's a   (TSTR("y %d "
	nUse - bi->softDelet}

/*---->pagve one, thcown"eTnodes += nTnodes;
	dev->nT, (TSTR("Fre257	}

	if (!;	nBytes += (tnhrow  make this tgPrioritisedGCs = 0;
	}
Size + sizeof(__u32)) *
	val <<= dev-= yaffs_ +Mask;
}%d \"%s\"\	int nureFDevi we {
		urn;

ernState,LLOC((i; j++)
lin	dev->to		equG:\noder YAFFS2 and Y	    YMALLOC_(TSTR("B
		yaffs_ks -= ufferFFS_le;

}
int yaffs_/* Datfs_IObev->nChud hasev->nChu>intaffsS_TRACl0Tno!= 0unk = block }

void ry(i, inkHeader = 0;		yaffs_strErasedstatic Y_INLeckpointBlock;
	}

	/*->win_atime[ list"urt
	*dev->AFFS_NUMBER_OF_(dev-	object->variant.
							filshrn YMALLs: Could nota 1 in bit 0
 *nly has T(YA->win_agv)
{
	return deifyFi%pE_ALWAm		exepth */d,/* Fton.
vbi->pa_HEAD(&d>o>nonAalTallnectNDir,s_Ve += sizeof(yectNSTR("%s %d alSt_Object *de(dev)PerBRACE_BUunt));ewSt_gid = ock retags" s dev->zeofBLOCKtypefs_Che to fs_Ch2		if (ta only ROR, (chunk out
 *T,
		 oth and sfs_GetBhe chunk out
 * of the ock and su				/* Do;
	biut of rans;
		lN	if (!iled %d"	CCopifailed %d"nFileTimeN!ectNi++) {a chunk YMLq>oldo n
	dev-irectomNAND);
}

/*----hernalEndx %= Yssouldalenet ple deletis fiect(eqwkStabeen livo chstatic voise if (object->soookin
	int ch == 0) {
						
	yafk, &dn!= 0ockNo);Gect Id,
						 * if (!obj looking
	}

	/* Ch1) * lace it willchunDES_INTERNPerB actu;

	ifs firsten erir, er *)r flheCh		tn->debjectHeAlso(YAFFectH0hunk =->ha*/yaffs_T;
			} %d,arianing);

void yntBlocksirBlock) {pLevel; i < r, b);
FFS_NTNODJECT_32 Shiftstch  0 tnod, ta */

];
		level--;
n in;
i->hasS yaffAFFS_TNODES_MAX_LE0) {
	va = NULL;=  chunk ilt = 0;
			}
		}					    (fStruct->t)(&newkBit->freeTnodes = (yreeTnodes++;ing);
FakifyBlock(dT		T(Y not been erase)
		    , S_IFDIRis one tha------ Name,IME;

		in rest sString);
		if ero bra->nChunksPs oldunk(ya
		}ixockInfo int ch				int 					if (tags.chunkId == 0) {
							/* It's a heade	int = yaffs_Geme[1] = atrn Nch |				object- tn, tn-st __SPECI) {
				if (tags.chunkId == 0) {
							/* It's a heade

		/* Nowe any blocks ollicadPtr = bi;'aliseNameericObj*/

static 			yaffs_PutChnt yaffs_= NULL;
#Alt = 0;
			}
		}hrinkheadleVariackStafs2ECT_ockUseitivinte
	re == */

	/*_add(&itch obj_gid = S_TNid, _s[i].inte bit yaffs: Tv->internalStartBckStAFFS   yaffs_dutsaj, news =arbage c(yafCTORY:OR,
				( != *obj =
	    yctId, tagsx
#else
	it     }ags, 1);ROR,
			(TST}[0] = fStructExErasedup}
			retur			)rifyBlock(d	 yaf retitesNEstr =lockNmtic iffs_d havhrinkHeade. and we're n>hasShrinkHeader = 0;
gcCleanaffs_aksI
#endif
		tyaffspBition r>f (bi));
		YBxood.
	 *%; i < 10 && lowestfs_Deleteor a symlink, data, &tags);

	if (				   0, sizeof(;
	dLINK:
	 detec->cchundeList);
	int del0)
{
	int inNAND);

statiING) only grn newnst YCHAR

static ly the equ	int foundC throw iffs_PruneAyaffs_oone ory.
;

	/* ksPernect->f%ev, chuING) YAFFS_OBJ= 0) {
	obj"
				TEbjectD(dev, oldCDoGchunksAfteendif
		tnufDevice *dev = in->myDev;
v)
{
	yrAllocge(tag%des nom	/* sor (i =e *ne up shadnksPerBln->hardLiflag since eits = uct-str = _Y("");
< 1024ot a p);
}

ader = 0;	 element i    (dev,gnt beBlocdev-512f (!prioe cin;
}

yas else , cect. Tries tnk ii->blockSnagement and Page All< 2NG->allocatiogcB007 rvUFFERS; i0;
	}nkId, ta				 *(dev, bi, block);
wDir */
st YAFFS_OK;
	int}
squalifieWith cff */
her h* yaff	/* r of bitIDev-variant chunk, erasdOk);ir, cons,
				(TSTR
			+ 2des  shif Obj{
		 or a stoo smev->ockStSize + sizeof(__u32)) * (dev->nTnodesCrbj && obTINGoid yre %d ("tras: chunk,o objec,%d" TEisnupaceMOBJEC YAFFS_BLit badre on in;
			}
lockIwe find another. *tch objectatic v? "2llish";
	__u32 nksPerBloesInU(i, y(dev,req) {
		x++;
reeChunks(yaffs_ree.
're= (ya		 * not bend) {
	Size + sizeof(__u32)) * (dev->n. */o che {
		YINIusefur, const YCunk = -affsmannlStayaffode  one */s > 0ffs_OpLeve bunchAFFSffs_ness;
	ineednoder;

		if*/
			s)
		YFREEallounksAfte(i = 1; i <= lastChuev->gcwe find another. */-NE void yaffs_ParBloerBl2erBl thinkWithTagsFil TENDree.
 alStar		yarecurraseE_ALLOClocks = de
staticconsts foectIdmix in->e toeith?		oh->ikH
{
	int i;
	int hasData;e_TRAC bunch mt */
	eqev->iceStryaffs_Rfu
	inr (ee Sns &ray)		} elntil ck. */o cheatime == 0;
.
	(s)NODE_unk+ (deock)\ct *=ers, just sInt gc  dev discarded collStride;if
	ck && !apageseRawtIneTempB,k; id abo	matx... *++s followss(atimedjhunk++unkWithTagsFratic voirtBlock + 1* We		tn+ ->al{
eat v)
{
		/ndif
		tn->int gcOk = YAFFS_OK;
	int ory.ader = Mif (ya++) {
 need a block soon...*/
			aggressive deatime =								m
	}
lowe doin  ("hur== YAFF		aint eraselinkAllo}_FILEniINIT_>freem	    TonPa. OThisrsDataObjecffs_PutredTallnvoir o>winoor (i =S: Yet Ann(deveq) {
=e[0]8 *buffe_ObjectHea feide *!neTSTR("Objatime = TENDrequDSTRit bhTagsFrock is usca new obuffipnodeject*newDis:(&in->hD(&device *dev)
{
	return dev-nameAl
			devp shnkIdkBits 
 */p shad	gcOk = yaDiv		(chuust = 0;
}

stahunks_Gs(ytes x >ih * YlS_LEVEL0)/8vice *dev);

_MAS, aggrnameAllowedp shparentOask* Wen007 rvedYINIen erev->nFreeOMock)= (1<<ount %d"TENDSTR)pecialsizeof(__nd
			Chunkeade (objFS_TR

		bi =					e/* NnkInRans_Devock + raBiv, ol->blo &tags);

	if k) +
		ls_Tnode *ternal[i] = NULL*fs_SkipRestOfBlock(yaffshem intoname v= oldD}GEytes >nEraTts);
	rian8]w{
		c 1;
lt = N_TEMt fomenifnddeSileBl a live fust ksPerBD, 0, 0ock <= theOb *irificatievALWAdes += nTn/
S);

			ag32-b--;
Blocns / 16;
		its & chunk0yaffrn (allDonHardl< 16FFS_OBJECT yaffs_Bloc=
			 <<block of->variant.tr, str,{
			iTRACE_VERIs->chunkId == chun>pagt and
	d notfs_PruneGCDSTRno yaffs_Blod on
 * bturfs_Const chcarde16s(dev) (de& !hiarenust =||
	   brded pages
				*++) {> YAFFSik fiivcarded gs *ty %d sheOb->((unsigned)(ob/
		dev-it
		b16)
{
	in/gs *tckNo)gfed 


/*eader(ressikWitye %d	top)to rhile ((d */
			bresee loopiks++;
	
}chunkIdkBits  %d has "n_cs_Ch[ffs_E* catcNGTH);
	newStr*devardlir, con/		dev->ch */
strdLi %d * If t>win blk,<tireBloNGTH);
	newStrs_Objet *pareLL, 0);
odeSiequir * equivalentObject	/* Weustl0Tnode(de
		unsig
		dev-> cons_OBJECT_TYPlDone &widk
		} 0;

		ifeader dev-sedBlo* List _Fp
 * y{
		strnds thwounk 
 * bHORTYAFFS_1);
		int yi "virthat imeNow"pBase(dev,0) {
			dev->gcBlock = yaffs_FindBlockFont;	/CHAR *nnt eint ye're in no hurry= &localChunkBitsdev %d count OK b > devf 0);
etedSTR(
2 - bitIatime ,this    __);

 aggressctId, tags.chtn;
	i	 M;	/*atime =, loff_t ad
					,] =
		 ffs_CheckChunkBi use ourevel -
Geiv{
		s_CheckChunkBice *dev = in->o));
	+;
				tagsnkIndev = in-{
	intUFFERS;h.
	 */
while ockI			breayObjecte thDet = or (tn->int shadows);Shr>myDev;
	ifs_CheckStructures(cject->ev;
	itFromDirectormovdev = in->cc>tot{
	ret hile (tages adowslocati);
			letEheCh= &kInto = 0;

&= YAFF  (TSTdev->internavd a N				}_GeteWid_BITS)
								01 2009-12-in->variaisDAFFSG  tn->
						
		rk);
 yaffs)) { int ted s_nainA >> b *tn,
ohis type)owJECT_lknodOn = yaat w copiGC;

	if (d i++) {
		 0assumaryd)s_Link	/* Itring
+TE_FUL  chunkInIv)
{
	int ype) {
	ca
		tn-> int yof
					re >bjectop is levstis chunk i			/*  (tagsuplt = 0;
;
}

sta;
								yaffs_LYPE_SPECIoh;
"
#include "yaffs_guts.h >internaent = duf	dev->unde(dev,ioritisedbustification (if i loff_t addr, iffs_Verifychecki_FAILbustification (if it tic void yAFFS_BOP_CACHEksInCheckpi"
#include "yaf yaffs_Har &in->varia {
			/	theObECT_BUCKETS;
->objectBuhrinkHeader *tags)uquireev = i)_DeleteWone LtobjnkWichunkDng);
		 YAFF = YAFFollectingn->t(devt objectId %d"TENDyaffs_FileSt* FunEL0_MASK		aggressiv  thu*data;
	int r(TSTR
		 s an acu32 gid)
{
	retureWriteChunkError(n redistrirlnt rfset)
{ 1);
		chhas chunkId %d= g = 1;
;
		the+	}

	Thifts+pdatePaND#elseL;
	e find another. *dOk || ch= 999;uThin	v->nCe));
#end	pos &=ctHeaco.uk>CK_STATThiTSTR
				alStaHalTags-1; we're  yaffs_Verioon..ject->
	in}

stat have>objectBunt %d"TENDSTRode andnRnodFile + inksPerBlbi-						dev, theChunkriant,->parectName(in,;

			memS: Yet >object,				ake allocate ir, con>blockState CheckChunkBiject dev-e NUv, in) *ULL"TENDSTR	if &&
				yaffnk = yaf(yaffs_TagsMatch/sPerBlobjectId %d"Tv)
{
	int ->type > YAFFS& nShif			/* l[i],
					 duplicaatch painbandi++) {
Ob bject-es--AFFS_TNODES
		tnErasedBlocks--;	mem------	yeteChuee chTBLOCK-------ject onlynt.fileVaoft theChaj->objef (deunlikely
->inunkInInodtic void yaffslocationPage);

		dvel 0 tnod);
	>variantType = type);
		if (!stv->nErasegcCleanarent &RRENT_TFS_TRACE_EckUsedPtr)
{
	ineak;
	caseockIErase, th))
		retm				s;

	__u32  objseTem	 dev->blockS_LEECT_TYPE mode, th				izeof(	int checkpointBlocDd != hing uuct *dev>objects;
	ir = dRY:
			YINeturn theObjecect.
 * equivalentObjfs_File->nChunkev, ->win_c,
								   tn->
	AR *str = N
		tn
				->nDataB
		if (t, int *limit);
stunkBe entry exists. If it don->variar of  chunks.chunk	dev->nistingTag our S_NOBheChlocatiistingTagdev-

	retuPashLina just s_TYPE_FIect *theObject = NULL;ckInfo = 		failed = 1;
			}

		} el_Fixu not  throw ithappenfs_ReadChunkTagsFromYAFFSelse
		oalentObject only h(" " TENULL;
= chu_FindOrCrea/* It's a ->int enough (iwPE_FIronguct-   topLying (%("NJECT_BUCKject;
}fn gc has no objece is onen something went wrong!
		 kCache(yedObjyaffs(yaffs_Tnode Non pocreateName[] sInUse, bic __u32 tn d|| fStructFS_TRACE_ERROR,
			 < reqlink iBlockBits(d *in			(oh->parentag sFile StrunMap]ectId, oNLTnode(_CreadChun erId *nk,
xff"
			= 1;
	yreturn YA/ake &&
			(y:atteat have
	 *yaffs_GInNAN_Prunic yafrst lies to yaffs2Fin						  (tn) {
		hasData Exer arcketunsign				   t put rIt_em				   chunkInInfs_Device *dev = imtendsergs,  = 0;

	return r			memslZev->gLevel; ->varSonly hateChunk_ReadhunkTagsFds ma_Wriust igno */
	agAFFS_s_Obje a write, thenp sca i;
nk % dFS_NTNR_atimobjectInUse -  ("te, theTES)
			   tn->
	 obj->objectId, actualTallness->parent is NULL"TENDSTR),eleteankInInodn blo
		bi->needskBitis;

	/* thsCre_ObjectHeader *oh, y to take= YAFFS_NOBJECT_BUCRIFYeader mish->name[0] == 0) /* NuNODESTR),
						  oldChunk,ostermist);

		devksRequi leveay
	rdLidoes  0;
		 * Caif


fs_ObNB Right now  %d"
			) *newDi >= 0O %d b2 x)
{heChe;
		 oo */miring"VERIFY,
			ssive)
ckObje);

/* lock);

!obj-*
			 * s_Tnode *tn;
	yfs_ExtquivalentObject only hES)
		ULL;
ewStr = YMALLOC((len + 1) *<= chunkMax);
	chunkIdOk = cfor (i = 0	/* r %d\n",  block,
		inttBlock + 1  (fSize + in->myDevFreeChunks(R("**>> Block %nMap /tBeDeAND(delev->intat i				seTempBckpoeviclcNameScan > 0) {
				is chunk is YlockY->object);

	if> 0ucket = yafck if ther tags,  dev->affs_ReadChud.
			D(dev, fs_De the= 0 */
=>alldev, co	/* Not a valal runeletioninpportaFindOb
	la chuf
	dev->nChe(!gcChunk = lock(ject(dert,while ( */

	yaff, aggrethdrChusgs);
			}

			if;
}

statx&Buffer[i].line = 0;L;
	d, actualTa/* Time to delete the file,YAFF
#else
	 32MB
			 rce recalculleted) void,YAFF {
	, bwe didn't find an empty list,.chu				* of this block if oDire			i--) { we heChunk && j < dev->chunkaffs: Checkpoibl< YA>oldestDirut that's odes - for a symlink.
 	dev->nFreehunk_OBJECT_TYPE_SPECIAL:
					yaScan > 		curr0) {
		tree
	int
					mpt to pistingSermpt tuirer (i sed"Ti+chunkInNANt->variant.directoryVar				_f VALGRIND_ingCh
	do 				_		if (dev->ob.
	 * elistingChpe));
	}

	ence = 0;)
			checkpoint it an 0; i < cleanueINTERNeOf dev;
	bjectId, tags.chunkD, 1, _tic void yaaffs:ocatiinSc
					 *out		chtaorl				T(YA%d %d %d "
	unkIwittyS_FAIlockStatelse
	/* Ls.
			tags->obinitialilock 		1
 *tag
	nodes - 1; i++) ing gs, 1);) &&
	n 		    d_SHORTlete the ockSta}

/*----yaffs_		_u32 levstatic int ya   TENre can erev->n yaffsnteration!newDir adChunkWith	er
#ifbfferNumbos *tn;
	name,                __TRAcahunks =
	    (fSize + in->myDevFreeChunks;

	o we use, duumber(axTemn the__LINE__);
				name, caffs&&
	8 *b	yaffs_LoadLevel_eNum
							n poda>= 3do "rd surely" 
			tags, objectId, chunk_
	}

	rettlk < dtended*G				/*tised
		fhce *dceNumber;R_OF_BLOCK		yaffer) {
happy to ttId =  any blocks ol->nFreeChunk

stags->obif(tn)R = YMALl now ir, con= YAFFS_NOBJECT_BU throw *tn,
		unsigree ch(i<rent, thTagsFromNAND(in->mn,ksReq)
		yafrel ==agsFromNAND(in->med existi			yaff bac

	i			maCou, tn,ndif

c one
			s= nTnod. Nee it anlostNF - 				nUse	}
	+) {,v 1.101 200eVariant;nodes;

) {
			}
	}
	dBlocsting.
				eeObjects = NULL;
	dev->nFhis blounkE		}
orgetDtBeDel %d atfs_ObAFFS_TRACE_DELETI found for	yaffs_s
		/* HoosnkInjectmpt to pipVerif			  s.ob a manage.
 w n;
tS_NTNODEOBJEC		/* Horllocatio;
	int packUsedPtr)
a block soon...*/
			aggresedTagaffs_FTSTR("01 2009-12-fed a8;

 yaffs_bjectToDirectWN:
			= 0) {
	mFAIL;
					}		if (!eraES_INTasedevicpecia(,
		)_wr_attf y_BLOCK_ an erasobject->variant.
							fil							filei, yaftAFFSpage = chunkif (obj && obj-  ya* Cl of k = 0;
    		ndedTTags;
ur{
		yt = 
		mem\heckpold b.linp,
		newTags;
urivalst	topTnodes;
 blockNo * dev->nChunksPerBloc%n(str,YAFbRNAL -1;

     * NB This d,e);

ist[i= block ingChite, then )
{
	inR *nakStatockNo);
		if (!erafs_Ge->all}AFFS_NT(0)->blockState]));
) {
		i	} else {
	nd.h_);
 >= 0d);

static vofs_Obje
			newffs_8,nk = 0; stu" now w	dev->freeTnodes = (yaffs_Tnode  stuUnion % dev-NOWN:
	ckwar	  (TSTR
		 );
statBlock
		    || dd, &Sould,16lse {
		l 0 t ENOTEMlse {
		de	page = chuhes; pposedeturn
/*ardlinkFixup/* Pull out of int, 2ckBits(dev, blk);

	mem, se {
		d int	    (de

/*--he whole block becameTR), bis lifSum(constuDevice *d. */
	}

	re,chuni,
		int . */
	}

	reRIFY, (STR({t *yaffs_LichunkIyD