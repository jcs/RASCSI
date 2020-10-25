//---------------------------------------------------------------------------
//
//  SCSI Video Mirror for Raspberry Pi
//
//  Copyright (C) 2020 joshua stein <jcs@jcs.org>
//
//  Licensed under the BSD 3-Clause License.
//  See LICENSE file in the project root folder.
//
//---------------------------------------------------------------------------

#pragma once

#include "os.h"
#include "disk.h"

class SCSIVideo : public Disk
{
public:
	SCSIVideo();
	virtual ~SCSIVideo();

	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor);
	BOOL FASTCALL ReceiveBuffer(int len, BYTE *buffer);

private:
	int screen_width;
	int screen_height;

	int fbfd;
	char *fb;
	int fbwidth;
	int fbheight;
	int fblinelen;
	int fbsize;
	int fbbpp;
};
