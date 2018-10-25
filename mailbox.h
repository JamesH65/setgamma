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

#ifndef RASPI_MEMORY_H_
#define RASPI_MEMORY_H_

typedef struct
{
  uint32_t handle;
  uint32_t buffer;
  void *user;
  uint32_t size;
} VIDEOCORE_MEMORY_H;

int mailbox_init(void);
void mailbox_deinit(int fd);

int set_mailbox_property(int file_desc, void *buf);

unsigned int mailbox_memory_alloc(int file_desc, unsigned int size, unsigned int align, unsigned int flags);
unsigned int mailbox_memory_free(int file_desc, unsigned int handle);
unsigned int mailbox_memory_lock(int file_desc, unsigned int handle);
unsigned int mailbox_memory_unlock(int file_desc, unsigned int handle);


VIDEOCORE_MEMORY_H mailbox_videocore_alloc(int file_desc, int size);
void mailbox_videocore_free(int file_desc, VIDEOCORE_MEMORY_H mem);

unsigned int mailbox_set_cursor_position(int file_desc, int enabled, int x, int y, int flag);
unsigned int mailbox_set_cursor_info(int file_desc, int width, int height, int format, uint32_t buffer, int hotspotx, int hotspoty);

unsigned int mailbox_get_version(int file_desc);
unsigned int mailbox_get_overscan(int file_desc, int *top, int *bottom, int *left, int *right);


#endif /* RASPI_MEMORY_H_ */
