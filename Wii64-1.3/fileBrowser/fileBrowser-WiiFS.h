/**
 * Wii64 - fileBrowser-WiiFS.h
 * Copyright (C) 2007, 2008, 2009 Mike Slegeir
 * 
 * fileBrowser Wii FileSystem module
 *
 * Wii64 homepage: http://www.emulatemii.com
 * email address: tehpola@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/


#ifndef FILE_BROWSER_WIIFS_H
#define FILE_BROWSER_WIIFS_H

#include "fileBrowser.h"

extern fileBrowser_file topLevel_WiiFS;
extern fileBrowser_file saveDir_WiiFS;

int fileBrowser_WiiFS_readDir(fileBrowser_file*, fileBrowser_file**, int, int);
int fileBrowser_WiiFS_readFile(fileBrowser_file*, void*, unsigned int);
int fileBrowser_WiiFS_writeFile(fileBrowser_file*, void*, unsigned int);
int fileBrowser_WiiFS_seekFile(fileBrowser_file*, unsigned int, unsigned int);
int fileBrowser_WiiFS_init(fileBrowser_file* f);
int fileBrowser_WiiFS_deinit(fileBrowser_file* f);

int fileBrowser_WiiFSROM_readFile(fileBrowser_file*, void*, unsigned int);
int fileBrowser_WiiFSROM_init(fileBrowser_file*);
int fileBrowser_WiiFSROM_deinit(fileBrowser_file*);

#endif

