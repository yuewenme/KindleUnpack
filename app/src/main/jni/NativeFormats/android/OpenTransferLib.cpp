//
// Created by yuewen on 19-8-8.
//

#include <NativeFormats/common/zlibrary/core/unix/filesystem/ZLUnixFileInputStream.h>
#include <NativeFormats/common/fbreader/formats/pdb/PalmDocStream.h>
#include <NativeFormats/common/fbreader/formats/css/CSSInputStream.h>
#include <ZLStringUtil.h>
#include <ZLLogger.h>
#include "OpenTransferLib.h"

PalmDocContentStream * stream;
PalmDocCssStream *cssStream;
std::string cssString;
bool isMobi8,isExistCover;
std::string getCover();

void replace (char *str)
{
    const char *src = "</body>";
    int src_len = 7;
    const char *src2 = "</html>";
    int src_len2 = 7;
    char* pos = str;
    char* pos2 = str;
    pos = strstr (pos, src);
    pos2 = strstr (pos2, src2);
    if(pos2||pos){
        if(pos&&pos2){
            if(pos2-pos>0){
                while(pos+src_len!=pos2) {
                    *pos = *(pos+src_len);
                    pos++;
                }
                while(*(pos+src_len2+src_len)) {
                    *pos = *(pos+src_len2+src_len);
                    pos++;
                }
                while(*src) {
                    *pos = *src;
                    pos++;
                    src++;
                }
                while(*src2) {
                    *pos = *src2;
                    pos++;
                    src2++;
                }
            }

        } else if(pos2){
            while(*(pos2+src_len2)) {
                *pos2 = *(pos2+src_len2);
                pos2++;
            }
            while(*src2) {
                *pos2 = *src2;
                pos2++;
                src2++;
            }

        }else{
            while(*(pos+src_len)) {
                *pos = *(pos+src_len);
                pos++;
            }
            while(*src) {
                *pos = *src;
                pos++;
                src++;
            }
        }
    }
}
void html_readjust(char *str)//html重整：azw的部分标签</html>等会直接在<body>之后该方法是将这些内容移动到末尾
{
    const char *src = "<body";
    const char *src2 = "</";
    const char *src3 = "</html>";
    int src_len3 = 7;
    char* pos = strstr (str, src);
    if(pos){
        char* pos2 = strstr(pos, src2);
        char* pos3 = strstr(pos2, src3);
        if(pos2&&pos3&&*(pos3+src_len3+1)){
            char* start = pos2;
            int len = 0;
            while(start++!=pos3)
                len++;
            len += src_len3;
            char temp[len+1];
            strncpy(temp,pos2,len);
            temp[len] = 0;
            while(*(pos2+len)) {
                *pos2 = *(pos2+len);
                pos2++;
            }
            int count = 0;
            while(temp[count]) {
                *pos2 = temp[count];
                pos2++;
                count++;
            }
        }
    }
}
void getString2(int position, int *dataLen, char *chapterData) {
    if(position<0&&isExistCover){
        strcpy(chapterData,getCover().c_str());
    } else{
        stream->fastSeek(position);
        size_t len = stream->read(chapterData, *dataLen);
        *dataLen = len;
//        if(!isMobi8)
        if(len>0)
            html_readjust(chapterData);
//        LOGI("yun position %d %d len %d %s",isMobi8,position,*dataLen,chapterData);
    }
}

static char * _itoa(char *out, int v)
{
    char buf[32], *s = out;
    unsigned int a;
    int i = 0;
    if (v < 0) {
        a = -v;
        *s++ = '-';
    } else {
        a = v;
    }
    while (a) {
        buf[i++] = (a % 10) + '0';
        a /= 10;
    }
    if (i == 0)
        buf[i++] = '0';
    while (i > 0)
        *s++ = buf[--i];
    *s = 0;
    return out;
}

static char*i2a(unsigned i, char *a, unsigned r)
{
    if (i / r > 0) {
        a = i2a(i / r, a, r);
    }
    *a = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"[i % r];
    return a + 1;
}


static char *_itoar(char *a,int i, int r)
{
    r = ((r < 2) || (r > 36)) ? 10 : r;
    if (i < 0) {
        *a = '-';
        *i2a(-i, a + 1, r) = 0;
    } else {
        *i2a(i, a, r) = 0;
    }
    return a;
}
std::string getCover() {
    std::string cover = "<div style=\"text-align:center\" aid=\"1\"><img class=\"cover\" src=\"kindle:embed:";
    char buf[32];
    cover = cover+_itoar(buf,(int)stream->coverOffset+1,isMobi8?32:16)+"?mime=image/jpg\"/></div>";
    return cover;
}
char *getString(int position, int len, int *dataLen) {
    //stream->fastSeek(position);
    char *chapterData = (char *) malloc(len);
    *dataLen = stream->realRead(position, chapterData, len);
    //LOGI("yun %d %d", *dataLen, len);
    return chapterData;
}
int getChapterIndex(int position){
    if (isMobi8) {
        int start = 0;
        for (int i = 0; i < stream->skeltbl.size(); ++i) {
            start += stream->skeltbl[i].fragtblRecordCount;
            if(stream->skeltbl[i].startPosition > position)
                return i-1;
        }
        return stream->skeltbl.size()-1;
    } else {
        for (int i = 0; i < stream->ncxtbl.size(); ++i) {
            if(stream->ncxtbl[i].position > position)
                return i-1;
        }
        return stream->ncxtbl.size()-1;
    }
}

std::string parseStream(shared_ptr<ZLInputStream> cssStream) {
    //ZLLogger::Instance().registerClass("oeb");
    std::string string;
    cssStream = new CSSInputStream(cssStream);
    if (cssStream->open()) {
        char *buffer = new char[1024];
        while (true) {
            memset(buffer, 0, 1024);
            int len = cssStream->read(buffer, 1024);
            if (len == 0) {
                break;
            }
            string.append(buffer);
        }
        delete[] buffer;
    }
    return string;
}

void CreateFile(const char *path) {
//    LOGI("yun create file : %s",path);
    shared_ptr<ZLInputStream> inputStream = new ZLUnixFileInputStream(path);
    stream = new PalmDocContentStream(inputStream);
    shared_ptr<ZLInputStream> inputStream2 = new ZLUnixFileInputStream(path);
    cssStream = new PalmDocCssStream(inputStream2);
}

int Open() {
    if(!stream->open())
        return -1;
    isMobi8 = stream->isMobi8();
//    LOGI("yun version %ld",stream->getKindleVersion());
    if (isMobi8) {
        if(cssStream->open())
            cssStream->loadCssTableForAZW();
    } else{
        if(stream->ncxtbl.size()==0){
            int result = stream->loadTableOfContents();
            if(!result)
                return -1;
        }
        cssString = parseStream(cssStream);
    }
    isExistCover = stream->coverOffset != 0xffffffff;
    return 0;
}

void GetCssDataLen(char *value, int *position, int *dataLen) {
    if (isMobi8) {
        if(value){
            const std::string tag = value;
            if (tag.find("text/css") >= 0) {
                int start = tag.find("flow:");
                int end = tag.find("?");
                std::string indexStr = tag.substr(start + 5, end - start - 5);
                int index = PdbUtil::fromBase32(indexStr) - 1;
                if (index <= cssStream->fdsttbl.size()) {
                    *position = cssStream->fdsttbl[index];
                    *dataLen = cssStream->fdsttbl[index + 1] - cssStream->fdsttbl[index];
                }
            }
        } else{
            *dataLen =*position = 0;
        }
//        LOGI("yun position %d %d",*position,*dataLen);
    } else {
        *position = -1;
        *dataLen = cssString.size();
    }
}

char *GetCssData(int position, int *dataLen, char *data) {
    if (!isMobi8) {
        return const_cast<char *>(cssString.c_str());
    } else {
        cssStream->fastSeek(position);
        *dataLen = cssStream->read(data, *dataLen);
        //LOGI("yun position  %d len %d data : %s", position, *dataLen, data);
    }
    return 0;
}

int GetCssDataNumber() {
    if (isMobi8) {
        return cssStream->fdsttbl.size();
    } else {
        return 1;
    }

}

char *GetChapterData(int index, int *dataLen) {
    char *chapterData = getString(stream->ncxtbl[index].position,
            stream->ncxtbl[index].len,dataLen);
    return chapterData;
}

void GetChapterDataLen(int index, int *position, int *dataLen) {
    if(isExistCover){
        if(index){
            index--;
        } else{
            *position=-1;
            *dataLen = getCover().size()+1;
            return;
        }
    }
    if (isMobi8) {
        int start = 0, count;
        for (int i = 0; i < index; ++i) {
            start += stream->skeltbl[i].fragtblRecordCount;
        }
        count = stream->skeltbl[index].fragtblRecordCount;
        *position = stream->skeltbl[index].startPosition ;
        *dataLen = stream->skeltbl[index].length;
        for (int i = start; i < start + count; ++i) {
            *dataLen += stream->fragtbl[i].length;
        }
//        LOGI("yun position %d datalen %d name %s",*position,*dataLen,stream->skeltbl[index].skeltonName.c_str());
//        LOGI("yun count %d length %d co %d",count,stream->skeltbl[index].length,*dataLen);
    } else {
        *position = stream->ncxtbl[index].position;
        *dataLen = stream->ncxtbl[index].len;
//        LOGI("yun position %d datalen %d text : %s ",*position,*dataLen,stream->ncxtbl[index].text.c_str());
    }
}

void GetChapterData2(int index, int position, int *dataLen, char *data) {
    getString2(position, dataLen, data);
}
void GetNcxInfo(int index,int *chapterIndex, int *parentIndex, char *pos_aid, char *title) {
    if(index<stream->ncxtbl.size()){
        *parentIndex = stream->ncxtbl[index].parent;
        /**
         * 当为azw是通过position查找章节编号
         */
        *chapterIndex = isMobi8 ? getChapterIndex(stream->ncxtbl[index].position) : index;
        if(stream->ncxtbl[index].pos_fid>0&&stream->ncxtbl[index].pos_off>0){
            std::string str_fid;
            PdbUtil::toBase32(&str_fid,stream->ncxtbl[index].pos_fid);
//            LOGI("yun str_num %s",str_fid.c_str());
            strcpy(pos_aid,"fid:");
            strcat(pos_aid,str_fid.c_str());
            strcat(pos_aid,":off:00");
            std::string str_off;
            PdbUtil::toBase32(&str_off,stream->ncxtbl[index].pos_off);
            strcat(pos_aid,str_off.c_str());
        }
        if(!stream->ncxtbl[index].text.empty())
            strcpy(title,stream->ncxtbl[index].text.c_str());
    }
    if(isExistCover)
        (*chapterIndex)++;
}

int GetNcxLen(){
    return stream->ncxtbl.size();
}
void getImage(int position, int len, char *chapterImage, int *dataLen) {
    *dataLen = stream->realRead(position, chapterImage, len);
}

int GetChapterDataNumber() {
    return (isMobi8 ? stream->skeltbl.size() : stream->ncxtbl.size()) + isExistCover;
}

void FreeChapterData() {
    if (stream) {
        delete stream;
        stream = 0;
    }
}
void FreeCssData() {
    if (cssStream) {
        delete cssStream;
        cssStream = 0;
    }
}

void FreeFile() {
    if(isMobi8)
        FreeCssData();
    FreeChapterData();
}

void GetImageDataLen(char *name, char *value, int *position, int *dataLen) {
    int index = -1;
    const std::string aName = name;
    if (aName == "recindex") {
        index = ZLStringUtil::parseDecimal(value, -1);
    } else if (aName == "src") {
        static const std::string KINDLE_EMBED_PREFIX = "kindle:embed:";
        std::string aValue = value;
        if (ZLStringUtil::stringStartsWith(aValue, KINDLE_EMBED_PREFIX)) {
            aValue = aValue.substr(KINDLE_EMBED_PREFIX.length());
            const size_t q = aValue.find('?');
            if (q != std::string::npos) {
                aValue = aValue.substr(0, q);
            }
            if (!isMobi8) {
                index = ZLStringUtil::parseHex(aValue, -1);
            } else {
                index = PdbUtil::fromBase32(aValue);
            }
        }
//        index =4;
    } else {

    }
    if (index >= 0) {
        std::pair<int, int> imageLocation = stream->imageLocation(stream->header(), index - 1);
        if (imageLocation.first > 0 && imageLocation.second > 0) {
            *dataLen = imageLocation.second;
            *position = imageLocation.first;
        }
    }
}

void GetLinkData(char *value, char * link) {
    const std::string href = value;   //azw3 里没有filepos kindle:pos:fid:005N:off:0000000012
    int off = href.find(":off:");
    if (off <= 0) {
        const int intValue = ZLStringUtil::parseDecimal(href, -1);
        if (intValue > 0) {
            stream->fastSeek(intValue);
            char r[400];
            stream->read(r,400);
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
            std::string n;
            char v[32];
            n.append(_itoa(v,getChapterIndex(intValue)+(isExistCover?1:0))).append("#").append(id);
            strcpy(link,n.c_str());
        }
    } else {
        int lastColon = href.rfind(":off:");  //最后一个冒号
        std::string offset = href.substr(lastColon + 5, href.size() - lastColon - 5);
        int pre = href.substr(0, lastColon).find_last_of(':');
        std::string fid = href.substr(pre + 1, lastColon - pre - 1);
        std::string name = stream->getIDTagByPosFid(fid, offset);
        int p = stream->getIDPosByPosFid(fid, offset);
        std::string n;
        char v[32];
        n.append(_itoa(v,getChapterIndex(p)+(isExistCover?1:0))).append("#").append(name);
        strcpy(link,n.c_str());
    }
}
void GetOriginalResolution(float * w,float * h){
    if(stream->originalResolution.size()>0){
        int start = stream->originalResolution.find('x');
        if(start>0){
            std::string width = stream->originalResolution.substr(0,start);
            std::string height = stream->originalResolution.substr(start+1);
            *w = atoi(width.data());
            *h = atoi(height.data());
        }
    }
//    LOGI("yun originalResolution %s ",stream->originalResolution.data());
}
void GetImageData(int position, int *dataLen, char *data) {
    getImage(position, *dataLen, data, dataLen);
}

int test(PalmDocStream *palmDocStream) {
    //stream = palmDocStream;
}