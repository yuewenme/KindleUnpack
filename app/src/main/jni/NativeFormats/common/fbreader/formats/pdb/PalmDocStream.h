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

#ifndef __PALMDOCSTREAM_H__
#define __PALMDOCSTREAM_H__
#include <ZLUnicodeUtil.h>
#include "PalmDocLikeStream.h"
#include <map>

enum OpenType {
	OPEN_NONE,
	OPEN_DICT_IS_DICT,
	OPEN_DICT_SEARCH_INDEX,
	OPEN_DICT_SEARCH,
};
class HuffDecompressor;

class PalmDocStream : public PalmDocLikeStream {

public:
    typedef struct INDXHeader{
        size_t len,nul1,type,gen,start,count,code,lng,total,ordt,ligt,nligt,nctoc;
    }S_INDXHeader;
	typedef struct INDXHeaderForDictionary{
        size_t otype = 1, oentries, op1, op2, otagx;
        std::string ordt1;
        std::vector<size_t> ordt2 ;
    }S_INDXDict;
	typedef struct SectionTag{
		size_t tag,valuesPerEntry,mask,endFlag;
	} S_SectionTag;

	typedef struct FragmentTag{
		int tag,valueCount;
		int valueBytes;
		int valuesPerEntry;
		FragmentTag(int tag1,int valueCount1,int valueBytes1,int valuesPerEntry1){
			tag = tag1;
			valueBytes = valueBytes1;
			valueCount = valueCount1;
			valuesPerEntry = valuesPerEntry1;
		}
	}S_FragmentTag;

	typedef struct TableInfo{
		std::string text;
		std::map<size_t ,std::vector<size_t> > tagMap;
		TableInfo (std::string text1,std::map<size_t ,std::vector<size_t> >& tagMap1){
            text =  text1;
			tagMap = tagMap1;
		}
	}S_TableInfo;

	typedef struct FragmentTable{
		int filePosition;  //相对与整个azw文件来说的
		std::string linkIdText;
		size_t fileNum;
		size_t sequenceNum;
		size_t startPosition;  //相对于当前fileNum文件来说的
		size_t length;
		FragmentTable(int filePosition1,std::string linkIdText1,
					  size_t fileNum1,size_t sequenceNum1,size_t startPosition1,size_t length1){
			filePosition = filePosition1;
			linkIdText = linkIdText1;
			fileNum = fileNum1;
			sequenceNum = sequenceNum1;
			startPosition = sequenceNum1;
			length = length1;
		}
	}S_FragmentTable;
    typedef struct IndexTable{
        std::string  fileName;
        size_t startPosition;  //相对于当前fileNum文件来说的
        size_t length;
        IndexTable(std::string fileName1,size_t startPosition1,size_t length1){
            fileName = fileName1;
            startPosition = startPosition1;
            length = length1;
        }
    }S_IndexTable;
    typedef struct SkeltonTable{
        int fileNum;
        std::string skeltonName;
        int fragtblRecordCount;
        int startPosition;
        int length;
        SkeltonTable(int fileNum1,std::string name,int recordCount,int start,int len){
            fileNum = fileNum1;
            skeltonName = name;
            fragtblRecordCount = recordCount;
            startPosition = start;
            length = len;
        }
    }S_SkeltonTable;

    typedef struct NCXInfo{
        int index;
		int position;
		int len;
        int parent;
        int firstchild;
        int endchild;
        int pos_fid;
        int pos_off;
        std::string pos_aid;
        std::string text;
    }S_NCXInfo;

protected:
	PalmDocStream(const shared_ptr<ZLInputStream> inputStream,OpenType openType = OPEN_NONE,std::string = "",long start = 0,long end = 0);
public:
	~PalmDocStream();

	std::pair<int,int> imageLocation(const PdbHeader &header, int index) const;
	bool hasExtraSections() const;
    std::string getIDTagByPosFid(std::string posfid,std::string offset);
    std::string getIDTagByPosFid(int pos_fid,int pos_off);
	long getIDPosByPosFid(std::string posfid,std::string offset);
	unsigned long getKindleVersion();
	bool isMobi8();
    void loadTableInfoForAZW();
	bool loadTableOfContents();
    bool loadCssTableForAZW();
    bool loadIndexInfoForDict(std::string wordName);
	ZLUnicodeUtil::Ucs2String getIndexString(size_t startPos,S_INDXDict indxD,char hdr[]);
	size_t getIndexTable(size_t startPos,size_t endPos,S_INDXDict indxD,char hdr[],size_t controlByteCount,std::vector<S_SectionTag> tagTable,ZLUnicodeUtil::Ucs2String name,std::vector<IndexTable> indexTable);
	int loadDatpForDict(const char* data,long length);
	bool fastSeek(size_t offset);

	int realRead(size_t offset,char *buffer,size_t len);
private:
	bool processRecord();
    bool loadSection(std::string,int i);
	bool processZeroRecord();
    std::map<size_t,std::string> readCTOC(int tocOffset,int nctoc);
    bool parseINDXHeader(const char* data,int len,S_INDXHeader& indxHeader);
	bool parseINDXHeaderForDictiony(const char* data,S_INDXDict& indxD,bool);
	size_t readTagSection(size_t start,const char* data,std::vector<S_SectionTag>& tags);
	std::map<size_t ,std::vector<size_t > > getTagMap(size_t controlByteCount,std::vector<S_SectionTag>& tagTable,const char* entryData,size_t startPos,size_t endPos);
	void getVariableWidthValue(const char* data,size_t dataLen,size_t offset,size_t& consumed,size_t& value);
	bool getIndexData(size_t tableIndex,std::vector<S_TableInfo>& outtbl,std::map<size_t,std::string>& ctoc_text);
private:
	unsigned short myCompressionVersion;
	unsigned short myTextRecordNumber;
	unsigned short myImageStartIndex;
	unsigned long myNcxIndex;
	unsigned long myFDSTIndex;
	unsigned long myFragmentIndex;
	unsigned long mySkeletonIndex;
	//unsigned long myGuideIndex;
    std::string myCodec;
	unsigned long myKindleVersion;   //kindle文件格式版本号，等于8就是azw3格式
	unsigned long myMetaOrthIndex;   //当书籍有索引时，该值表示索引起点 长度4字节
	unsigned long myMetainflindex;   //书籍有索引时，该值表示索引变形的起点 长度4字节
	std::vector<size_t> datp;		//词典每一段的大小对高压缩模式有效

	shared_ptr<HuffDecompressor> myHuffDecompressorPtr;
public:
	unsigned long  myTextLength;

    std::vector<FragmentTable> fragtbl;   //正文块信息表
    std::vector<S_SkeltonTable> skeltbl;  //文件信息表
    std::vector<S_NCXInfo> ncxtbl;        //目录信息表
	std::vector<size_t> fdsttbl;          //css或svg文件起止位置表
    unsigned long coverOffset;
	std::string originalResolution;
	OpenType openType;
	std::string translationContext;
    long start;
    long end;
    std::vector<IndexTable> indexTable;
	bool isDictionary;   //是否为词典

};

class PalmDocContentStream : public PalmDocStream {

public:
	PalmDocContentStream(const shared_ptr<ZLInputStream> inputStream,OpenType openType = OPEN_NONE,std::string = "",long start = 0,long end = 0);
	bool open();
private:
	size_t sizeOfOpened();
};

class PalmDocCssStream : public PalmDocStream {

public:
	PalmDocCssStream(const shared_ptr<ZLInputStream> inputStream);
    bool open();

public:
	size_t sizeOfOpened();
	size_t offset() const;
};
//inline const jobject &PalmDocStream::jobject() const { return *myBookModel->getJavaModel(); }
//inline const OpenType &PalmDocStream::openType()  const {
//	if(myBookModel==0)
//		return OPEN_NONE;
//	return myBookModel->openType(); }
inline unsigned long PalmDocStream::getKindleVersion() { return myKindleVersion;}
inline bool PalmDocStream::isMobi8() {
	return header().Start!=0||myKindleVersion==8;}
#endif /* __PALMDOCSTREAM_H__ */
