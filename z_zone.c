/* Z_zone.c */

#include "doomdef.h"

/* 
============================================================================== 
 
						ZONE MEMORY ALLOCATION 
 
There is never any space between memblocks, and there will never be two
contiguous free memblocks.

The rover can be left pointing at a non-empty block

It is of no value to free a cachable block, because it will get overwritten
automatically if needed

============================================================================== 
*/ 
 
memzone_t	*mainzone;
memzone_t	*refzone;

 
/*
========================
=
= Z_InitZone
=
========================
*/

memzone_t *Z_InitZone (byte *base, int size)
{
	memzone_t *zone;
	
	zone = (memzone_t *)base;

	D_memset(zone, 0, size);
	
	zone->size = size;
	zone->rover = &zone->blocklist;
	zone->blocklist.size = size - 8;
	zone->blocklist.tag = 0;
	zone->blocklist.id = ZONEID;
	zone->blocklist.next = NULL;
	zone->blocklist.prev = NULL;
#ifndef MARS
	zone->blocklist.lockframe = -1;
#endif
	return zone;
}

/*
========================
=
= Z_Init
=
========================
*/

void Z_Init (void)
{
	byte	*mem;
	int		size;

	mem = I_ZoneBase (&size);
	
#ifdef MARS
/* mars doesn't have a refzone */
	mainzone = Z_InitZone (mem,size);
#else
	mainzone = Z_InitZone (mem,0x80000);
	refzone = Z_InitZone (mem+0x80000,size-0x80000);
#endif
}


/*
========================
=
= Z_Free2
=
========================
*/

void Z_Free2 (memzone_t *memzone, void *ptr, const char *file, int line)
{
	memblock_t	*block, *adj;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		I_Error ("Z_Free: freed a pointer without ZONEID");

	block->tag = 0;
	block->id = 0;

#ifdef MEMDEBUG
	if (memzone == mainzone)
		CONS_Printf("Freeing block of size %d\n%s: %d", block->size, file, line);
#endif

	// merge with adjacent blocks
	adj = block->prev;
	if (adj && !adj->tag)
	{
		adj->next = block->next;
#ifdef MEMDEBUG
		if (memzone != mainzone && adj->next == NULL)
		{
			CONS_Printf("Null adj->next for block size %d", block->size);
		}
#endif
		adj->next->prev = adj; // If adj->next is null and not mainzone...
		adj->size += block->size;
		if (memzone->rover == block)
			memzone->rover = adj;
		block = adj;
	}

	adj = block->next;
	if (adj && !adj->tag)
	{
		block->next = adj->next;
		if (block->next)
			block->next->prev = block;
		block->size += adj->size;
		if (memzone->rover == adj)
			memzone->rover = block;
	}
}

 
/*
========================
=
= Z_Malloc2
=
========================
*/

int Z_CalculateAllocSize(int datasize)
{
	int allocsize = datasize+1;
	allocsize += sizeof(memblock_t);	/* account for size of block header */
	allocsize = (allocsize+3)&~3;		/* longword align everything */

	return allocsize;
}

#define MINFRAGMENT	64

#ifdef MEMDEBUG
void *Z_Malloc2 (memzone_t *memzone, int size, int tag, boolean err, const char *file, int line)
#else
void *Z_Malloc2 (memzone_t *memzone, int size, int tag, boolean err)
#endif
{
	int		extra;
	memblock_t	*start, *rover, *new, *base;

#ifdef MEMDEBUG
	if (memzone == mainzone)
		CONS_Printf("Had to malloc %d from %s:%d", size, file, line);
#endif

#if 0
Z_CheckHeap (memzone);	/* DEBUG */
#endif

/* */
/* scan through the block list looking for the first free block */
/* of sufficient size, throwing out any purgable blocks along the way */
/* */
	size += sizeof(memblock_t);	/* account for size of block header */
	size = (size+3)&~3;			/* longword align everything */
	
	start = base = memzone->rover;
	
	while (base->tag || base->size < size)
	{
		if (base->tag)
			rover = base;
		else
			rover = base->next;
			
		if (!rover)
			goto backtostart;
		
		if (rover->tag)
		{
		/* hit an in use block, so move base past it */
			base = rover->next;
			if (!base)
			{
backtostart:
				base = &memzone->blocklist;
			}
			
			if (base == start)	/* scaned all the way around the list */
			{
#if MEMDEBUG
				if (err)
					I_Error("Z_Malloc: failed on %i (LFB:%i)\n%s:%d", size, Z_LargestFreeBlock(memzone), file, line);
#else
				if (err)
					I_Error("Z_Malloc: failed on %i (LFB:%i)", size, Z_LargestFreeBlock(memzone));
#endif
				return NULL;
			}
			continue;
		}
	}
	
/* */
/* found a block big enough */
/* */
	extra = base->size - size;
	if (extra >  MINFRAGMENT)
	{	/* there will be a free fragment after the allocated block */
		new = (memblock_t *) ((byte *)base + size );
		new->size = extra;
		new->tag = 0;		/* free block */
		new->prev = base;
		new->next = base->next;
		if (new->next)
			new->next->prev = new;
		base->next = new;
		base->size = size;
	}
	
//#ifdef MEMDEBUG
//	D_strncpy(base->file, file, 16);
//	base->line = line;
//#endif
	base->tag = tag;
	base->id = ZONEID;
#ifndef MARS
	base->lockframe = -1;
#endif	
	memzone->rover = base->next;	/* next allocation will start looking here */
	if (!memzone->rover)
		memzone->rover = &memzone->blocklist;
		
	return (void *) ((byte *)base + sizeof(memblock_t));
}

#ifdef MEMDEBUG
void *Z_Calloc2 (memzone_t *memzone, int size, int tag, boolean err, const char *file, int line)
{
	void *p = Z_Malloc2(memzone, size, tag, err, file, line);
	D_memset(p, 0, size);
	return p;
}
#else
void *Z_Calloc2 (memzone_t *memzone, int size, int tag, boolean err)
{
	void *p = Z_Malloc2(memzone, size, tag, err);
	D_memset(p, 0, size);
	return p;
}
#endif

/*
========================
=
= Z_FreeTags
=
========================
*/

void Z_FreeTags (memzone_t *memzone)
{
	memblock_t	*block, *next;
	
	for (block = &memzone->blocklist ; block ; block = next)
	{
		next = block->next;		/* get link before freeing */
		if (!block->tag)
			continue;			/* free block */
		if (block->tag == PU_LEVEL || block->tag == PU_LEVSPEC)
			Z_Free2 (memzone, (byte *)block+sizeof(memblock_t), __FILE__, __LINE__);
	}
}


/*
========================
=
= Z_CheckHeap
=
========================
*/

memblock_t	*checkblock;

void Z_CheckHeap (memzone_t *memzone)
{
	
	for (checkblock = &memzone->blocklist ; checkblock; checkblock = checkblock->next)
	{
		if (!checkblock->next)
		{
			if ((byte *)checkblock + checkblock->size - (byte *)memzone
			!= memzone->size)
				I_Error ("Z_CheckHeap: zone size changed\n");
			continue;
		}
		
		if ( (byte *)checkblock + checkblock->size != (byte *)checkblock->next)
			I_Error ("Z_CheckHeap: block size does not touch the next block\n");
		if ( checkblock->next->prev != checkblock)
			I_Error ("Z_CheckHeap: next block doesn't have proper back link\n");
	}
}

#ifdef MEMDEBUG
/*void Z_DumpHeap(memzone_t *memzone, int skipCount)
{
	char memmap[2048];
	memmap[0] = '\0';
	char *mapPtr = memmap;
	int numblocks = 0;

	int i = 0;

	memblock_t *block;
	for (block = &memzone->blocklist; block; block = block->next)
	{
		if (i < skipCount)
		{
			i++;
			continue;
		}
		char appendMe[32];

		if (block->tag)
			D_snprintf(appendMe, 32, "%s:%d:%d", block->file, block->line, block->size);
		else
			D_snprintf(appendMe, 32, "Free:%d", block->size);

		for (int i = 0; i < 32; i++)
		{
			if (appendMe[i] == '\0')
				break;
			*mapPtr++ = appendMe[i];
		}
		*mapPtr++ = '\n';
		numblocks++;
		i++;
	}

	*mapPtr++ = '\0';
	I_Error("%d blocks:\n%s", numblocks, memmap);
}*/
#endif

/*
========================
=
= Z_ChangeTag
=
========================
*/

void Z_ChangeTag (void *ptr, int tag)
{
	memblock_t	*block;
	
	block = (memblock_t *) ( (byte *)ptr - sizeof(memblock_t));
	if (block->id != ZONEID)
		I_Error ("Z_ChangeTag: freed a pointer without ZONEID");
	block->tag = tag;
}


/*
========================
=
= Z_FreeMemory
=
========================
*/

int Z_FreeMemory (memzone_t *memzone)
{
	memblock_t	*block;
	int			free;
	
	free = 0;
	for (block = &memzone->blocklist ; block ; block = block->next)
		if (!block->tag)
			free += block->size;
	return free;
}

/*
========================
=
= Z_LargestFreeBlock
=
========================
*/
int Z_LargestFreeBlock(memzone_t *memzone)
{
	memblock_t	*block;
	int			free;
	
	free = 0;
	for (block = &memzone->blocklist ; block ; block = block->next)
		if (!block->tag)
			if (block->size > free) free = block->size;
	return free;
}

/*
========================
=
= Z_ForEachBlock
=
========================
*/
void Z_ForEachBlock(memzone_t *memzone, memblockcall_t cb, void *p)
{
	memblock_t	*block, *next;

	for (block = &memzone->blocklist ; block ; block = next)
	{
		next = block->next;
		if (block->tag)
			cb((byte *)block + sizeof(memblock_t), p);
	}
}

/*
========================
=
= Z_FreeBlocks
=
========================
*/
int Z_FreeBlocks(memzone_t* memzone)
{
	int total = 0;
	memblock_t* block, * next;

	for (block = &memzone->blocklist; block; block = next)
	{
		next = block->next;
		if (!block->tag)
			total += block->size;
	}
	return total;
}

/*
========================
=
= Z_DumpHeap
=
========================
*/

#ifdef NeXT

void Z_DumpHeap (memzone_t *memzone)
{
	memblock_t	*block;
	
	printf ("zone size: %i  location: %p\n",memzone->size,memzone);
	
	for (block = &memzone->blocklist ; block ; block = block->next)
	{
		printf ("block:%p    size:%7i    tag:%3i    frame:%i\n",
			block, block->size, block->tag,block->lockframe);
		if (!block->next)
			continue;
			
		if ( (byte *)block + block->size != (byte *)block->next)
			printf ("ERROR: block size does not touch the next block\n");
		if ( block->next->prev != block)
			printf ("ERROR: next block doesn't have proper back link\n");
	}
}

#endif

