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
#include <algorithm>
#include "PalmDocStream.h"
#include "DocDecompressor.h"
#include "HuffDecompressor.h"
#include <regex.h>
#include <ZLUnicodeUtil.h>
#include <ZLLanguageUtil.h>
#include <ZLStringUtil.h>

PalmDocStream::PalmDocStream(const shared_ptr<ZLInputStream> inputStream, OpenType open, std::string word, long start,
                             long end) : PalmDocLikeStream(inputStream) {
    myCodec = "windows-1252";
    myFragmentIndex = 0xffffffff;
    mySkeletonIndex = 0xffffffff;
    myNcxIndex = 0xffffffff;
    translationContext = word;
    openType = open;
    this->start = start;
    this->end = end;
}

PalmDocStream::~PalmDocStream() {
    close();
}

bool PalmDocStream::processRecord() {
    const size_t currentOffset = recordOffset(myRecordIndex);
    const size_t nextOffset = recordOffset(myRecordIndex + 1);
//    if (currentOffset < myBase->offset()&&nextOffset>myBase->offset()&&!isNotFastSeek) {
//        return false;
//    }
    myBase->seek(currentOffset, true);
    if (nextOffset < currentOffset) {
//        LOGI("nextOffset %d currentOffset",nextOffset,currentOffset);
        return false;
    }
    const unsigned short recordSize = nextOffset - currentOffset;
    switch (myCompressionVersion) {
        case 17480://'DH'	// HuffCDic compression
            if (myHuffDecompressorPtr->error())
                myErrorCode = ERROR_UNKNOWN;
            else
                myBufferLength = myHuffDecompressorPtr->decompress(*myBase, myBuffer, recordSize,
                                                                   myMaxRecordSize);
            break;
        case 2:                // PalmDoc compression
            myBufferLength = DocDecompressor().decompress(*myBase, myBuffer, recordSize,
                                                          myMaxRecordSize);
            break;
        case 1:                // No compression
            myBufferLength = myBase->read(myBuffer, std::min(recordSize, myMaxRecordSize));
            break;
    }
//    LOGI("yun2 %d %d %d %d %d %d", myOffset, currentOffset, myRecordIndex, myMaxRecordSize, myCompressionVersion,myBufferLength);
    myBufferOffset = 0;
    return true;
}

/**
 * 整个mobi、azw书籍的第一个流解析方法，在这里解析mobi头，它的pdb头已经在PdfStream里解析出来了   by bmj
 *此时myBase已经指向了pdfheader.offset[header.Start]的位置，这是mobi文件正文的开始位置，之前的部分是pdb文件头的区域。
 */
bool PalmDocStream::processZeroRecord() {
    //LOGI("yun current : %d",myBase->offset());
    // 解析压缩方式
    myCompressionVersion = PdbUtil::readUnsignedShort(*myBase); // myBase offset: ^ + 2
    switch (myCompressionVersion) {   //判断是否是支持的压缩方式
        case 1:
        case 2:
        case 17480:
            break;
        default:
            myErrorCode = ERROR_COMPRESSION;
            return false;
    }

    /**
     * seek()的false表示相对，相对于pdfheader.offset[header.Start]的偏移。
     * 所以myBase->seek(2, false)等于myBase->seek(pdfheader.offset[header.Start]+4,true)
     */
    myBase->seek(2, false);                                    // myBase offset: ^ + 4
    //正文结束的位置，这是相对于mobi起点的未压缩文件流里的文件位置值
    myTextLength = PdbUtil::readUnsignedLongBE(*myBase);    // myBase offset: ^ + 8


    //解析正文分块数
    myTextRecordNumber = PdbUtil::readUnsignedShort(*myBase);    // myBase offset: ^ + 10
    size_t endSectionIndex = header().Offsets.size();
    myMaxRecordIndex = std::min((unsigned short) (myTextRecordNumber + header().Start),
                                (unsigned short) (endSectionIndex - 1));
    //解析内容块的最大长度
    myMaxRecordSize = PdbUtil::readUnsignedShort(*myBase);    // myBase offset: ^ + 12
    //LOGI("yun myCompressionVersion : %d myMaxRecordSize : %d",myCompressionVersion,myMaxRecordSize);
    if (myCompressionVersion == 17480) {
        myMaxRecordSize *= 2;
    }
    if (myMaxRecordSize == 0) {
        myErrorCode = ERROR_UNKNOWN;
        return false;
    }

    //解析是否加密
    const unsigned short encrypted = PdbUtil::readUnsignedShort(
            *myBase);        // myBase offset: ^ + 14
    if (encrypted) {                                        //Always = 2, if encrypted，加密的书是不支持的
        myErrorCode = ERROR_ENCRYPTION;
        return false;
    }

    //解析mobi文件头的内容长度
    myBase->seek(6, false);
    size_t headerLen = PdbUtil::readUnsignedLongBE(*myBase); // myBase offset: ^ + 24
    //解析文本编码方式
    myBase->seek(4, false);                                // myBase offset: ^ + 28
    size_t codepage = PdbUtil::readUnsignedLongBE(*myBase); // myBase offset: ^ + 32
    if (codepage == 65001)
        myCodec = "utf-8";

    //解析kindle version
    myBase->seek(4, false);                                  // myBase offset: ^ + 36
    myKindleVersion = PdbUtil::readUnsignedLongBE(*myBase); // myBase offset: ^ + 40
    if (myKindleVersion < 8) {
        myMetaOrthIndex = PdbUtil::readUnsignedLongBE(*myBase);//myBase offset: ^ + 44
        if (myMetaOrthIndex != 0xffffffff)
            myMetaOrthIndex += header().Start;
        myMetainflindex = PdbUtil::readUnsignedLongBE(*myBase);//myBase offset: ^ + 48
        if (myMetainflindex != 0xffffffff)
            myMetainflindex += header().Start;
        if (myMetaOrthIndex != 0xffffffff && openType == OPEN_DICT_IS_DICT) {
            myBase->seek(44, false);// myBase offset: ^ + 92
            const unsigned long inputLanguage = PdbUtil::readUnsignedLongBE(
                    *myBase);//+96:input Language	Input language for a dictionary
            const unsigned long outputLanguage = PdbUtil::readUnsignedLongBE(
                    *myBase);//+100：Output Language	Output language for a dictionary

            std::string lang = ZLLanguageUtil::languageByIntCode(inputLanguage & 0xFF,
                                                                 (inputLanguage >> 8) & 0xFF);
            std::string lang2 = ZLLanguageUtil::languageByIntCode(outputLanguage & 0xFF,
                                                                  (outputLanguage >> 8) & 0xFF);
            if (lang != "" && lang2 != "")
                isDictionary = true;
            else
                isDictionary = false;
            myBase->seek(8, false);
        } else {
            isDictionary = false;
            myBase->seek(60, false);// myBase offset: ^ + 108
        }
    } else {
        isDictionary = false;
        myMetaOrthIndex = 0xffffffff;
        myMetainflindex = 0xffffffff;
        myBase->seek(68, false);// myBase offset: ^ + 108
    }

    //解析图片资源块索引
    myImageStartIndex = PdbUtil::readUnsignedLongBE(*myBase) +
                        header().Start;   //第一个资源文件在pdbheader.Offsets列表中的索引 myBase offset: ^ + 112

    //解析是否有附加文件头信息数据
    myBase->seek(16, false);   //128 0x80
    size_t exth_flag = PdbUtil::readUnsignedLongBE(*myBase);  //132

    //解析css(svg)信息表的索引
    myBase->seek(60, false);// myBase offset: ^ + 192
    myFDSTIndex = PdbUtil::readUnsignedLongBE(*myBase) + header().Start;// myBase offset: ^ + 196
    myBase->seek(48, false);// myBase offset: ^ + 244
    //解析目录块索引
    myNcxIndex = PdbUtil::readUnsignedLongBE(*myBase) + header().Start;
    if (isMobi8()) {
        //解析正文块信息表的索引（块编号，所属文件，起止位置等）
        myFragmentIndex = PdbUtil::readUnsignedLongBE(*myBase) + header().Start;
        //解析文件信息表的索引（文件编号，文件起始aid，文件起止位置，文件大小，文件分块数等）
        mySkeletonIndex = PdbUtil::readUnsignedLongBE(*myBase) + header().Start;
        //myBase->seek(4, false);
        //解析Guide文件的索引
        //myGuideIndex = PdbUtil::readUnsignedLongBE(*myBase);
    }

    //如果start大于0，说明都多个文件头。现在读取的是start位置的文件头，但是图像资源是全书共享的，
    //所以还得用第一个文件头的图像偏移信息。
    if (header().Start > 0) {
        myBase->seek(header().Offsets[0] + 0x6c, true);
        myImageStartIndex = PdbUtil::readUnsignedLongBE(*myBase);
        myTextLength += header().Offsets[header().Start];
    }

    //LOGE("ffff","version=%d,StartOffset=%d,myTextLen=%d,image=%d,nxc=%d,fragment=%d,skeleton=%d,myFDSTIndex=%d",
    //     myKindleVersion,header().Offsets[header().Start],myTextLength,myImageStartIndex, myNcxIndex,myFragmentIndex,mySkeletonIndex,myFDSTIndex);
    //myBase->seek(header().Offsets[header().Start+1],true);

    //读取封面图片的索引偏移
    bool hasExth = (exth_flag & 0x40) > 0;
    size_t exth_offset = headerLen + 16;
    if (hasExth) {
        myBase->seek(header().Offsets[header().Start] + exth_offset + 4,
                     true);//myBase offset: ^ + exth_offset+4
        size_t exth_len = PdbUtil::readUnsignedLongBE(*myBase);  //myBase offset: ^ + exth_offset+8
        size_t num_items = PdbUtil::readUnsignedLongBE(
                *myBase);   //myBase offset: ^ + exth_offset+12
        for (size_t i = 0; i < num_items; i++) {
            size_t id = PdbUtil::readUnsignedLongBE(*myBase);
            size_t size = PdbUtil::readUnsignedLongBE(*myBase);
            size_t contentsize = size - 8;

            /**id的一部分取值范围
             * id_map_values = {
             *         115 : 'sample',
             *         116 : 'StartOffset',
             *        121 : 'K8(121)_Boundary_Section',
             *         125 : 'K8(125)_Count_of_Resources_Fonts_Images',
             *         126 : 'original-resolution',
             *         131 : 'K8(131)_Unidentified_Count',
             *         201 : 'CoverOffset',
             *         202 : 'ThumbOffset',
             *         203 : 'Has Fake Cover',
             *         204 : 'Creator Software',
             *         205 : 'Creator Major Version',
             *         206 : 'Creator Minor Version',
             *         207 : 'Creator Build Number',
             *         401 : 'Clipping Limit',
             *         402 : 'Publisher Limit',
             *         404 : 'Text to Speech Disabled',
             *         406 : 'Rental_Indicator',
             * }
             */
            if (id == 201) {
                if (size == 12) {
                    coverOffset = PdbUtil::readUnsignedLongBE(*myBase);
                }
                break;
            }else if (id == 126) {
                if (contentsize>0) {
                    char buffer[contentsize+1];
                    myBase->read(buffer,contentsize);
                    buffer[contentsize] = 0;
                    originalResolution = buffer;
                }
            } else {
                myBase->seek(contentsize, false);
            }
        }
    }

    if (myCompressionVersion == 17480) {
        unsigned long mobiHeaderLength;
        unsigned long huffSectionIndex;
        unsigned long huffSectionNumber;
        unsigned long huffTblOffset;
        unsigned long huffTblNumber;
        unsigned long extraFlags = 0;
        unsigned long initialOffset = header().Offsets[header().Start + 0];

        myBase->seek(initialOffset + 20,
                     true);                                        // myBase offset: ^ + 20
        mobiHeaderLength = PdbUtil::readUnsignedLongBE(*myBase);        // myBase offset: ^ + 24

        myBase->seek(initialOffset + 112,
                     true);                                // myBase offset: ^ + 112
        huffSectionIndex = PdbUtil::readUnsignedLongBE(*myBase)+header().Start;;        // myBase offset: ^ + 116
        huffSectionNumber = PdbUtil::readUnsignedLongBE(*myBase);        // myBase offset: ^ + 120
        huffTblOffset = PdbUtil::readUnsignedLongBE(*myBase);        // myBase offset: ^ + 124
        huffTblNumber = PdbUtil::readUnsignedLongBE(*myBase);        // myBase offset: ^ + 128

        if (16 + mobiHeaderLength >= 244) {
            myBase->seek(initialOffset + 240,
                         true);                            // myBase offset: ^ + 240
            extraFlags = PdbUtil::readUnsignedLongBE(*myBase);            // myBase offset: ^ + 244
        }
        /*
        std::cerr << "mobi header length: " <<  mobiHeaderLength << "\n";
        std::cerr << "Huff's start record  : " << huffSectionIndex << " from " << endSectionIndex - 1 << "\n";
        std::cerr << "Huff's records number: " << huffSectionNumber << "\n";
        std::cerr << "Huff's extraFlags    : " << extraFlags << "\n";
        */
        const unsigned long endHuffSectionIndex = huffSectionIndex + huffSectionNumber;
        if (endHuffSectionIndex > endSectionIndex || huffSectionNumber <= 1) {
            myErrorCode = ERROR_COMPRESSION;
            return false;
        }
        const unsigned long endHuffDataOffset = recordOffset(endHuffSectionIndex);
        std::vector<unsigned long>::const_iterator beginHuffSectionOffsetIt =
                header().Offsets.begin() + huffSectionIndex;
        // point to first Huff section
        std::vector<unsigned long>::const_iterator endHuffSectionOffsetIt =
                header().Offsets.begin() + endHuffSectionIndex;
        // point behind last Huff section
//        LOGI("mobi******attachStorage beginHuffSectionOffsetIt : %d endHuffSectionOffsetIt ： %d ,endHuffDataOffset : %d ,myBase->sizeOfOpened() : %d",
//             beginHuffSectionOffsetIt,endHuffSectionOffsetIt,endHuffDataOffset,myBase->sizeOfOpened());
        myHuffDecompressorPtr = new HuffDecompressor(*myBase, beginHuffSectionOffsetIt,
                                                     endHuffSectionOffsetIt, endHuffDataOffset,
                                                     extraFlags);
        datp.clear();
        //LOGI("yun %d huffTblNumber : %d",huffTblOffset,huffTblNumber);
        for (int i = huffTblOffset; i < huffTblOffset + huffTblNumber; ++i) {
            size_t hdrLen = header().Offsets[i + 1] - header().Offsets[i];
            char hdr[hdrLen];
            myBase->seek((int) header().Offsets[i], true);
            myBase->read(hdr, hdrLen);
            loadDatpForDict(hdr, hdrLen);
            //LOGI("yun %d ",datp.size());
        }
        myBase->seek(initialOffset + 14,
                     true);                                    // myBase offset: ^ + 14
    }

    return true;
}



bool PalmDocStream::loadIndexInfoForDict(std::string wordName) {
    ZLUnicodeUtil::Ucs2String name;
    ZLUnicodeUtil::utf8ToUcs2(name, wordName);//将要查的词转为ucs2
//    if (name.size() > 4)
//        LOGI("yun text3 : %ld %ld %ld %ld", name[0], name[1], name[2], name[3]);
    //for (int i = 0; i < name.size(); i++)
    //LOGI("yun text3 : %d ", name[i]);
    bool isSuccess = false;
    if (myMetaOrthIndex != 0xFFFFFFFF) {//判断是否有词典索引
        int curOffset = myBase->offset();
        //LOGI("yun text start2 %d",curOffset);
        //LOGI("yun text myMetaOrthIndex %d",myMetaOrthIndex);
        size_t hdrLen = header().Offsets[myMetaOrthIndex + 1] - header().Offsets[myMetaOrthIndex];
        char data[hdrLen];
        myBase->seek((int) header().Offsets[myMetaOrthIndex], true);
        std::string string;
        myBase->read(data, hdrLen);
        S_INDXHeader indxHeader;
        bool isOk = parseINDXHeader(data, hdrLen, indxHeader);
        if (isOk) {//判断词典索引是否为空
            S_INDXDict indxD;
            /**
             * 生成有用数据：indxD.ordt2,用于索引内容的转码
             * indxD.otype转码的辅助参数
             */
            parseINDXHeaderForDictiony(data, indxD, 0xfdea == indxHeader.code);
            std::vector<S_SectionTag> tagTable;
            size_t controlByteCount = readTagSection(indxHeader.len, data, tagTable);
            std::string lastText;
            long start = myMetaOrthIndex + 1, end = myMetaOrthIndex + 1 + indxHeader.count, mid =
                    (start + end) / 2;
            /**
             * 以下为二分法法查找首先查到是那个块，再在索引块下进行二分法，由于可能出现多个数据查到后会进行前后匹配（目前未作跨块匹配）
             */
            while (start <= end) {
                //LOGI("yun text1 : %d %d", start, end);
                if (mid > start) {
                    size_t hdrLen = header().Offsets[mid + 1] - header().Offsets[mid];
                    char hdr[hdrLen];
                    myBase->seek((int) header().Offsets[mid], true);
                    myBase->read(hdr, hdrLen);
                    S_INDXHeader indxHeader2;
                    bool isOk = parseINDXHeader(hdr, hdrLen, indxHeader2);
                    if (isOk) {
                        char pos_data0[2] = {hdr[indxHeader2.start + 4],
                                             hdr[indxHeader2.start + 5]};
                        size_t startPos = PdbUtil::readUnsignedShort(pos_data0);
                        if (startPos > hdrLen)
                            break;
                        ZLUnicodeUtil::Ucs2String startString = getIndexString(startPos, indxD,
                                                                               hdr);
                        long compare = ZLUnicodeUtil::compareTo(name, startString);
                        if (compare > 0) {
                            start = mid;
                        } else if (compare < 0) {
                            end = mid;
                        } else {
                            int endPos = startPos;
                            int lastIndex = 0;
                            for (int i = 0; i < indxHeader2.count + 1; i++) {
                                startPos = endPos;
                                if (i == indxHeader2.count) {
                                    endPos = indxHeader2.start;
                                } else {
                                    char pos_data2[2] = {hdr[indxHeader2.start + 4 + 2 * i],
                                                         hdr[indxHeader2.start + 5 + 2 * i]};
                                    endPos = PdbUtil::readUnsignedShort(pos_data2);
                                }
                                lastIndex = getIndexTable(startPos, endPos, indxD, hdr,
                                                          controlByteCount, tagTable, name,
                                                          indexTable);
                                if (lastIndex == 0)
                                    break;
                            }
                            break;
                        }
                        mid = (start + end) / 2;
                    } else {
                        break;
                    }
                } else {
                    size_t hdrLen = header().Offsets[mid + 1] - header().Offsets[mid];
                    char hdr[hdrLen];
                    myBase->seek((int) header().Offsets[mid], true);
                    myBase->read(hdr, hdrLen);
                    S_INDXHeader indxHeader2;
                    bool isOk = parseINDXHeader(hdr, hdrLen, indxHeader2);
                    if (isOk) {
                        long start = 0, end = indxHeader2.count, mid = (start + end) / 2;
                        while (start <= end) {
                            //LOGI("yun text2 : %d %d", start, end);
                            /*for (int i = 0; i < end; ++i) {
                                char pos_data0[2] = {hdr[indxHeader2.start + 4 + 2 * i],
                                                     hdr[indxHeader2.start + 5 + 2 * i]};
                                int startPos = PdbUtil::readUnsignedShort(pos_data0);
                                if (startPos > hdrLen)
                                    break;
                                ZLUnicodeUtil::Ucs2String startString = getIndexString(startPos, indxD,
                                                                                       hdr);
                                std::string text;
                                ZLUnicodeUtil::ucs2ToUtf8(text, startString);
                                LOGI("yun text :%d  end: %d %s",start,i, text.c_str());
                            }*/
                            char pos_data0[2] = {hdr[indxHeader2.start + 4 + 2 * mid],
                                                 hdr[indxHeader2.start + 5 + 2 * mid]};
                            int startPos = PdbUtil::readUnsignedShort(pos_data0);
                            if (startPos > hdrLen)
                                break;
                            ZLUnicodeUtil::Ucs2String startString = getIndexString(startPos, indxD,
                                                                                   hdr);
                            //std::string text;
                            //ZLUnicodeUtil::ucs2ToUtf8(text, startString);
                            long compare = ZLUnicodeUtil::compareTo(name, startString);
                            if (compare > 0) {
                                //LOGI("yun text :%d  end: %d %s > 0",start,end, text.c_str());
                                start = mid + 1;
                            } else if (compare < 0) {
                                //LOGI("yun text :%d  end: %d %s < 0",start,end, text.c_str());
                                end = mid - 1;
                            } else {
                                //Todo

                                int endPos = startPos;
                                int lastIndex = 0;
                                for (int i = mid + 1; i < indxHeader2.count + 1; i++) {
                                    startPos = endPos;
                                    if (i == indxHeader2.count) {
                                        endPos = indxHeader2.start;
                                    } else {
                                        char pos_data2[2] = {hdr[indxHeader2.start + 4 + 2 * i],
                                                             hdr[indxHeader2.start + 5 + 2 * i]};
                                        endPos = PdbUtil::readUnsignedShort(pos_data2);
                                    }
                                    lastIndex = getIndexTable(startPos, endPos, indxD, hdr,
                                                              controlByteCount, tagTable, name,
                                                              indexTable);
                                    if (lastIndex == 0)
                                        break;
                                }
                                if (mid > 2) {
                                    startPos = PdbUtil::readUnsignedShort(pos_data0);
                                    for (int i = mid - 1; i > 0; i--) {
                                        endPos = startPos;
                                        char pos_data2[2] = {hdr[indxHeader2.start + 4 + 2 * i],
                                                             hdr[indxHeader2.start + 5 + 2 * i]};
                                        startPos = PdbUtil::readUnsignedShort(pos_data2);
                                        lastIndex = getIndexTable(startPos, endPos, indxD, hdr,
                                                                  controlByteCount, tagTable, name,
                                                                  indexTable);
                                        if (lastIndex == 0)
                                            break;
                                    }
                                }

                                break;
                            }
                            mid = (start + end) / 2;
                        }
                    }
                    break;
                }
            }
        }
        myBase->seek(curOffset, true);
    }
    return isSuccess;
}

ZLUnicodeUtil::Ucs2String
PalmDocStream::getIndexString(size_t startPos, S_INDXDict indxD, char hdr[]) {
    int textLength = hdr[startPos];
    char text1[textLength];
    for (int k = 0; k < textLength; k++) {
        text1[k] = hdr[startPos + 1 + k];
    }
    //获取text，下面text做了特殊情况下的字符转换。
    ZLUnicodeUtil::Ucs2String utext;
    int pattern = 0;
    if (indxD.otype == 0) {//为0时说明为两个字节代表一个字符的Unicode编码
        pattern = 2;
    } else {
        pattern = 1;
    }
    if (!indxD.ordt2.empty() && textLength > 0) {
        for (int k = 0; k < textLength; k = k + pattern) {
            size_t off;
            if (pattern == 1) {
                off = text1[k];
            } else {
                char longBytes[2] = {text1[k + 0], text1[k + 1]};
                off = PdbUtil::readUnsignedShort(longBytes);
            }
            if (off < indxD.ordt2.size()) {
                size_t us2 = ZLUnicodeUtil::changeUcs2Char(indxD.ordt2[off]);
                //LOGI("yun text : %d",us2);
                if (us2 != 0)
                    utext.push_back(us2);

            } else {
                size_t us2 = ZLUnicodeUtil::changeUcs2Char(off);
                //LOGI("yun text : %d",us2);
                if (us2 != 0)
                    utext.push_back(us2);
            }
        }

    } else {
        //LOGI("yun text3 : %s",text1);
        for (int k = 0; k < textLength; k++) {
            size_t off;
            /*if (pattern == 1) {*/
            off = text1[k];
            /*} else {
                char longBytes[2] = {text1[k + 0], text1[k + 1]};
                off = PdbUtil::readUnsignedShort(longBytes);
            }*/
            size_t us2 = ZLUnicodeUtil::changeUcs2Char(off);
            //LOGI("yun text : %d",off);
            //LOGI("yun text : %d",us2);
            if (us2 != 0)
                utext.push_back(us2);
        }
    }
    //std::string string;
    //ZLUnicodeUtil::ucs2ToUtf8(string,utext,utext.size());
    //LOGI("yun text : %s",string.c_str());
    return utext;
}

size_t PalmDocStream::getIndexTable(size_t startPos, size_t endPos, S_INDXDict indxD, char hdr[],
                                    size_t controlByteCount, std::vector<S_SectionTag> tagTable,
                                    ZLUnicodeUtil::Ucs2String name,
                                    std::vector<IndexTable> indexTable2) {
    //LOGI("yun text : %d end ： %d",startPos,endPos);
    int textLength = hdr[startPos];
    char text1[textLength];
    for (int k = 0; k < textLength; k++) {
        text1[k] = hdr[startPos + 1 + k];
    }
    //获取text，下面text做了特殊情况下的字符转换。
    ZLUnicodeUtil::Ucs2String utext;
    ZLUnicodeUtil::Ucs2String realText;
    int pattern = 0;
    if (indxD.otype == 0) {//为0时说明为两个字节代表一个字符的Unicode编码
        pattern = 2;
    } else {
        pattern = 1;
    }
    if (!indxD.ordt2.empty() && textLength > 0) {
        for (int k = 0; k < textLength; k = k + pattern) {
            size_t off;
            if (pattern == 1) {
                off = text1[k];
            } else {
                char longBytes[2] = {text1[k + 0], text1[k + 1]};
                off = PdbUtil::readUnsignedShort(longBytes);
            }
            if (off < indxD.ordt2.size()) {
                size_t us2 = ZLUnicodeUtil::changeUcs2Char(indxD.ordt2[off]);
                if (us2 != 0)
                    utext.push_back(us2);
                realText.push_back(indxD.ordt2[off]);

            } else {
                size_t us2 = ZLUnicodeUtil::changeUcs2Char(off);
                if (us2 != 0)
                    utext.push_back(us2);
                realText.push_back(off);
            }
        }

    } else {
        for (int k = 0; k < textLength; k++) {
            size_t off = text1[k];
            size_t us2 = ZLUnicodeUtil::changeUcs2Char(off);
            if (us2 != 0)
                utext.push_back(us2);
            realText.push_back(off);
        }
    }
    std::string text;
    ZLUnicodeUtil::ucs2ToUtf8(text, utext);
    if (ZLUnicodeUtil::compareTo(utext, name) == 0) {
        std::string text;
        ZLUnicodeUtil::ucs2ToUtf8(text, realText);
        std::map<size_t, std::vector<size_t> > tagMap = getTagMap(
                controlByteCount, tagTable, hdr, startPos + 1 + textLength, endPos);
        //LOGI("yun text : %d end ： %d",tagMap[1].size(),tagMap[2].size());
        if (tagMap[1].size() > 0) {
            size_t start = tagMap[1][0], len = 0;
            if (tagMap[2].size() > 0) {
                len = tagMap[2][0];
                //LOGI("yun text3 : %d end ： %d",tagMap[1][0],tagMap[1][0]);
            }
//            for (int i = 0; i < 6; ++i) {
//                if (tagMap[i].size() > 0) {
//                    len = tagMap[i][0];
//                    LOGI("yun text3 : %d",tagMap[i][0]);
//                }
//            }

            bool isInsert = false;
            for (int i = 0; i < indexTable.size(); i++) {
                if (indexTable[i].startPosition > start) {
                    isInsert = true;
                    //LOGI("yun startPos : %d",start);
                    indexTable.insert(indexTable.begin() + i, IndexTable(text, start, len));
                    break;
                }
            }
            if (!isInsert) {
                indexTable.push_back(IndexTable(text, start, len));
            }
            return start;
        }

        //LOGI("yun text2 : %d",indexTable.size());
    }
    return 0;
}

bool PalmDocStream::fastSeek(size_t offset) {
//    LOGI("yun0 %d  %d %d %d %d %d %d %d", offset, myRecordIndex, myMaxRecordSize, myCompressionVersion,header().Start,myBufferOffset,myBufferLength,myHeader.Offsets.size());
    if (myCompressionVersion == 17480) {
        size_t recordIndex = 0;
        size_t len = 0;
        for (size_t i = 0; i < datp.size(); ++i) {
            len += datp[i];
            if (len > offset) {
                len -= datp[i];
                recordIndex = i;
//                LOGI("yun myRecordIndex : %d",myRecordIndex);
                break;
            }
        }
        if(recordIndex != myRecordIndex||myOffset>offset){
            isNotFastSeek = 1;
            if (myOffset != 0) {
                myBufferOffset = myBufferLength = 0;
            }
            myOffset = len;
            myRecordIndex = recordIndex;
//            LOGI("yun %d",datp.size());
//            if (myOffset != 0 && myRecordIndex == 0) {
//                myMaxRecordIndex = datp.size() - 1;
//            }
//            LOGI("yun myCompressionVersion = 17480 %d  %d %d %d", myOffset,  myRecordIndex, myMaxRecordSize, myCompressionVersion);
        }
        seek(offset - myOffset, false);
    } else if (myCompressionVersion == 2) {
        int maxRecordSize = myMaxRecordSize;
        size_t off = header().Start>0?recordOffset(header().Start):0;
        size_t recordIndex = offset / maxRecordSize;
        offset += off;
        if(recordIndex+header().Start != myRecordIndex||myOffset<off||myOffset>offset){
            size_t len = maxRecordSize*recordIndex+off;
            isNotFastSeek = 1;
            if (myOffset != 0) {
                myBufferOffset = myBufferLength = 0;
            }
            myOffset = len;
            myRecordIndex = recordIndex+header().Start;
        }
//        LOGI("yun myCompressionVersion = 2 %d  %d %d %d", myOffset,  myRecordIndex, myMaxRecordSize, myCompressionVersion);
        seek(offset - myOffset, false);
    } else {
        isNotFastSeek = 0;
        seek(offset, true);
//        LOGI("yun myCompressionVersion = 0 %d  %d %d %d %d %d",header().Start,myMaxRecordIndex,myOffset,myRecordIndex, myMaxRecordSize, myCompressionVersion);
    }

    return true;
}

int PalmDocStream::realRead(size_t offset,char *buffer,size_t len){
    int curOffset = myBase->offset();

    myBase->seek(offset, true);
    int l = myBase->read(buffer,len);

    myBase->seek(curOffset,true);
    return l;
}
void PalmDocStream::loadTableInfoForAZW() {
    int curOffset = myBase->offset();

    //fragtbl 解析fragment table，正文块信息表
    fragtbl.clear();
    std::vector<S_TableInfo> outtbl;
    std::map<size_t, std::string> ctoc_text;
    bool isOk = false;
    if (myFragmentIndex != 0xffffffff)
        isOk = getIndexData(myFragmentIndex, outtbl, ctoc_text);
    if (isOk) {
        for (int i = 0; i < outtbl.size(); i++) {
            int ctocoffset = outtbl[i].tagMap[2].at(0);
            std::string ctocdata = ctoc_text[ctocoffset];
            fragtbl.push_back(S_FragmentTable(
                    atoi(outtbl[i].text.data()), ctocdata, outtbl[i].tagMap[3][0],
                    outtbl[i].tagMap[4][0], outtbl[i].tagMap[6][0], outtbl[i].tagMap[6][1]));
            //LOGI("table:%d %s %d %d %d %d",fragtbl[i].filePosition,fragtbl[i].linkIdText.data(),fragtbl[i].fileNum,fragtbl[i].sequenceNum,fragtbl[i].startPosition,fragtbl[i].length);
        }
    }


    //skeltbl 文件分块信息表
    outtbl.clear();
    ctoc_text.clear();
    isOk = false;
    if(mySkeletonIndex != 0xffffffff)
        isOk = getIndexData(mySkeletonIndex, outtbl, ctoc_text);
    if (isOk) {
        int fileptr = 0;
        for (int i = 0; i < outtbl.size(); i++) {
            skeltbl.push_back(S_SkeltonTable(
                    fileptr,outtbl[i].text,outtbl[i].tagMap[1][0],
                    outtbl[i].tagMap[6][0], outtbl[i].tagMap[6][1]));
            fileptr += 1;
            //LOGE("table:%d %s %d %d %d",skeltbl[i].fileNum,skeltbl[i].skeltonName.data(),skeltbl[i].fragtblRecordCount,skeltbl[i].startPosition,skeltbl[i].length);
        }
    }

    //获取ncx目录信息
    outtbl.clear();
    ctoc_text.clear();
    ncxtbl.clear();
    if (myNcxIndex != 0xffffffff) {
        isOk = getIndexData(myNcxIndex, outtbl, ctoc_text);
        //LOGE("yun ffff","ncx:outtbl.size=%d,codec=%s",outtbl.size(),myCodec.data());
        if (isOk) {
            std::map<int, int> tag_fieldname_map;
            tag_fieldname_map.insert(std::make_pair(1, 0));   //pos
            tag_fieldname_map.insert(std::make_pair(2, 0));   //len
            tag_fieldname_map.insert(std::make_pair(3, 0)); //noffs
            tag_fieldname_map.insert(std::make_pair(4, 0));  //hlvl
            tag_fieldname_map.insert(std::make_pair(5, 0)); //koffs
            tag_fieldname_map.insert(std::make_pair(6, 0));//pos_aid
            tag_fieldname_map.insert(std::make_pair(21, 0));//parent
            tag_fieldname_map.insert(std::make_pair(22, 0));//child1
            tag_fieldname_map.insert(std::make_pair(23, 0));//childn

            for (int j = 0; j < outtbl.size(); j++) {
                std::string text = outtbl[j].text;
                std::map<size_t, std::vector<size_t> > &tagMap = outtbl[j].tagMap;
                std::map<int, int>::iterator it = tag_fieldname_map.begin();
                S_NCXInfo tmp;
                tmp.index = j;
                tmp.parent = -1;
                while (it != tag_fieldname_map.end()) {
                    int tag = it->first;
                    int i = it->second;
                    if (tagMap.find(tag) != tagMap.end()) {
                        size_t fieldvalue = tagMap[tag][i];
                        if(tag == 6){
                            tmp.pos_fid = fieldvalue;
                            tmp.pos_off = tagMap[tag][i+1];
//                            tmp.pos_aid = getIDTagByPosFid(tmp.pos_fid,tmp.pos_off);
                        }
                        else
                        if (tag == 3) {
                            std::map<size_t, std::string>::iterator it = ctoc_text.find(fieldvalue);
                            if (it != ctoc_text.end()) {
                                tmp.text = it->second;  //这个可能需要使用myCodec编码格式来解码，不然是乱码
                            } else {
                                tmp.text = "Unknown Text";
                            }
                        } else if (tag == 21) {
                            tmp.parent = fieldvalue;
                        } else if (tag == 22) {
                            tmp.firstchild = fieldvalue;
                        } else if (tag == 23) {
                            tmp.endchild = fieldvalue;
                        } else if (tag == 1) {
                            tmp.position = fieldvalue;
                        }else if (tag == 2) {
                            tmp.len = fieldvalue;
                        }
                    }
                    it++;
                }

                //LOGE("yun ffff","1 ncx:index=%d,parent=%d,first=%d,end=%d,pos_aid=%s,text=%s,position=%d,len=%d,",tmp.index,tmp.parent,
                //     tmp.firstchild,tmp.endchild,tmp.pos_aid.data(),tmp.text.data(),tmp.position,tmp.len);
                //目录排序
                if (ncxtbl.size() == 0)
                    ncxtbl.push_back(tmp);
                else {
                    int parent;
                    for (parent = 0; parent < ncxtbl.size(); parent++) {
                        if (ncxtbl[parent].index != tmp.parent) {
                            continue;
                        } else break;
                    }

                    int lastBrother; //最后一个兄弟
                    for (lastBrother = parent + 1; lastBrother < ncxtbl.size(); lastBrother++) {
                        if (tmp.parent == ncxtbl[lastBrother].parent)
                            continue;
                        else break;
                    }

                    if (lastBrother < ncxtbl.size()) {
                        ncxtbl.insert(ncxtbl.begin() + lastBrother, tmp);
                    } else
                        ncxtbl.push_back(tmp);
                }
            }
        }
    }

//    LOGE("ffff","ncxtbl size=%d",ncxtbl.size());
//    for(int a=0;a<ncxtbl.size();a++){
//        LOGE("ffff","2-----ncx:index=%d,parent=%d,first=%d,end=%d,pos_aid=%s,text=%s",ncxtbl[a].index,ncxtbl[a].parent,
//             ncxtbl[a].firstchild,ncxtbl[a].endchild,ncxtbl[a].pos_aid.data(),ncxtbl[a].text.data());
//    }
    myBase->seek(curOffset, true);
}

bool PalmDocStream::loadTableOfContents() {
    int curOffset = myBase->offset();
    //获取正文中ncx目录信息
    std::size_t BUFSIZE = 2048;
    char buffer[BUFSIZE+1];
    size_t len = read(buffer,BUFSIZE);
    if(len<=0)
        return false;
    char * pos_pos = strstr(buffer,"filepos");
    if (pos_pos) {
        int count = 0;
        std::string string = buffer;
        S_NCXInfo title_ncx;
        int len = string.find("filepos");
        if(len>0){
            int start = string.find("=",len)+1;
            int end = string.find(" />",start);
            title_ncx.position =ZLStringUtil::parseDecimal(string.substr(start,end-start), -1);
            len = string.find("title");
            if(len>0){
                int start = string.find("\"",len)+1;
                int end = string.find("\"",start);
                title_ncx.text = string.substr(start,end-start);
            } else{
                title_ncx.text = "Table of Contents";
            }
            title_ncx.len = sizeOfOpened()-title_ncx.position;
//            LOGI("yun position %d len %d text %s",title_ncx.position,title_ncx.len,title_ncx.text.c_str());
            fastSeek(title_ncx.position);
            //do{
            char buffer2[title_ncx.len+1];
            read(buffer2,title_ncx.len);
            buffer2[title_ncx.len]=0;
//            LOGI("yun buffer %s",buffer2);
            string = buffer2;
            int cur = 0;
            do{
                cur = string.find("filepos",cur);
                if(cur>0){
                    S_NCXInfo tmp;
                    tmp.parent = 0;
                    tmp.index = count++;
                    int start = string.find("=",cur)+1;
                    int end = string.find(">",start);
//                    LOGI("string.substr(start,end) ：%s",string.substr(start,end-start).c_str());
                    tmp.position = ZLStringUtil::parseDecimal(string.substr(start,end-start), -1);
//                    LOGI("string.substr(start,end) ：%d",tmp.position);
                    end = string.find("</",end);
                    start = string.rfind(">",end)+1;
                    tmp.text = string.substr(start,end-start);
                    cur = end;
//                    LOGI("yun position %d len %d text %s",tmp.position,tmp.len,tmp.text.c_str());
                    ncxtbl.push_back(tmp);
                } else
                    break;
            }while (1);
            // }while (len==BUFSIZE);
        } else{
            title_ncx.position =myOffset;
            title_ncx.text = "unknown";
            title_ncx.len = myBase->sizeOfOpened()-title_ncx.position;
        }
        title_ncx.parent = 0;
        title_ncx.index = count;
        ncxtbl.push_back(title_ncx);
        for (int i = 0; i < count; ++i) {
            ncxtbl[i].len = ncxtbl[i+1].position-ncxtbl[i].position;
        }
    } else{
        S_NCXInfo title_ncx;
        title_ncx.index = 0;
        title_ncx.parent = 0;
        title_ncx.position =myOffset;
        title_ncx.text = "unknown";
        title_ncx.len = myBase->sizeOfOpened()-title_ncx.position;
        ncxtbl.push_back(title_ncx);
    }
    myBase->seek(curOffset, true);
    return true;
}
bool PalmDocStream::loadCssTableForAZW() {
    /**
     * 这里有两条线的偏移：myBase线和this线。myBase指向的是物理文件流，this算是物理文件对应的未压缩文件流
     * 所以myBase.offset 不等于 this.offset。header.Offsets里的文件地址是物理文件地址。
     */
    int curOffset = myBase->offset();

    /* *获取每个css文件的文件位置，位置存在fdsttbl里,这个位置值也是相对值，与myTextLength一样。
     *n和n+1表示fdsttbl的前后两个元素，n就是css的起始位置，n+1就是结束位置。
     *文件个数就是fdsttbl.size-1
     * */
    fdsttbl.clear();
    if (header().Offsets.size() <= myFDSTIndex ||
        header().Offsets[myFDSTIndex] > myBase->sizeOfOpened())
        return false;
    myBase->seek((int) header().Offsets[myFDSTIndex], true);
    char fdstHeader[4];
    myBase->read(fdstHeader, 4);
    if (fdstHeader[0] == 'F' && fdstHeader[1] == 'D' && fdstHeader[2] == 'S' &&
        fdstHeader[3] == 'T') {
        myBase->seek(4, false);
        size_t num_sections = PdbUtil::readUnsignedLongBE(*myBase);
        for (int i = 0; i < num_sections * 2; i++) {
            size_t filepos = PdbUtil::readUnsignedLongBE(*myBase);
            if (filepos == 0)
                continue;
//            LOGI("yun %ld %d",header().Offsets[header().Start],filepos);
            if (fdsttbl.size() == 0) {
                fdsttbl.push_back(filepos);
            } else {
                size_t back = fdsttbl.back();
                if (back != filepos) {
                    fdsttbl.push_back(filepos);
                }
            }
        }
    }
    myBase->seek(curOffset, true);
    return true;
}



bool PalmDocStream::getIndexData(size_t tableIndex, std::vector<S_TableInfo> &outtbl,
                                 std::map<size_t, std::string> &ctoc_text) {
    if(header().Offsets.size()<tableIndex+1){
        return false;
    }
    myBase->seek((int) header().Offsets[tableIndex], true);
    size_t tableDataLen = header().Offsets[tableIndex + 1] - header().Offsets[tableIndex];
    char tableData[tableDataLen];
    myBase->read(tableData, tableDataLen);
    S_INDXHeader tableHeader;

    bool ok = parseINDXHeader(tableData, tableDataLen, tableHeader);
    if (ok) {
        //LOGE("equal indx");
//        LOGI("count=%d,start=%d,nctoc=%d,total=%d,len = %d",tableHeader.count,
//             tableHeader.start,tableHeader.nctoc,tableHeader.total,tableHeader.len);

        std::vector<S_SectionTag> tagTable;
        size_t controlByteCount = readTagSection(tableHeader.len, tableData, tagTable);
//        LOGI("controlByteCount=%d,tagTable.size=%d",controlByteCount,tagTable.size());

        for (int i = tableIndex + 1; i < tableIndex + 1 + tableHeader.count; i++) {
            size_t hdrLen = header().Offsets[i + 1] - header().Offsets[i];
            char hdr[hdrLen];
            myBase->seek((int) header().Offsets[i], true);
            myBase->read(hdr, hdrLen);
            S_INDXHeader indxHeader;
            bool isOk = parseINDXHeader(hdr, hdrLen, indxHeader);
            if (isOk) {
                std::vector<size_t> idxPositions;
                for (int j = 0; j < indxHeader.count; j++) {
                    //从indxHeader.start+4开始每次解析两个字节
                    char pos_data[2] = {hdr[indxHeader.start + 4 + 2 * j],
                                        hdr[indxHeader.start + 4 + 2 * j + 1]};
                    idxPositions.push_back(PdbUtil::readUnsignedShort(pos_data));
                }
                idxPositions.push_back(indxHeader.start);

                for (int j = 0; j < indxHeader.count; j++) {
                    size_t startPos = idxPositions[j];
                    size_t endPos = idxPositions[j + 1];
                    size_t textLength = (size_t) hdr[startPos];
                    char text[textLength];
                    //获取text，这个text在插件里还做了特殊情况下的字符转换，这里没做了。
                    for (int k = 0; k < textLength; k++) {
                        text[k] = hdr[startPos + 1 + k];
                    }
                    std::map<size_t, std::vector<size_t> > tagMap = getTagMap(
                            controlByteCount, tagTable, hdr, startPos + 1 + textLength, endPos);
                    outtbl.push_back(S_TableInfo(std::string(text, 0, textLength), tagMap));
                }
            } else {
                return false;
            }
        }
        //读取CTOC数据，每一块CTOC占用header().Offsets里的一个编号
        ctoc_text = readCTOC(tableIndex + tableHeader.count + 1, tableHeader.nctoc);
    } else {
        // LOGE("not equal indx");
        return false;
    }
    return true;
}

bool PalmDocStream::loadSection(std::string string, int i) {
    size_t hdrLen = header().Offsets[i + 1] - header().Offsets[i];
    char hdr[hdrLen];
    //LOGI("yun text2 i : %d",i);
    //LOGI("yun text2 hdrLen : %d end : %d start %d", hdrLen, header().Offsets[i + 1],
    //     header().Offsets[i]);
    myBase->seek((int) header().Offsets[i], true);
    myBase->read(hdr, hdrLen);
    //LOGI("yun text2 hdrLen : %s",hdr);
    std::string utext;
    int p = 0, len = 0;
    while (p < hdrLen) {
        size_t c = hdr[p];
        p++;
        if (c >= 1 && c <= 8) {
            for (int i = p; i < p + c; i++) {
                //LOGI("yun text hdr[i] : %d",hdr[i]);
                utext.push_back(hdr[i]);
                len++;
            }
            p += c;
        } else if (c < 128) {
            utext.push_back(c);
            //LOGI("yun text2 hdr[i] : %d",c);
            len++;
        } else if (c >= 192) {
            //LOGI("yun text2 utext.size() : %d",utext.size());
            utext.push_back(' ');
            //LOGI("yun text3 hdr[i] : %d", ' ');
            len++;
            utext.push_back((char) (c ^ 128));
            //LOGI("yun text3 hdr[i] : %d", c ^ 128);
            len++;
            //LOGI("yun text2 utext.size() : %d",utext.size());
        } else {
            if (p < hdrLen) {
                //LOGI("yun text2 utext.size() : %d",utext.size());
                c = (c << 8) | hdr[p];
                p++;
                int m = (c >> 3) & 0x07ff;
                int n = (c & 7) + 3;
                //int size = utext.size();
                int size = utext.size();
                //LOGI("yun text2 m : %d n : %d size %d",m,n,size);
                //LOGI("yun text2 m : %d size+(n-m)size : %d ",size-m%size,size-(m-n)%size);
                if (m > n) {
                    for (int i = size - m % size; i < size - (m - n) % size; i++)
                        //for (ZLUnicodeUtil::Ucs2String::const_iterator it = utext.end()-m; it != it2; ++it)
                    {
                        //LOGI("yun text4 hdr[i] : %d", utext[i]);
                        utext.push_back(utext[i]);
                        len++;
                    }
                } else
                    for (int i = 0; i < n; i++) {
                        int size = utext.size();
                        //LOGI("yun text4 hdr[i] : %d", utext[m==0?0:size-m%size]);
                        utext.push_back(utext[m == 0 ? 0 : size - m % size]);
                        len++;

                    }
                //LOGI("yun text2 utext.size() : %d",utext.size());

            }
        }
    }
    //for(int i =0;i<utext.size();i++)
    //LOGI("yun:%d",utext[i]);
    //ZLUnicodeUtil::ucs2ToUtf8(string1, utext, utext.size());
    //LOGI("yun text2 hdrLen : %d", len);
    //LOGI("yun text2 hdrLen : %d  , %s",utext.size(),utext.c_str());
    return true;
}

std::map<size_t, std::vector<size_t> >
PalmDocStream::getTagMap(size_t controlByteCount, std::vector<S_SectionTag> &tagTable,
                         const char *entryData, size_t startPos, size_t endPos) {
    //LOGE("getTagMap: %d %d %d",controlByteCount,startPos,endPos);
    //for(int i=0;i<tagTable.size();i++)
    //    LOGE("tagTableItem: %d %d %d %d",tagTable[i].tag,tagTable[i].endFlag,tagTable[i].mask,tagTable[i].valuesPerEntry);
    size_t controlByteIndex = 0;
    size_t dataStart = startPos + controlByteCount;
    std::vector<S_FragmentTag> tags;

    for (int i = 0; i < tagTable.size(); i++) {
        if (tagTable[i].endFlag == 0x01) {
            controlByteIndex += 1;
            continue;
        }
        int cbyte = (int) entryData[startPos + controlByteIndex];
        int value = cbyte & tagTable[i].mask;
        if (value != 0) {
            if (value == tagTable[i].mask) {
                if (PdbUtil::countSetBits(tagTable[i].mask) > 1) {
                    size_t consumed = 0, value1 = 0;
                    getVariableWidthValue(entryData, endPos, dataStart, consumed, value1);
                    dataStart += consumed;
                    // -1 == None
                    tags.push_back(
                            S_FragmentTag(tagTable[i].tag, -1, value, tagTable[i].valuesPerEntry));
                } else {
                    tags.push_back(
                            S_FragmentTag(tagTable[i].tag, 1, -1, tagTable[i].valuesPerEntry));
                }
            } else {
                while ((tagTable[i].mask & 0x01) == 0) {
                    tagTable[i].mask >>= 1;
                    value >>= 1;
                }
                tags.push_back(
                        S_FragmentTag(tagTable[i].tag, value, -1, tagTable[i].valuesPerEntry));
            }
        }
    }

    std::map<size_t, std::vector<size_t> > tagHashMap;
    for (int i = 0; i < tags.size(); i++) {
        //LOGE("tags item: %d %d %d %d",tags[i].tag,tags[i].valueCount,tags[i].valueBytes,tags[i].valuesPerEntry);
        std::vector<size_t> values;
        if (tags[i].valueCount >= 0) {
            for (int j = 0; j < tags[i].valueCount * tags[i].valuesPerEntry; j++) {
                size_t consumed = 0, data = 0;
                getVariableWidthValue(entryData, endPos, dataStart, consumed, data);
                //LOGE("process value: %d %d %d",dataStart,consumed,data);
                dataStart += consumed;
                values.push_back(data);
            }
        } else {
            int totalConsumed = 0;
            while (totalConsumed < tags[i].valueBytes) {
                size_t consumed = 0, data = 0;
                getVariableWidthValue(entryData, endPos, dataStart, consumed, data);
                dataStart += consumed;
                totalConsumed += consumed;
                values.push_back(data);
                if(values.size()>=values.max_size()-1)
                    break;
            }
            if (totalConsumed != tags[i].valueBytes) {
                //LOGE("PalmDocStream", "Error:Should consume %d bytes,but consumed %d",tags[i].valueBytes, totalConsumed);
            }
        }
        tagHashMap[tags[i].tag] = values;
    }
    return tagHashMap;
}

bool PalmDocStream::parseINDXHeader(const char *data, int len, S_INDXHeader &indxHeader) {
    if (data[0] == 'I' && data[1] == 'N' && data[2] == 'D' && data[3] == 'X') {
        for (int i = 1; i <= 13; i++) {
            char longBytes[4] = {data[4 * i + 0], data[4 * i + 1], data[4 * i + 2],
                                 data[4 * i + 3]};
            if (i == 1)indxHeader.len = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 2)indxHeader.nul1 = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 3)indxHeader.type = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 4)indxHeader.gen = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 5)indxHeader.start = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 6)indxHeader.count = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 7)indxHeader.code = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 8)indxHeader.lng = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 9)indxHeader.total = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 10)indxHeader.ordt = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 11)indxHeader.ligt = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 12)indxHeader.nligt = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 13)indxHeader.nctoc = PdbUtil::readUnsignedLongBE(longBytes);
        }
        return true;
    }
    return false;
}

int PalmDocStream::loadDatpForDict(const char *data, long length) {
    if (data[0] == 'D' && data[1] == 'A' && data[2] == 'T' && data[3] == 'P') {
        int num = data[8];
        int k = 10;
        char longBytes[2] = {data[k + 0], data[k + 1]};
        size_t count = PdbUtil::readUnsignedShort(longBytes);
        for (int i = length-2; i >= 0; i = i - 2) {
            char longBytes[2] = {data[i + 0], data[i + 1]};
            length = i;
            if(PdbUtil::readUnsignedShort(longBytes)!=0){
                length += 2;
                break;
            }
        }
        size_t start = length - count * 2;
        size_t len = count * 2 + start;
        for (int i = start; i < len; i = i + 2) {
            char longBytes[2] = {data[i + 0], data[i + 1]};
            datp.push_back(PdbUtil::readUnsignedShort(longBytes));
        }
        //LOGI("yun cout : %d start : %d", count, start);
        return num;
    }
    return 0;
}
//bool PalmDocStream::readTagSection(char *data, S_INDXDict &indxD, long start) {
//    long offset = start;
//    if (data[offset] == 'T' && data[offset+1] == 'A' && data[offset+2] == 'G' && data[offset+3] == 'X'){
//        offset +=4;
//        char longBytes[4] = {data[offset + 0], data[offset + 1], data[offset + 2],
//                             data[offset + 3]};
//        long firstEntryOffset = PdbUtil::readUnsignedLongBE(longBytes);
//        offset +=4;
//        char longBytes2[4] = {data[offset + 0], data[offset + 1], data[offset + 2],
//                             data[offset + 3]};
//        long controlByteCount = PdbUtil::readUnsignedLongBE(longBytes2);
//
//        for(int i=offset+4;i<offset+firstEntryOffset;i=i+4){
//
//        }
//
//    }
//
//}
/**
 * 附加解析头，其中ordt2主要用于索引的特殊字符转换
 * @param data
 * @param indxD
 * @param isCode
 * @return
 */
bool PalmDocStream::parseINDXHeaderForDictiony(const char *data, S_INDXDict &indxD, bool isCode) {
    if (data[0] == 'I' && data[1] == 'N' && data[2] == 'D' && data[3] == 'X') {
        int offset = 0xa4;
        for (int i = 0; i <= 5; i++) {
            char longBytes[4] = {data[offset + 0], data[offset + 1], data[offset + 2],
                                 data[offset + 3]};
            if (i == 0)indxD.otype = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 1)indxD.oentries = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 2)indxD.op1 = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 3)indxD.op2 = PdbUtil::readUnsignedLongBE(longBytes);
            if (i == 4)indxD.otagx = PdbUtil::readUnsignedLongBE(longBytes);
            offset += 4;
        }
        if (isCode || indxD.oentries > 0) {
            int offset = indxD.op1;
            //LOGI("yun offset : %x",offset);
            //LOGI("yun indxD.otype : %d",indxD.otype);
            if (data[offset + 0] == 'O' && data[offset + 1] == 'R' && data[offset + 2] == 'D' &&
                data[offset + 3] == 'T') {
                offset = indxD.op2;
                if (data[offset + 0] == 'O' && data[offset + 1] == 'R' && data[offset + 2] == 'D' &&
                    data[offset + 3] == 'T') {
                    int start1 = indxD.op1 + 4, start2 = indxD.op2 + 4;
                    int len1 = indxD.oentries, len2 = 2 * indxD.oentries;
                    //char ordt1[len1];
                    //LOGI("yun indxD.oentries : %d",indxD.oentries);
                    //LOGI("yun indxD.oentries : %d",indxD.op2);
                    for (int i = start1; i < len1 + start1; i = i + 1) {
                        indxD.ordt1.push_back(data[i]);
                    }
                    //indxD.ordt1 = ordt1;
                    //char ordt2[len2];
                    for (int i = start2; i < len2 + start2; i = i + 2) {
                        char longBytes[2] = {data[i + 0], data[i + 1]};
                        indxD.ordt2.push_back(PdbUtil::readUnsignedShort(longBytes));
                        //LOGI("yun start2 : %d indxD.oentries : %d",start2,PdbUtil::readUnsignedShort(longBytes));
                    }
                    //indxD.ordt2 = ordt2;
                }
            }
        }
        return true;
    }
    return false;
}

//读取CTOC数据
std::map<size_t, std::string> PalmDocStream::readCTOC(int tocOffset, int nctoc) {
    int rec_off = 0;
    std::map<size_t, std::string> ctoc_data;
    for (int i = 0; i < nctoc; i++) {
        int tocindex = tocOffset + i;
        size_t tocLen = header().Offsets[tocindex + 1] - header().Offsets[tocindex];
        myBase->seek((int) header().Offsets[tocindex], true);
        char tocData[tocLen];
        myBase->read(tocData, tocLen);

        size_t offset = 0;
        while (offset < tocLen) {
            if (tocData[offset] == '\0')
                break;

            size_t idx_offs = offset;
            size_t ilen = 0, pos = 0;
            getVariableWidthValue(tocData, tocLen, offset, pos, ilen);

            offset += pos;
            std::string name(tocData, offset, ilen);
            offset += ilen;
            ctoc_data[idx_offs + rec_off] = name;
//            LOGI("toc count key-value : %d %s",idx_offs,name.data());
//            if (offset + ilen >= tocLen) break;
        }
        rec_off += 0x10000;
    }
    return ctoc_data;
}

void
PalmDocStream::getVariableWidthValue(const char *data, size_t dataLen, size_t offset,
                                     size_t &consumed,
                                     size_t &value) {
    bool finished = false;
    while (!finished && offset + consumed < dataLen) {
        char c = data[offset + consumed];
        consumed += 1;
        if (c & 0x80)
            finished = true;
        value = (value << 7) | (c & 0x7f);
    }
}


size_t
PalmDocStream::readTagSection(size_t start, const char *data, std::vector<S_SectionTag> &tags) {
    size_t controlByteCount = 0;
    size_t firstEntryOffset = 0;
    if (data[start] == 'T' && data[start + 1] == 'A' &&
        data[start + 2] == 'G' && data[start + 3] == 'X') {
        char firstChars[4] = {data[start + 0x4], data[start + 0x4 + 1], data[start + 0x4 + 2],
                              data[start + 0x4 + 3]};
        firstEntryOffset = PdbUtil::readUnsignedLongBE(firstChars);
        char secondChars[4] = {data[start + 0x8], data[start + 0x8 + 1], data[start + 0x8 + 2],
                               data[start + 0x8 + 3]};
        controlByteCount = PdbUtil::readUnsignedLongBE(secondChars);
        for (int i = 12; i < firstEntryOffset; i += 4) {
            int pos = start + i;
            S_SectionTag tag;
            tag.tag = (size_t) data[pos];
            tag.valuesPerEntry = (size_t) data[pos + 1];
            tag.mask = (size_t) data[pos + 2];
            tag.endFlag = (size_t) data[pos + 3];
            tags.push_back(tag);
        }
    }
    return controlByteCount;
}

std::string PalmDocStream::getIDTagByPosFid(std::string posfid, std::string offset) {
    long row = PdbUtil::fromBase32(posfid);
    long off = PdbUtil::fromBase32(offset);
    if (row>=0&&row < fragtbl.size()) {
        S_FragmentTable &ftbl = fragtbl[row];
        fastSeek(ftbl.filePosition+off);
        char r[400];
        read(r,400);
//        LOGI("yun r: %s row %ld off %ld offset : %s ftbl.filePosition %d",r,row,off,offset.c_str(),ftbl.filePosition);
        std::string id = r;
        int len = id.find(" id=\"");
        if(len>=0)
            len+=5;
        else{
            len = id.find(" name=\"");
            if(len>=0)
                len+=7;
            else{
                len = id.find(" filepos=\"");
                if(len>=0)
                    len+=10;
            }
        }
        if(len>=0){
            id = id.substr(len);
            if(id.find("\"")>0)
                id = id.substr(0,id.find("\""));
        } else{
            len = id.find(" filepos=");
            if(len>=0){
                id = id.substr(len+9);
                id = id.substr(0,id.find(">"));
            } else
                id = "";
        }
        return id;
    } else {
        return "";
    }
}

std::string PalmDocStream::getIDTagByPosFid(int pos_fid, int pos_off) {
    long row = pos_fid;
    long off = pos_off;
    if (row>=0&&row < fragtbl.size()) {
        S_FragmentTable &ftbl = fragtbl[row];
        fastSeek(ftbl.filePosition+off);
        char r[400];
        read(r,400);
        std::string id = r;
//        LOGI("yun r2: %s row %ld off %ld ftbl.filePosition %d",r,row,off,ftbl.filePosition);
        int len = id.find(" id=\"");
        if(len<0){
            len = id.find(" name=\"");
            if(len>=0)
                len+=7;
        } else
            len+=5;
        if(len>=0){
            id = id.substr(len);
            if(id.find("\"")>0)
                id = id.substr(0,id.find("\""));
        } else{
            id = "";
        }
        return id;
    } else {
        return "";
    }
}

long PalmDocStream::getIDPosByPosFid(std::string posfid, std::string offset) {
    long row = PdbUtil::fromBase32(posfid);
    long off = PdbUtil::fromBase32(offset);
    if (row>=0&&row < fragtbl.size()) {
        S_FragmentTable &ftbl = fragtbl[row];
        std::string idtext = ftbl.linkIdText.substr(12, ftbl.linkIdText.size() - 12 - 2);
        return ftbl.filePosition + off;
    } else {
        return off;
    }
}


bool PalmDocStream::hasExtraSections() const {
    return myMaxRecordIndex < header().Offsets.size() - 1;
}

std::pair<int, int> PalmDocStream::imageLocation(const PdbHeader &header, int index) const {
    index += myImageStartIndex;
    int recordNumber = header.Offsets.size();
    if (index > recordNumber - 1 || index < 0) {
        return std::make_pair(-1, -1);
    } else {
        size_t start = header.Offsets[index];
        size_t end = (index < recordNumber - 1) ?
                     header.Offsets[index + 1] : myBase->offset();
        return std::make_pair(start, end - start);
    }
}


PalmDocContentStream::PalmDocContentStream(const shared_ptr<ZLInputStream> inputStream, OpenType openType, std::string word,
                                           long start, long end) : PalmDocStream(inputStream,
                                                                                 openType, word,
                                                                                 start,
                                                                                 end) {
}


bool PalmDocContentStream::open() {
    if (!PalmDocStream::open()) {
        return false;
    }
    if (openType == OPEN_DICT_SEARCH) {
        //LOGI("yun indexTable : OPEN_DICT_SEARCH");
    } else if (openType == OPEN_DICT_IS_DICT) {
        return isDictionary;
    } else if (openType == OPEN_DICT_SEARCH_INDEX) {
        indexTable.clear();
        loadIndexInfoForDict(translationContext);
    } else {
        PalmDocStream::loadTableInfoForAZW();
    }
    return true;
}

size_t PalmDocContentStream::sizeOfOpened() {
    return myTextLength;
}

PalmDocCssStream::PalmDocCssStream(const shared_ptr<ZLInputStream> inputStream) : PalmDocStream(inputStream) {
}

bool PalmDocCssStream::open() {
    if (!PalmDocStream::open()) {
        return false;
    }

    //跳转到css内容的起点位置
    seek(myTextLength, false);

    /*发现《山海经故事丛书（全20册-本社.mobi》这本书的RecordIndex已经等于最大值了，但是offset()还是myTextLength；
     *也有些书offset()等于myTextLength了，但是recordIndex是小于的最大值的。所以两个条件都写上。 by bmj*/
    if((PalmDocStream::myRecordIndex)<(PalmDocStream::myMaxRecordIndex) &&
        PalmDocStream::offset() < myTextLength){
        close();
        return false;
    }
    return true;
}

//PalmDocCssStream的偏移是相对于css内容起点位置来说的，可不是0.
size_t PalmDocCssStream::offset() const {
    const size_t o = PalmDocStream::offset();
    return o <= myTextLength ? 0 : o - myTextLength;
}

size_t PalmDocCssStream::sizeOfOpened() {
    return (size_t) (1 << 31) - 1;
}
