/* microfs - Minimally Improved Compressed Read Only File System
 * Copyright (C) 2013 Erik Edlund <erik.edlund@32767.se>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef DEV_H
#define DEV_H

#ifndef HOSTPROGS_H
#error "#include hostprogs.h first"
#endif
#ifndef __MICROFS_H__
#error "#include microfs.h first"
#endif

/* Wrapper for %makedev() which fails if the device number
 * does not fit in the given number of %bits.
 */
static dev_t makedev_lim(int dev_major, int dev_minor, int bits)
{
	dev_t dev = makedev(dev_major, dev_minor);
	if (dev & -(1 << bits)) {
		error("device number (major=%du, minor=%du) truncated to %d bits"
			" - this most likely very wrong", dev_major, dev_minor, bits);
	}
	return dev;
}

/* Calculate the minor device number.
 */
static inline unsigned int minor_n(const unsigned long major,
	const unsigned long start, const unsigned long incr,
	const unsigned long n)
{
	return major + (n * incr - start);
}

#endif

