/*
 * Copyright (C) 2011-2015 FBReader.ORG Limited <contact@fbreader.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

package com.kindleunpack;

public class NativeFormatPlugin {

	//词典特制接口
	protected static native boolean isDictionaryNative(String path);
	protected static native Boolean[] isDictionaryListNative(String pathString);
	protected static native String getDictionaryContentNative(String path, String word);
	protected static native boolean initDictionaryNative(String pathString);//还未做
	protected static native String[] getDictionaryContentListNative(String word);

}
