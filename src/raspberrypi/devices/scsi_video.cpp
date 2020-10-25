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

#include "scsi_video.h"

#include <err.h>
#include <fcntl.h>
#include <linux/fb.h>

static unsigned char reverse_table[256];

SCSIVideo::SCSIVideo() : Disk()
{
	struct fb_var_screeninfo fbinfo;
	struct fb_fix_screeninfo fbfixinfo;

	disk.id = MAKEID('S', 'C', 'V', 'D');

	// create lookup table
	for (int i = 0; i < 256; i++) {
		unsigned char b = i;
		b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
		b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
		b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
		reverse_table[i] = b;
	}

	// TODO: receive these through a SCSI message sent by the remote
	this->screen_width = 512;
	this->screen_height = 342;

	this->fbfd = open("/dev/fb0", O_RDWR);
	if (this->fbfd == -1)
		err(1, "open /dev/fb0");

	if (ioctl(this->fbfd, FBIOGET_VSCREENINFO, &fbinfo))
		err(1, "ioctl FBIOGET_VSCREENINFO");

	if (ioctl(this->fbfd, FBIOGET_FSCREENINFO, &fbfixinfo))
		err(1, "ioctl FBIOGET_FSCREENINFO");

	if (fbinfo.bits_per_pixel != 32)
		errx(1, "TODO: support %d bpp", fbinfo.bits_per_pixel);

	this->fbwidth = fbinfo.xres;
	this->fbheight = fbinfo.yres;
	this->fbbpp = fbinfo.bits_per_pixel;
	this->fblinelen = fbfixinfo.line_length;
	this->fbsize = fbfixinfo.smem_len;

	printf("SCSIVideo drawing on %dx%d %d bpp framebuffer\n",
	    this->fbwidth, this->fbheight, this->fbbpp);

	this->fb = (char *)mmap(0, this->fbsize, PROT_READ | PROT_WRITE, MAP_SHARED,
		this->fbfd, 0);
	if ((int)this->fb == -1)
		err(1, "mmap");

	memset(this->fb, 0, this->fbsize);
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSIVideo::~SCSIVideo()
{
	munmap(this->fb, this->fbsize);
	close(this->fbfd);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIVideo::Inquiry(
	const DWORD *cdb, BYTE *buf, DWORD major, DWORD minor)
{
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Basic data
	// buf[0] ... Communication Device
	// buf[2] ... SCSI-2 compliant command system
	// buf[3] ... SCSI-2 compliant Inquiry response
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x09;

	// SCSI-2 p.104 4.4.3 Incorrect logical unit handling
	if (((cdb[1] >> 5) & 0x07) != disk.lun) {
		buf[0] = 0x7f;
	}

	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 36 - 5 + 8;	// required + 8 byte extension

	// Fill with blanks
	memset(&buf[8], 0x20, buf[4] - 3);

	// Vendor name
	memcpy(&buf[8], "jcs", 3);

	// Product name
	memcpy(&buf[16], "video mirror", 12);

	// Revision
	memcpy(&buf[32], "1337", 4);

	// Optional function valid flag
	buf[36] = '0';

	// Size of data that can be returned
	size = (buf[4] + 5);

	// Limit if the other buffer is small
	if (size > (int)cdb[4]) {
		size = (int)cdb[4];
	}

	//  Success
	disk.code = DISK_NOERROR;
	return size;
}

BOOL FASTCALL SCSIVideo::ReceiveBuffer(int len, BYTE *buffer)
{
	int row = 0;
	int col = 0;

	for (int i = 0; i < len; i++) {
		unsigned char j = reverse_table[buffer[i]];

		for (int bit = 0; bit < 8; bit++) {
			int loc = (col * (this->fbbpp / 8)) + (row * this->fblinelen);
			col++;
			if (col % this->screen_width == 0) {
				col = 0;
				row++;
			}

			*(this->fb + loc) = (j & (1 << bit)) ? 0 : 255;
			*(this->fb + loc + 1) = (j & (1 << bit)) ? 0 : 255;
			*(this->fb + loc + 2) = (j & (1 << bit)) ? 0 : 255;
		}
	}

	return TRUE;
}
