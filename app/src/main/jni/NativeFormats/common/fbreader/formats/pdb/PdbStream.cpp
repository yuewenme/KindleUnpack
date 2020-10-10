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

#include <cstring>
#include "PdbStream.h"

PdbStream::PdbStream(const shared_ptr<ZLInputStream> inputStream) : myBase(inputStream) {
	myBuffer = 0;
}

PdbStream::~PdbStream() {
}

bool PdbStream:: open() {
	close();
	if (myBase.isNull() || !myBase->open() /*|| !myHeader.read(myBase)*/) {
		return false;
	}

	if(myHeader.Offsets.size() == 0 && !myHeader.read(myBase))
		return false;

	// myBase offset: startOffset + 78 + 8 * records number ( myHeader.Offsets.size() )
	
	myBase->seek(myHeader.Offsets[0], true);
	myBase->seek(36,false);                                  // myBase offset: ^ + 36
	size_t version= PdbUtil::readUnsignedLongBE(*myBase);    // myBase offset: ^ + 40
	myHeader.Start = 0;
	if(version != 8) {
		for (int i = 1; i < myHeader.Offsets.size(); i++) {
			if (myHeader.Offsets[i] - myHeader.Offsets[i - 1] == 8) {
				myBase->seek(myHeader.Offsets[i - 1], true);
				char buffer[8];
				int readLen = myBase->read(buffer, 8);
                std::string boundary(buffer,8);
				if (readLen == 8 && boundary.compare("BOUNDARY")==0) {
					myHeader.Start = i;
					break;
				}
			}
		}
	}

    //物理文件跳转到阅读部分的起始点
	myBase->seek(myHeader.Offsets[myHeader.Start], true);
	//未压缩文件（虚拟线）从0开始，好比截取了物理文件的一部分
	myBufferLength = 0;
	myBufferOffset = 0;
	myOffset = 0;

	return true;
}

size_t PdbStream::read(char *buffer, size_t maxSize) {
	maxSize = std::min(maxSize, (size_t)std::max(sizeOfOpened() - offset(), (size_t)0));
	//LOGI("yun28 myOffset %d sizeOfOpened() %d",myOffset,sizeOfOpened());
	size_t realSize = 0;
	while (realSize < maxSize) {
		if (!fillBuffer()) {//PalmDocLikeStream::fillBuffer
			break;
		}
		size_t size = std::min((size_t)(maxSize - realSize), (size_t)(myBufferLength - myBufferOffset));
		if (size > 0) {
			if (buffer != 0) {
				memcpy(buffer + realSize, myBuffer + myBufferOffset, size);
			}
			realSize += size;
			myBufferOffset += size;
		}
	}
	myOffset += realSize;

	return realSize;
}

void PdbStream::close() {
	if (!myBase.isNull()) {
		myBase->close();
	}
	if (myBuffer != 0) {
		delete[] myBuffer;
		myBuffer = 0;
	}
}

/**
 * 这个seek处理的是未压缩文件，myBase.seek处理的是物理压缩文件。如果传进来的offset是相对于物理文件流的值，那么跳转就错了，除非物理文件本来没压缩。
 * @param offset 是相对于解压后的文件流来说的，所以是通过读的方式来跳转，在read里会解压数据。
 * @param absoluteOffset
 */
void PdbStream::seek(int offset, bool absoluteOffset) {
	if (absoluteOffset) {
		offset -= this->offset();
	}
	if (offset > 0) {
		read(0, offset);
	} else if (offset < 0) {
		offset += this->offset();
		open();
		if (offset >= 0) {
			read(0, offset);
		}
	}
}

size_t PdbStream::offset() const {
	return myOffset;
}

size_t PdbStream::recordOffset(size_t index) const {
	return index < myHeader.Offsets.size() ?
		myHeader.Offsets[index] : myBase->sizeOfOpened();
}
