/* Memory management routine
   Copyright (C) 1998 Kunihiro Ishiguro

This file is part of GNU Zebra.

GNU Zebra is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

GNU Zebra is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Zebra; see the file COPYING.  If not, write to the Free
Software Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef _ZEBRA_MEMORY_H
#define _ZEBRA_MEMORY_H

enum
{
  MTYPE_TMP = 1,
  MTYPE_STREAM,
  MTYPE_STREAM_DATA,
  MTYPE_NMSG,

  MTYPE_MAX,
};


#define XMALLOC(mtype, size)       malloc (size)
#define XCALLOC(mtype, size)       calloc (1, (size))
#define XREALLOC(mtype, ptr, size) realloc ( (ptr), (size))
#define XFREE(mtype, ptr)          do { \
                                     free ((ptr)); \
                                     ptr = NULL; } \
                                   while (0)
#define XSTRDUP(mtype, str)        strdup ((str))


#endif /* _ZEBRA_MEMORY_H */