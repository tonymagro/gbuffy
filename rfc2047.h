/*
 * Copyright (C) 1996-8 Michael R. Elkins <me@cs.hmc.edu>
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */ 

/* File copied and slightly modified by from Mutt - http://www.mutt.org
   Tomas Ogren <stric@ing.umu.se> and Magnus Jonsson <bigfoot@acc.umu.se> */


void rfc2047_decode (char *, const char *, size_t);
#define hexval(c) Index_hex[(int)(c)]

#define NONULL(x) x?x:""

#define base64val(c) Index_64[(unsigned int)(c)]

enum
{
    ENCOTHER,
    ENC7BIT,
    ENC8BIT,
    ENCQUOTEDPRINTABLE,
    ENCBASE64,
    ENCBINARY
};
#define strfcpy(A,B,C) strncpy(A,B,C), *(A+(C)-1)=0
#define IsPrint(c) (isprint((unsigned char)(c)) || \
        (((unsigned char)(c) > 0xa0) && ((unsigned char)(c) < 0xff)))

