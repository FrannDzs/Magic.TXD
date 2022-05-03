/*****************************************************************************
*
*  PROJECT:     Native Executive
*  LICENSE:     See LICENSE in the top level directory
*  FILE:        NativeExecutive/CExecutiveManager.spinlock.internal.h
*  PURPOSE:     Cross-platform native spin-lock implementation for low-level locking
*
*  Multi Theft Auto is available from http://www.multitheftauto.com/
*  Project location: https://osdn.net/projects/natexec/
*
*****************************************************************************/

#ifndef _NAT_EXEC_SPIN_LOCK_IMPL_HEADER_
#define _NAT_EXEC_SPIN_LOCK_IMPL_HEADER_

#include "CExecutiveManager.spinlock.h"

BEGIN_NATIVE_EXECUTIVE

// It is a good idea to share the spin-lock with everyone without needing a redirection layer.
typedef CSpinLock CSpinLockImpl;

END_NATIVE_EXECUTIVE

#endif //_NAT_EXEC_SPIN_LOCK_IMPL_HEADER_
