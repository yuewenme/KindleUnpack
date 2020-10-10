/*
 * Copyright (C) 2004-2015 FBReader.ORG Limited <contact@fbreader.org>
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

#include "PalmDocLikeStream.h"

PalmDocLikeStream::PalmDocLikeStream(const shared_ptr<ZLInputStream> inputStream) : PdbStream(inputStream) {
}

PalmDocLikeStream::~PalmDocLikeStream() {
	close();
}

bool PalmDocLikeStream::open() {
	myErrorCode = ERROR_NONE;
	if (!PdbStream::open()) {
		myErrorCode = ERROR_UNKNOWN;
		return false;
	}
	
	if (!processZeroRecord()) {
		return false;
	}

	myBuffer = new char[myMaxRecordSize];
	/**
	 * open()和processZeroRecord()已经将myBase定位到header().Start去了
	 */
	myRecordIndex = header().Start;
	return true;
}

bool PalmDocLikeStream::fillBuffer() {
	while (myBufferOffset == myBufferLength) {
		if (myRecordIndex + 1 > myMaxRecordIndex) {
			return false;
		}
		++myRecordIndex;
		if (!processRecord()) {//PalmDocStream::processRecord
			return false;
		}
//		LOGI("myRecordIndex %d myMaxRecordIndex %d myBufferOffset %d myBufferLength %d",myRecordIndex,myMaxRecordIndex,myBufferOffset,myBufferLength);
	}
	//myBufferOffset = 0;
	return true;
}

PalmDocLikeStream::ErrorCode PalmDocLikeStream::errorCode() const {
	return myErrorCode;
}
