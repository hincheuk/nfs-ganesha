// ----------------------------------------------------------------------------
// Copyright IBM Corp. 2010, 2011
// All Rights Reserved
// ----------------------------------------------------------------------------
// ----------------------------------------------------------------------------
// Filename:    pt_util_cache.c
// Description: Define common macros and data types for caching purpose
// Author:      FSI IPC team
// ----------------------------------------------------------------------------

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include "fsi_ipc_ccl.h"
#include "pt_util_cache.h"

static void fsi_cache_32Bytes_rawDump(fsi_ipc_trace_level loglevel, void *data, int index);
// ----------------------------------------------------------------------------
// Initialize the cache table and setup memory
//
// Return: FSI_IPC_OK = success
//         otherwise  = failure
int fsi_cache_table_init(CACHE_TABLE_T *cacheTableToInit,
                         CACHE_TABLE_INIT_PARAM *cacheTableInitParam)
{
  // Validate the input parameters.
  if ((cacheTableToInit == NULL) ||
      (cacheTableInitParam == NULL) ||
      (cacheTableInitParam->keyLengthInBytes == 0) ||
      (cacheTableInitParam->keyLengthInBytes >= CACHE_ENTRY_KEY_MAX_LENGTH) ||
      (cacheTableInitParam->dataSizeInBytes == 0) ||
      (cacheTableInitParam->dataSizeInBytes >= CACHE_ENTRY_DATA_MAX_LENGTH) ||
      (cacheTableInitParam->maxNumOfCacheEntries == 0) ||
      (cacheTableInitParam->cacheKeyComprefn == NULL) ||
      (cacheTableInitParam->cachePrintKeysfn == NULL) ||
      (cacheTableInitParam->cacheTableID ==0))
  {
    FSI_TRACE(FSI_ERR, "Failed to initialize ");
    return -1;
  }

  // Populate the cache table meta data
  memset (cacheTableToInit, 0x00, sizeof (CACHE_TABLE_T));
  cacheTableToInit->cacheEntries =
      malloc (sizeof(CACHE_TABLE_ENTRY_T) * cacheTableInitParam->maxNumOfCacheEntries);

  if (cacheTableToInit->cacheEntries == NULL) {
    FSI_TRACE(FSI_ERR, "Unable to allocate memory for cache table"
              " (cache id = %d", cacheTableInitParam->cacheTableID);
    return -1;
  }

  cacheTableToInit->cacheMetaData.keyLengthInBytes =
      cacheTableInitParam->keyLengthInBytes;
  cacheTableToInit->cacheMetaData.dataSizeInBytes =
      cacheTableInitParam->dataSizeInBytes;
  cacheTableToInit->cacheMetaData.maxNumOfCacheEntries =
      cacheTableInitParam->maxNumOfCacheEntries;
  cacheTableToInit->cacheMetaData.cacheKeyComprefn =
      cacheTableInitParam->cacheKeyComprefn;
  cacheTableToInit->cacheMetaData.cachePrintKeysfn =
  		cacheTableInitParam->cachePrintKeysfn;
  cacheTableToInit->cacheMetaData.cacheTableID =
      cacheTableInitParam->cacheTableID;

  return FSI_IPC_EOK;
}

// ----------------------------------------------------------------------------
// Compare two key entry and indicates the order of those two entries
// This is intended for the use of binary search and binary insertion routine
// used in this cache utilities
//
// Return:  1 if string1 >  string2
//          0 if string1 == string2
//         -1 if string1 <  string2
//
// Sample:
//    int fsi_cache_keyCompare(const void *cacheEntry1, const void *cacheEntry2)
//    {
//      CACHE_TABLE_ENTRY_T *entry1 = (CACHE_TABLE_ENTRY_T *) cacheEntry1;
//      CACHE_TABLE_ENTRY_T *entry2 = (CACHE_TABLE_ENTRY_T *) cacheEntry2;
//      uint64_t num1 = *((uint64_t *) entry1->key);
//      uint64_t num2 = *((uint64_t *) entry2->key);
//
//      if (num1 < num2)
//        return -1;
//      else if (num1 > num2)
//        return 1;
//      else
//        return 0;
//    }
int fsi_cache_handle2name_keyCompare(void *cacheEntry1, void *cacheEntry2)
{
  uint64_t *num1 = (uint64_t *) cacheEntry1;
  uint64_t *num2 = (uint64_t *) cacheEntry2;
  int i;

  FSI_TRACE(FSI_INFO, "Comparing two keys");
  fsi_cache_32Bytes_rawDump(FSI_INFO, cacheEntry1, 0);
  fsi_cache_32Bytes_rawDump(FSI_INFO, cacheEntry2, 0);

  for (i=0; i<4; i++)
  {
    if (num1[i] < num2[i]) {
      FSI_TRACE(FSI_INFO, "Comparison exited at i=%d num1[0x%lx] < num2[0x%lx]",i, num1[i],num2[i]);
      return -1;
    }
    else if (num1[i] > num2[i]) {
      FSI_TRACE(FSI_INFO, "Comparison exited at i=%d num1[0x%lx] > num2[0x%lx]",i, num1[i],num2[i]);
      return 1;
    }
  }

  FSI_TRACE(FSI_INFO, "All matched");
  return 0;
}


// ----------------------------------------------------------------------------
// This routine will perform a binary search and identify where an cache should
// be placed in the cache table such that the resulting will still remain in
// proper sorted order.
//
// Return 0 if existing entry
// Return 1 if insertion point found
int fsi_cache_getInsertionPoint(CACHE_TABLE_T         *cacheTable,
                                char                  *key,
                                int                   *whereToInsert)
{
  int first, last, mid = 0;
  int found = 0;
  int compareRC = 0;
  *whereToInsert = 0;
  first = 0;
  last = cacheTable->cacheMetaData.numElementsOccupied - 1;

  while ((!found) && (first <= last)) {
    mid = first + (last - first)/2;
    compareRC = cacheTable->cacheMetaData.cacheKeyComprefn(key, cacheTable->cacheEntries[mid].key);
    FSI_TRACE (FSI_INFO, "compareRC = %d, first %d mid %d, last %d\n",
               compareRC, first, mid, last);
    if (compareRC == 0) {
      return 0; // existing entry
    } else if (compareRC < 0 ) {
      *whereToInsert = mid;
      last = mid - 1;
    } else if (compareRC > 0) {
      *whereToInsert = mid+1;
      first = mid + 1;
    }
  }
  return 1;
}

// ----------------------------------------------------------------------------
// Insert and entry to the array at the correct location in order
// to keep the correct order.
//
// Return: FSI_IPC_OK = success
//         otherwise  = failure
int fsi_cache_insertEntry(CACHE_TABLE_T *cacheTable, void *key, void *data)
{
  int rc;
  int whereToInsert;

  if ((cacheTable == NULL) ||
      (key == NULL) ||
      (data == NULL)) {
    FSI_TRACE (FSI_ERR, "param check");
    return -1;
  }

  // Log result of the insert
  FSI_TRACE(FSI_INFO, "Inserting the following handle:");
  fsi_cache_32Bytes_rawDump(FSI_INFO, key, 0);

  if (cacheTable->cacheMetaData.numElementsOccupied == cacheTable->cacheMetaData.maxNumOfCacheEntries) {
    FSI_TRACE (FSI_ERR, "Cache table is full.  Cache ID = %d", cacheTable->cacheMetaData.cacheTableID);
    return -1;
  }

  CACHE_TABLE_KEY_DUMP(cacheTable, "Dumping cache table keys before insertion:");

  rc = fsi_cache_getInsertionPoint(cacheTable, key, &whereToInsert);

  if (rc == 0) {
    FSI_TRACE (FSI_WARNING, "Duplicated entry");
    // Log result of the insert
    FSI_TRACE(FSI_WARNING, "Attempted to insert the following handle:");
    fsi_cache_32Bytes_rawDump(FSI_WARNING, key, 0);
    CACHE_TABLE_KEY_DUMP(cacheTable, "Dumping cache table keys currently:");
    return -1;
  }

  // Insert the element to the array
  memmove (&cacheTable->cacheEntries[whereToInsert+1],
           &cacheTable->cacheEntries[whereToInsert],
           (cacheTable->cacheMetaData.numElementsOccupied - whereToInsert) * sizeof(CACHE_TABLE_ENTRY_T));

/*  ptr = (void *) malloc(cacheTable->cacheMetaData.keyLengthInBytes);
  if (ptr == NULL) {
    FSI_TRACE (FSI_ERR, "Failed allocate memory for inserting key");
    return -1;
  }
  cacheTable->cacheEntries[whereToInsert].key = ptr;

  ptr = (void *) malloc(cacheTable->cacheMetaData.dataSizeInBytes);
  if (ptr == NULL) {
    free (cacheTable->cacheEntries[whereToInsert].key);
    FSI_TRACE (FSI_ERR, "Failed allocate memory for inserting data");
    return -1;
  }
  cacheTable->cacheEntries[whereToInsert].data = ptr;
*/
  memcpy (cacheTable->cacheEntries[whereToInsert].key,
          key,
          cacheTable->cacheMetaData.keyLengthInBytes);
  memcpy (cacheTable->cacheEntries[whereToInsert].data,
          data,
          cacheTable->cacheMetaData.dataSizeInBytes);

  cacheTable->cacheMetaData.numElementsOccupied++;

  CACHE_TABLE_KEY_DUMP(cacheTable, "Dumping cache table keys after insertion:");

  return FSI_IPC_EOK;
}

// ----------------------------------------------------------------------------
// Delete an entry in the array at the correct location in order
// to keep the correct order.
//
// Return: FSI_IPC_OK = success
//         otherwise  = failure
int fsi_cache_deleteEntry(CACHE_TABLE_T *cacheTable, char *key)
{
  CACHE_TABLE_ENTRY_T *entryMatched = NULL;
  int whereToDeleteIdx = 0;

  // Validate parameter
  if ((cacheTable == NULL) ||
      (key == NULL)) {
    FSI_TRACE(FSI_ERR, "Param check");
    return -1;
  }

  // Log result of the delete
  FSI_TRACE(FSI_INFO, "Deleting the following handle:");
  fsi_cache_32Bytes_rawDump(FSI_INFO, key, 0);

  if (cacheTable->cacheMetaData.numElementsOccupied <= 0) {
    FSI_TRACE(FSI_ERR, "Cache is empty.  Skipping delete entry." );
    return -1;
  }

  CACHE_TABLE_KEY_DUMP(cacheTable,"Dumping cache table keys before deletion:");

  entryMatched = bsearch(key,
                         &cacheTable->cacheEntries[0],
                         cacheTable->cacheMetaData.numElementsOccupied,
                         sizeof(CACHE_TABLE_ENTRY_T),
                         cacheTable->cacheMetaData.cacheKeyComprefn);

  if (entryMatched == NULL) {
    FSI_TRACE(FSI_INFO, "No match for delete");
    return -1;
  }


  whereToDeleteIdx = entryMatched - cacheTable->cacheEntries;
  FSI_TRACE(FSI_DEBUG, "whereToDeleteIdx = %d", whereToDeleteIdx);

  // Now we have a match
  // Deleting the cache entry
/*
  // Free the current entry and set the current entry pointers to NULL
  free (cacheTable->cacheEntries[whereToDeleteIdx].key);
  free (cacheTable->cacheEntries[whereToDeleteIdx].data);
  cacheTable->cacheEntries[whereToDeleteIdx].key = NULL;
  cacheTable->cacheEntries[whereToDeleteIdx].data = NULL; */

  // If what we are deleting now is the not the last element in the cache table,
  // we need to "shift" the cache entry up so that they are still continous
  if (whereToDeleteIdx != (cacheTable->cacheMetaData.numElementsOccupied-1)) {
    memmove(&cacheTable->cacheEntries[whereToDeleteIdx],
            &cacheTable->cacheEntries[whereToDeleteIdx+1],
            ((cacheTable->cacheMetaData.numElementsOccupied - whereToDeleteIdx) - 1) * sizeof(CACHE_TABLE_ENTRY_T));
//    cacheTable->cacheEntries[cacheTable->cacheMetaData.numElementsOccupied-1].key = NULL;
//    cacheTable->cacheEntries[cacheTable->cacheMetaData.numElementsOccupied-1].data = NULL;
  }
  cacheTable->cacheMetaData.numElementsOccupied--;

  CACHE_TABLE_KEY_DUMP(cacheTable,"Dumping cache table keys after deletion:");

  return FSI_IPC_EOK;
}

// ----------------------------------------------------------------------------
// Search and return an entry that matches the key pointed by *buffer.key
// The matching data will be pointed by *buffer.data
//
// buffer (IN/OUT) = 'key' contains pointer to key to search on in the cache table
//                   'data' contains pointer to data retrieved
//
// Return: FSI_IPC_OK = success
//         otherwise  = failure

int fsi_cache_getEntry(CACHE_TABLE_T *cacheTable, char *key, char *data)
{
  CACHE_TABLE_ENTRY_T *entryMatched = NULL;

  // Validate parameter
  if ((cacheTable == NULL) ||
      (key == NULL) ||
      (data == NULL)) {
    FSI_TRACE(FSI_ERR, "Param check");
    return -1;
  }

  FSI_TRACE(FSI_INFO, "Looking for the following handle:");
  fsi_cache_32Bytes_rawDump(FSI_INFO, key, 0);

  if (cacheTable->cacheMetaData.numElementsOccupied <= 0) {
    FSI_TRACE(FSI_INFO, "Cache is empty." );
    return -1;
  }

  CACHE_TABLE_KEY_DUMP(cacheTable, "Dumping current cache table keys:");

  entryMatched = bsearch(key,
                         &cacheTable->cacheEntries[0],
                         cacheTable->cacheMetaData.numElementsOccupied,
                         sizeof(CACHE_TABLE_ENTRY_T),
                         cacheTable->cacheMetaData.cacheKeyComprefn);

  if (entryMatched == NULL) {
    FSI_TRACE(FSI_INFO, "No match for handle");
    return -1;
  }

  data = entryMatched->data;
  return FSI_IPC_EOK;
}

// ----------------------------------------------------------------------------
// This function is used to dump all keys in the
// g_fsi_name_handle_cache_opened_files to the log.
//
// Input: cacheTable = cacheTable holding the keys to be printed.
//        titleString = Description the purpose of this print.  It's for
//                      logging purpose only. (NOTE: this can be NULL)
void fsi_cache_handle2name_dumpTableKeys(CACHE_TABLE_T *cacheTable, char *titleString)
{
	int i;

	if (titleString != NULL) {
		FSI_TRACE(FSI_DEBUG, titleString);
	}

	for (i=0; i<cacheTable->cacheMetaData.numElementsOccupied; i++) {
	  fsi_cache_32Bytes_rawDump(FSI_DEBUG, cacheTable->cacheEntries[i].key,i);
  }
}

// ----------------------------------------------------------------------------
// This function is used to dump first 32 bytes of data pointed by *data
//
// Input: data = data to be dumped.
//        index = indicate the index of this particular piece of data within
//                an whole array of similar data
void fsi_cache_32Bytes_rawDump(fsi_ipc_trace_level loglevel, void *data, int index)
{
  uint64_t * ptr = (uint64_t *) data;

	if (data != NULL) {
		ptr = (uint64_t *)data;
	  FSI_TRACE(loglevel, "Data[%d] = 0x%lx %lx %lx %lx",
	            index, ptr[0], ptr[1], ptr[2], ptr[3]);
	}
}

