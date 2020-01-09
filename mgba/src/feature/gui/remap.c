/* Copyright (c) 2013-2016 Jeffrey Pfau
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "remap.h"

#include <mgba-util/gui.h>
#include <mgba-util/gui/menu.h>

void mGUIRemapKeys(struct GUIParams* params, struct mInputMap* map, const struct GUIInputKeys* keys) {
    // add xjsxjs197 start
	struct GUIMenu menu = {
		//.title = "Remap keys",
		.title = "重新映射按键",
		.index = 0,
		.background = 0
	};
	GUIMenuItemListInit(&menu.items, 0);
	const char* keyNames[keys->nKeys + 1];
	memcpy(&keyNames[1], keys->keyNames, keys->nKeys * sizeof(keyNames[0]));
	//keyNames[0] = "Unmapped";
	keyNames[0] = "未设定";
	size_t i;
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		//.title = "Game keys:",
		.title = "游戏按键：",
		.data = 0,
	};
	for (i = 0; i < map->info->nKeys; ++i) {
		*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
			.title = map->info->keyId[i],
			.data = (void*) (GUI_INPUT_MAX + i + 1),
			.submenu = 0,
			.state = mInputQueryBinding(map, keys->id, i) + 1,
			.validStates = keyNames,
			.nStates = keys->nKeys + 1
		};
	}
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		//.title = "Interface keys:",
		.title = "界面按键",
		.data = 0,
	};
	for (i = 0; i < params->keyMap.info->nKeys; ++i) {
		if (!params->keyMap.info->keyId[i]) {
			continue;
		}
		*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
			.title = params->keyMap.info->keyId[i],
			.data = (void*) i + 1,
			.submenu = 0,
			.state = mInputQueryBinding(&params->keyMap, keys->id, i) + 1,
			.validStates = keyNames,
			.nStates = keys->nKeys + 1
		};
	}
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		//.title = "Save",
		.title = "保存",
		.data = (void*) (GUI_INPUT_MAX + map->info->nKeys + 2),
	};
	*GUIMenuItemListAppend(&menu.items) = (struct GUIMenuItem) {
		//.title = "Cancel",
		.title = "取消",
		.data = 0,
	};
	// add xjsxjs197 end

	struct GUIMenuItem* item;
	while (true) {
		enum GUIMenuExitReason reason;
		reason = GUIShowMenu(params, &menu, &item);
		if (reason != GUI_MENU_EXIT_ACCEPT || !item->data) {
			break;
		}
		if (item->data == (void*) (GUI_INPUT_MAX + map->info->nKeys + 2)) {
			for (i = 0; i < GUIMenuItemListSize(&menu.items); ++i) {
				item = GUIMenuItemListGetPointer(&menu.items, i);
				if ((uintptr_t) item->data < 1) {
					continue;
				}
				if ((uintptr_t) item->data < GUI_INPUT_MAX + 1) {
					mInputBindKey(&params->keyMap, keys->id, item->state - 1, (uintptr_t) item->data - 1);
				} else if ((uintptr_t) item->data < GUI_INPUT_MAX + map->info->nKeys + 1) {
					mInputBindKey(map, keys->id, item->state - 1, (uintptr_t) item->data - GUI_INPUT_MAX - 1);
				}
			}
			break;
		}
		if (item->validStates) {
			// TODO: Open remap menu
		}
	}
}
