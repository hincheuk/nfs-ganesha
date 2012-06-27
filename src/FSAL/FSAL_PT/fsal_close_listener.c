// -----------------------------------------------------------------------------
// Copyright IBM Corp. 2010, 2011
// All Rights Reserved
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Filename:    pt_ganesha.c
// Description: Main layer for PT's Ganesha FSAL
// Author:      FSI IPC Team
// -----------------------------------------------------------------------------
#include "pt_ganesha.h"
#include <unistd.h>

void *ptfsal_closeHandle_listener_thread(void *args)
{
   int i;

   while (1) {
     FSI_TRACE(FSI_NOTICE, "CloseHandleListener thread is still alive");
     sleep (5);
   }
}
