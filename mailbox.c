 /*
 * Copyright © 2014 James Hughes jnahughes@googlemail.com
 * Based on some code copyright Herman Hermitage
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dlfcn.h>

#include "mailbox.h"

// Might be a define for this somewhere in the raspi userland headers somewhere?
// ARM can see VC4 L2 only on Pi 1
#define MEMORY_ALLOCATE_FLAG_L2 0x0c        // MEM_FLAG_L1_NONALLOCATING = (MEM_FLAG_DIRECT | MEM_FLAG_COHERENT) ... allocating in VC4 L2
#define MEMORY_ALLOCATE_FLAG_NON_L2 0x04    // MEM_FLAG_DIRECT

// Use a page size of 4k
static const int page_size = 4*1024;
static const int alignment = 4*1024;

// We will change the alloc flag depending on Pi1 or Pi2/Pi3
static int memory_alloc_flag = MEMORY_ALLOCATE_FLAG_L2;

// Macro to convert a bus to a physical address
#define BUS_TO_PHYS(x) ((x)&~0xC0000000)

// device parameters
#define OLD_MAILBOX_DEVICE_FILENAME "/dev/vc4mail"
#define MAILBOX_DEVICE_FILENAME "/dev/vcio"
#define MAJOR 100
#define MINOR 0
#define IOCTL_MBOX_PROPERTY   _IOWR(MAJOR, MINOR, char *)



int set_mailbox_property(int file_desc, void *buf)
{
   int retval = ioctl(file_desc, IOCTL_MBOX_PROPERTY, buf);

   if (retval < 0)
   {
      printf("ioctl_set_msg failed:%d\n", retval);
   }
   return retval;
}


/** map the specified address in to userspace
 *
 * @param base
 * @param size
 *
 * @return pointer to mapped memory, NULL if failed for any reason.
 *
 */
static void *map_memory(unsigned int base, unsigned int size)
{
   int fd;
   unsigned int offset = base % page_size;
   void *memory;

   base = base - offset;

   if ((fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0)
   {
      return NULL;
   }

   memory = mmap(0,
                  size,
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED,
                  fd,
                  base);

   if (memory == MAP_FAILED)
   {
      return NULL;
   }

   close(fd);

   return memory + offset;
}

/** Unmap previously mapped memory
 *
 * @param addr
 * @param size
 *
 */
static void *unmap_memory(void *addr, unsigned int size)
{
   int s = munmap(addr, size);

   if (s != 0)
   {
      // how to report error?
      return NULL;
   }

   return NULL;
}

void create_mailbox_request(unsigned int p[], unsigned int tag_id, unsigned int num_req_data, unsigned int num_res_data) {
   unsigned int i = 0;
   unsigned int num_fill = num_res_data > num_req_data ? num_res_data : num_req_data;
   p[i++] = 0;       // size. Filled in below
   p[i++] = 0x00000000;
   p[i++] = tag_id;
   p[i++] = num_res_data << 2;
   p[i++] = num_req_data << 2;
   for (unsigned int j = 0; j < num_fill; j++) {
     p[i++] = 0x00000000;
   }
   p[i++] = 0x00000000; // end tag
   p[0] = i*sizeof(*p); // actual size
}

/** Alloc a block of relocatable memory on the Videocore via mailbox call
 *
 * @param fd file descriptor of the mailbox driver
 * @param size size of block to allocate
 * @param align ALignment requirements
 * @param flags VC4 Allocation flag
 * @return Handle to the memory block, or NULL
 */
unsigned int mailbox_memory_alloc(int fd, unsigned int size, unsigned int align, unsigned int flags)
{
   unsigned int p[32];

   // Against the documentation, buffer size must be greater than 4 bytes
   create_mailbox_request(p, 0x0003000c, 3, 3);
   int i = 5;
   p[i++] = size;    // (num bytes? or pages?)
   p[i++] = align;   //
   p[i++] = flags;   // (MEMORY_ALLOCATE_FLAG)

   set_mailbox_property(fd, p);

   return p[5];
}

/** Free memory previously allocated on the Videocore via mailbox call
 *
 * @param fd file descriptor of the mailbox driver
 * @param handle Handle to thememory block as returned by the alloc call
 *
 */
unsigned int mailbox_memory_free(int file_desc, unsigned int handle)
{
   unsigned int p[32];
   create_mailbox_request(p, 0x0003000f, 1, 1);
   int i = 5;
   p[i++] = handle;

   set_mailbox_property(file_desc, p);

   return p[5];
}

/** Lock a block of relocatable memory Videocore via mailbox call
 *
 * @param fd file descriptor of the mailbox driver
 * @param handle Handle to thememory block as returned by the alloc call
 * @return Pointer (in video core address space) to the locked block
 */
unsigned int mailbox_memory_lock(int file_desc, unsigned int handle)
{
   unsigned int p[32];
   create_mailbox_request(p, 0x0003000d, 1, 1);
   int i = 5;
   p[i++] = handle;

   set_mailbox_property(file_desc, p);

   return p[5];
}

/** Lock a block of relocatable memory Videocore via mailbox call
 *
 * @param fd file descriptor of the mailbox driver
 * @param handle Handle to the memory block as returned by the alloc call
 * @return ??? Dunno
 */
unsigned int mailbox_memory_unlock(int file_desc, unsigned handle)
{
   unsigned int p[32];
   create_mailbox_request(p, 0x0003000e, 1, 1);
   int i = 5;
   p[i++] = handle;

   set_mailbox_property(file_desc, p);

   return p[5];
}

/** Function that wraps the mailbox calls above to make a easy to use
 * allocation function
 *
 * @param fd file descriptor of the mailbox driver
 * @param size Amount of memory to allocate
 * @return A structure containing the allocation details.
 */
VIDEOCORE_MEMORY_H mailbox_videocore_alloc(int fd, int size)
{
   VIDEOCORE_MEMORY_H mem;

   // allocate memory on GPU, map it ready for use
   mem.handle = mailbox_memory_alloc(fd, size, alignment, memory_alloc_flag);
   mem.buffer = mailbox_memory_lock(fd, mem.handle);
   mem.user = map_memory(BUS_TO_PHYS(mem.buffer), size);
   mem.size = size;

   return mem;
}

/** Function that wraps the mailbox calls above to make a easy to use
 * deallocation function
 *
 * @param fd file descriptor of the mailbox driver
 * @param mem Structure that was the result of the allocate call
 */
void mailbox_videocore_free(int file_desc, VIDEOCORE_MEMORY_H mem)
{
   unmap_memory(mem.user, mem.size);
   mailbox_memory_unlock(file_desc, mem.handle);
   mailbox_memory_free(file_desc, mem.handle);
}


/** Function that sets the HW cursor position on the display
 *
 * @param file_desc file descriptor of the mailbox driver
 * @param enabled Flag to enable/disable the cursor
 * @param x X position
 * @param y Y position
 * @param flag Bitfield. Bit 0 : 0 = display coords 1 = framebuffer coords.
 */
unsigned int mailbox_set_cursor_position(int file_desc, int enabled, int x, int y, int flag)
{
   unsigned int p[32];
   create_mailbox_request(p, 0x00008011, 4, 1);
   int i = 5;
   p[i++] = enabled;
   p[i++] = x;
   p[i++] = y;
   p[i++] = flag;

   set_mailbox_property(file_desc, p);
   return p[5];
}

/** Function that sets the HW cursor image, size and hotspots
 *
 * @param file_desc file descriptor of the mailbox driver
 * @param width Width of cursor, max 64
 * @param height Height of cursor, max 64
 * @param format Not presently used
 * @param Handle to Videocore memory buffer, as returned in VIDEOCORE_MEMORY_H.buffer in the mailbox_videocore_alloc call
 * @param hotspotx X point in image that is the 'hotspot'
 * @param hotspoty Y point in image that is the 'hotspot'
 *
 * @return ??
 */
unsigned int mailbox_set_cursor_info(int file_desc, int width, int height, int format, uint32_t buffer, int hotspotx, int hotspoty)
{
   unsigned int p[32];
   create_mailbox_request(p, 0x00008010, 6, 1);
   int i = 5;
   p[i++] = width;
   p[i++] = height;
   p[i++] = format;
   p[i++] = buffer;           // ptr to VC memory buffer. Doesn't work in 64bit....
   p[i++] = hotspotx;
   p[i++] = hotspoty;

   set_mailbox_property( file_desc, p);
   return p[5];
}

/** Function that gets the current VC version number
 *
 * @param file_desc file descriptor of the mailbox driver
 * @return The firmware version number (which is time of build)
 */
unsigned int mailbox_get_version(int file_desc)
{
   unsigned int p[32];
   create_mailbox_request(p, 0x00000001, 0, 1);

   set_mailbox_property(file_desc, p);
   return p[5];
}

/** Function that gets the current overscan settings
 *
 * @param file_desc file descriptor of the mailbox driver
 * @param[out] top, bottom, left, right
 * @return ??
 */
unsigned int mailbox_get_overscan(int file_desc, int *top, int *bottom, int *left, int *right)
{
   unsigned int p[32];
   create_mailbox_request(p, 0x0004000a, 0, 4);

   set_mailbox_property(file_desc, p);

   *top    = p[5];
   *bottom = p[6];
   *left   = p[7];
   *right  = p[8];

   return p[5];
}



/** Function to initialise the mailbox system
 *
 * @return Returns a file descriptor for use in mailbox_* calls or 0 if failed
 */
int mailbox_init(void)
{
   struct stat stat_buf;
   int fd;
   void *handle;
   unsigned (*bcm_host_get_sdram_address)(void);

   // use /dev/vcio if available (kernel >= 4.1)
   if (stat(MAILBOX_DEVICE_FILENAME, &stat_buf) != -1) {
     fd = open(MAILBOX_DEVICE_FILENAME, 0);
   } else {
      // See if we have a device node, if not create one.
      if (stat(OLD_MAILBOX_DEVICE_FILENAME, &stat_buf) == -1) {
        // No node so attempt to create one.
        // Character device, readable by all
        if (mknod(OLD_MAILBOX_DEVICE_FILENAME, S_IFCHR | S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH, makedev(MAJOR, MINOR)) == -1)
           return 0;
      }
      fd = open(OLD_MAILBOX_DEVICE_FILENAME, 0);
   }

   // First check to see if we have the mailbox char device
   if (fd < 0)
   {
      return 0;
   }

   // Find out if we are a Pi1 or a Pi2/Pi3.

   // Pi 1 defaults

   handle = dlopen("libbcm_host.so", RTLD_LAZY);
   if (!handle) return -1;

   *(void **) (&bcm_host_get_sdram_address) = dlsym(handle, "bcm_host_get_sdram_address");

   if (bcm_host_get_sdram_address && bcm_host_get_sdram_address()!=0x40000000)
   {
      // Pi 2 and Pi 3
      memory_alloc_flag = MEMORY_ALLOCATE_FLAG_NON_L2;
   }

   dlclose(handle);

   return fd;
}

/** Function to close down the mailbox system and release resources
 *
 * @param fd File descriptor returned from the init call.
 */
void mailbox_deinit(int fd)
{
   close(fd);

   // Should I delete the node?
}
