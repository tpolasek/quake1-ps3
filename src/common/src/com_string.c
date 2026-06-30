/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) Henrique Barateli, <henriquejb194@gmail.com>, et al.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
// com_string.c -- string library replacement functions.


#include "quakedef.h"
#include "net.h"
#include <string.h>
#include <strings.h>


void* Q_memmove(void* dest, const void* src, size_t count) {
    return memmove(dest, src, count);
}

void Q_memset(void* dest, int fill, size_t count) {
    memset(dest, fill, count);
}

void Q_memcpy(void* dest, const void* src, size_t count) {
    memcpy(dest, src, count);
}

int Q_memcmp(const void* m1, const void* m2, size_t count) {
    return memcmp(m1, m2, count);
}

void Q_strcpy(char* dest, const char* src) {
    strcpy(dest, src);
}

void Q_strncpy(char* dest, const char* src, size_t count) {
    strncpy(dest, src, count);
}

size_t Q_strlen(const char* str) {
    return strlen(str);
}

char* Q_strrchr(const char* s, char c) {
    return strrchr(s, c);
}

void Q_strcat(char* dest, const char* src) {
    strcat(dest, src);
}

int Q_strcmp(const char* s1, const char* s2) {
    return strcmp(s1, s2);
}

int Q_strncmp(const char* s1, const char* s2, size_t count) {
    return strncmp(s1, s2, count);
}

int Q_strncasecmp(const char* s1, const char* s2, size_t n) {
    return strncasecmp(s1, s2, n);
}

int Q_strcasecmp(const char* s1, const char* s2) {
    return Q_strncasecmp(s1, s2, 99999);
}

char* Q_strchr(const char* str, int c) {
    return strchr(str, c);
}

char* Q_strstr(const char* str, const char* substr) {
    return strstr(str, substr);
}
