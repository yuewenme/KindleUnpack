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

#include <cctype>
#include <cstdlib>
#include <map>

#include <ZLLogger.h>

#include "ZLUnicodeUtil.h"

struct ZLUnicodeData {
    enum SymbolType {
        LETTER_LOWERCASE,
        LETTER_UPPERCASE,
        LETTER_OTHER,
        UNKNOWN
    };

    const SymbolType Type;
    const ZLUnicodeUtil::Ucs4Char LowerCase;
    const ZLUnicodeUtil::Ucs4Char UpperCase;

    ZLUnicodeData(const SymbolType type, ZLUnicodeUtil::Ucs4Char lowerCase,
                  ZLUnicodeUtil::Ucs4Char upperCase);
};

ZLUnicodeData::ZLUnicodeData(const SymbolType type, ZLUnicodeUtil::Ucs4Char lowerCase,
                             ZLUnicodeUtil::Ucs4Char upperCase) : Type(type), LowerCase(lowerCase),
                                                                  UpperCase(upperCase) {
}

/*static std::map<ZLUnicodeUtil::Ucs4Char,ZLUnicodeData> UNICODE_TABLE;

class ZLUnicodeTableReader : public ZLXMLReader {

private:
	void startElementHandler(const char *tag, const char **attributes);
};

void ZLUnicodeTableReader::startElementHandler(const char *tag, const char **attributes) {
	static std::string SYMBOL_TAG = "symbol";
	static std::string LETTER_LOWERCASE_TYPE = "Ll";
	static std::string LETTER_UPPERCASE_TYPE = "Lu";

	if (SYMBOL_TAG == tag) {
		const char *codeS = attributeValue(attributes, "code");
		const ZLUnicodeUtil::Ucs4Char code = strtol(codeS, 0, 16);
		const char *typeS = attributeValue(attributes, "type");
		ZLUnicodeData::SymbolType type = ZLUnicodeData::UNKNOWN;
		if (LETTER_LOWERCASE_TYPE == typeS) {
			type = ZLUnicodeData::LETTER_LOWERCASE;
		} else if (LETTER_UPPERCASE_TYPE == typeS) {
			type = ZLUnicodeData::LETTER_UPPERCASE;
		} else if (typeS != 0 && *typeS == 'L') {
			type = ZLUnicodeData::LETTER_OTHER;
		}
		const char *lowerS = attributeValue(attributes, "lower");
		const ZLUnicodeUtil::Ucs4Char lower = lowerS != 0 ? std::strtol(lowerS, 0, 16) : code;
		const char *upperS = attributeValue(attributes, "upper");
		const ZLUnicodeUtil::Ucs4Char upper = upperS != 0 ? std::strtol(upperS, 0, 16) : code;
		UNICODE_TABLE.insert(std::make_pair(code, ZLUnicodeData(type, lower, upper)));
	}
}

static void initUnicodeTable() {
	static bool inProgress = false;
	if (!inProgress && UNICODE_TABLE.empty()) {
		inProgress = true;
		ZLUnicodeTableReader reader;
		reader.readDocument(ZLFile(ZLibrary::ZLibraryDirectory() + ZLibrary::FileNameDelimiter + "unicode.xml"));
		inProgress = false;
	}
}
*/

bool ZLUnicodeUtil::isUtf8String(const char *str, int len) {
    const char *last = str + len;
    int nonLeadingCharsCounter = 0;
    for (; str < last; ++str) {
        if (nonLeadingCharsCounter == 0) {
            if ((*str & 0x80) != 0) {
                if ((*str & 0xE0) == 0xC0) {
                    nonLeadingCharsCounter = 1;
                } else if ((*str & 0xF0) == 0xE0) {
                    nonLeadingCharsCounter = 2;
                } else if ((*str & 0xF8) == 0xF0) {
                    nonLeadingCharsCounter = 3;
                } else {
                    return false;
                }
            }
        } else {
            if ((*str & 0xC0) != 0x80) {
                return false;
            }
            --nonLeadingCharsCounter;
        }
    }
    return nonLeadingCharsCounter == 0;
}

bool ZLUnicodeUtil::isUtf8String(const std::string &str) {
    return isUtf8String(str.data(), str.length());
}

void ZLUnicodeUtil::cleanUtf8String(std::string &str) {
    int charLength = 0;
    int processed = 0;
    for (std::string::iterator it = str.begin(); it != str.end();) {
        if (charLength == processed) {
            if ((*it & 0x80) == 0) {
                ++it;
            } else if ((*it & 0xE0) == 0xC0) {
                charLength = 2;
                processed = 1;
                ++it;
            } else if ((*it & 0xF0) == 0xE0) {
                charLength = 3;
                processed = 1;
                ++it;
            } else if ((*it & 0xF8) == 0xF0) {
                charLength = 4;
                processed = 1;
                ++it;
            } else {
                it = str.erase(it);
            }
        } else if ((*it & 0xC0) == 0x80) {
            ++processed;
            ++it;
        } else {
            it -= processed;
            do {
                it = str.erase(it);
            } while (--processed);
            charLength = 0;
        }
    }
}
void ZLUnicodeUtil::changeUtf8String(std::string &str) {
    int charLength = 0;
    int processed = 0;
    for (std::string::iterator it = str.begin(); it != str.end();) {
        if (charLength == processed) {
            if ((*it & 0x80) == 0) {
                ++it;
            } else if ((*it & 0xE0) == 0xC0) {
                charLength = 2;
                processed = 1;
                ++it;
            } else if ((*it & 0xF0) == 0xE0) {
                charLength = 3;
                processed = 1;
                ++it;
            } else if ((*it & 0xF8) == 0xF0) {
                charLength = 4;
                processed = 1;
                ++it;
            } else {
                size_t ch = *it;
                if (ch < 0x80) {
                    *it = (char) ch;
                } else if (ch < 0x800) {
                    *it = (char) (0xC0 | (ch >> 6));
                    it = str.insert(it+1,(char) (0x80 | (ch & 0x3F)));
                } else {
                    *it = (char) (0xE0 | ch >> 12);
                    it = str.insert(it+1,(char) (0x80 | ((ch >> 6) & 0x3F)));
                    it = str.insert(it+1,(char) (0x80 | (ch & 0x3F)));
                }
                ++it;
            }
        } else if ((*it & 0xC0) == 0x80) {
            ++processed;
            ++it;
        } else {
            it -= processed;
            do {
                size_t ch = *it;
                if (ch < 0x80) {
                    *it = (char) ch;

                } else if (ch < 0x800) {
                    *it = (char) (0xC0 | (ch >> 6));
                    it = str.insert(it+1,(char) (0x80 | (ch & 0x3F)));

                } else {
                    *it = (char) (0xE0 | ch >> 12);
                    it = str.insert(it+1,(char) (0x80 | ((ch >> 6) & 0x3F)));
                    it = str.insert(it+1,(char) (0x80 | (ch & 0x3F)));
                }
                ++it;
            } while (--processed);
            charLength = 0;
        }
    }
}
int ZLUnicodeUtil::utf8Length(const char *str, int len) {
    const char *last = str + len;
    int counter = 0;
    while (str < last) {
        if ((*str & 0x80) == 0) {
            ++str;
        } else if ((*str & 0x20) == 0) {
            str += 2;
        } else if ((*str & 0x10) == 0) {
            str += 3;
        } else {
            str += 4;
        }
        ++counter;
    }
    return counter;
}

int ZLUnicodeUtil::utf8Length(const std::string &str) {
    return utf8Length(str.data(), str.length());
}

int ZLUnicodeUtil::length(const char *str, int utf8Length) {
    const char *ptr = str;
    for (int i = 0; i < utf8Length; ++i) {
        if ((*ptr & 0x80) == 0) {
            ++ptr;
        } else if ((*ptr & 0x20) == 0) {
            ptr += 2;
        } else if ((*ptr & 0x10) == 0) {
            ptr += 3;
        } else {
            ptr += 4;
        }
    }
    return ptr - str;
}

int ZLUnicodeUtil::length(const std::string &str, int utf8Length) {
    return length(str.data(), utf8Length);
}

void ZLUnicodeUtil::utf8ToUcs4(Ucs4String &to, const char *from, int length, int toLength) {
    to.clear();
    if (toLength < 0) {
        toLength = utf8Length(from, length);
    }
    to.reserve(toLength);
    const char *last = from + length;
    for (const char *ptr = from; ptr < last;) {
        if ((*ptr & 0x80) == 0) {
            to.push_back(*ptr);
            ++ptr;
        } else if ((*ptr & 0x20) == 0) {
            Ucs4Char ch = *ptr & 0x1f;
            ++ptr;
            ch <<= 6;
            ch += *ptr & 0x3f;
            to.push_back(ch);
            ++ptr;
        } else if ((*ptr & 0x10) == 0) {
            Ucs4Char ch = *ptr & 0x0f;
            ++ptr;
            ch <<= 6;
            ch += *ptr & 0x3f;
            ++ptr;
            ch <<= 6;
            ch += *ptr & 0x3f;
            to.push_back(ch);
            ++ptr;
        } else {
            // symbol number is > 0xffff :(
            to.push_back('X');
            ptr += 4;
        }
    }
}

void ZLUnicodeUtil::utf8ToUcs4(Ucs4String &to, const std::string &from, int toLength) {
    utf8ToUcs4(to, from.data(), from.length(), toLength);
}

void ZLUnicodeUtil::utf8ToUcs2(Ucs2String &to, const char *from, int length, int toLength) {
    to.clear();
    if (toLength < 0) {
        toLength = utf8Length(from, length);
    }
    to.reserve(toLength);
    const char *last = from + length;
    for (const char *ptr = from; ptr < last;) {
        if ((*ptr & 0x80) == 0) {
            to.push_back(*ptr);
            ++ptr;
        } else if ((*ptr & 0x20) == 0) {
            Ucs2Char ch = *ptr & 0x1f;
            ++ptr;
            ch <<= 6;
            ch += *ptr & 0x3f;
            to.push_back(ch);
            ++ptr;
        } else if ((*ptr & 0x10) == 0) {
            Ucs2Char ch = *ptr & 0x0f;
            ++ptr;
            ch <<= 6;
            ch += *ptr & 0x3f;
            ++ptr;
            ch <<= 6;
            ch += *ptr & 0x3f;
            to.push_back(ch);
            ++ptr;
        } else {
            // symbol number is > 0xffff :(
            to.push_back('X');
            ptr += 4;
        }
    }
}

void ZLUnicodeUtil::utf8ToUcs2(Ucs2String &to, const std::string &from, int toLength) {
    utf8ToUcs2(to, from.data(), from.length(), toLength);
}

std::size_t ZLUnicodeUtil::firstChar(Ucs4Char &ch, const std::string &utf8String) {
    return firstChar(ch, utf8String.c_str());
}

std::size_t ZLUnicodeUtil::firstChar(Ucs4Char &ch, const char *utf8String) {
    if ((*utf8String & 0x80) == 0) {
        ch = *utf8String;
        return 1;
    } else if ((*utf8String & 0x20) == 0) {
        ch = *utf8String & 0x1f;
        ch <<= 6;
        ch += *(utf8String + 1) & 0x3f;
        return 2;
    } else {
        ch = *utf8String & 0x0f;
        ch <<= 6;
        ch += *(utf8String + 1) & 0x3f;
        ch <<= 6;
        ch += *(utf8String + 2) & 0x3f;
        return 3;
    }
}

std::size_t ZLUnicodeUtil::lastChar(Ucs4Char &ch, const std::string &utf8String) {
    return lastChar(ch, utf8String.data() + utf8String.length());
}

std::size_t ZLUnicodeUtil::lastChar(Ucs4Char &ch, const char *utf8String) {
    const char *ptr = utf8String - 1;
    while ((*ptr & 0xC0) == 0x80) {
        --ptr;
    }
    switch (utf8String - ptr) {
        default:
            ch = '?';
            break;
        case 1:
            ch = *ptr;
            break;
        case 2:
            ch = *ptr & 0x1f;
            ch <<= 6;
            ch += *(ptr + 1) & 0x3f;
            break;
        case 3:
            ch = *ptr & 0x0f;
            ch <<= 6;
            ch += *(ptr + 1) & 0x3f;
            ch <<= 6;
            ch += *(ptr + 2) & 0x3f;
            break;
    }
    return utf8String - ptr;
}

int ZLUnicodeUtil::ucs4ToUtf8(char *to, Ucs4Char ch) {
    if (ch < 0x80) {
        *to = (char) ch;
        return 1;
    } else if (ch < 0x800) {
        *to = (char) (0xC0 | (ch >> 6));
        *(to + 1) = (char) (0x80 | (ch & 0x3F));
        return 2;
    } else {
        *to = (char) (0xE0 | ch >> 12);
        *(to + 1) = (char) (0x80 | ((ch >> 6) & 0x3F));
        *(to + 2) = (char) (0x80 | (ch & 0x3F));
        return 3;
    }
}

void ZLUnicodeUtil::ucs4ToUtf8(std::string &to, const Ucs4String &from, int toLength) {
    char buffer[3];
    to.erase();
    if (toLength > 0) {
        to.reserve(toLength);
    }
    for (Ucs4String::const_iterator it = from.begin(); it != from.end(); ++it) {
        to.append(buffer, ucs4ToUtf8(buffer, *it));
    }
}

int ZLUnicodeUtil::ucs2ToUtf8(char *to, Ucs2Char ch) {
    if (ch < 0x80) {
        *to = (char) ch;
        return 1;
    } else if (ch < 0x800) {
        *to = (char) (0xC0 | (ch >> 6));
        *(to + 1) = (char) (0x80 | (ch & 0x3F));
        return 2;
    } else {
        *to = (char) (0xE0 | ch >> 12);
        *(to + 1) = (char) (0x80 | ((ch >> 6) & 0x3F));
        *(to + 2) = (char) (0x80 | (ch & 0x3F));
        return 3;
    }
}

void ZLUnicodeUtil::ucs2ToUtf8(std::string &to, const Ucs2String &from, int toLength) {
    char buffer[3];
    to.erase();
    if (toLength > 0) {
        to.reserve(toLength);
    }
    for (Ucs2String::const_iterator it = from.begin(); it != from.end(); ++it) {

        to.append(buffer, ucs2ToUtf8(buffer, *it));
    }
}

/*
bool ZLUnicodeUtil::isLetter(Ucs4Char ch) {
	initUnicodeTable();
	std::map<ZLUnicodeUtil::Ucs4Char,ZLUnicodeData>::const_iterator it = UNICODE_TABLE.find(ch);
	if (it == UNICODE_TABLE.end()) {
		return false;
	}
	switch (it->second.Type) {
		case ZLUnicodeData::LETTER_LOWERCASE:
		case ZLUnicodeData::LETTER_UPPERCASE:
		case ZLUnicodeData::LETTER_OTHER:
			return true;
		default:
			return false;
	}
}
*/

bool ZLUnicodeUtil::isSpace(Ucs4Char ch) {
    return
            ((9 <= ch) && (ch <= 13)) ||
            (ch == 32) ||
            //(ch == 160) ||
            (ch == 5760) ||
            ((8192 <= ch) && (ch <= 8203)) ||
            (ch == 8232) ||
            (ch == 8233) ||
            (ch == 8239) ||
            (ch == 8287) ||
            (ch == 12288);
}

ZLUnicodeUtil::Breakable ZLUnicodeUtil::isBreakable(Ucs4Char c) {
    if (c <= 0x2000) {
        return NO_BREAKABLE;
    }

    if (((c < 0x2000) || (c > 0x2006)) &&
        ((c < 0x2008) || (c > 0x2046)) &&
        ((c < 0x207D) || (c > 0x207E)) &&
        ((c < 0x208D) || (c > 0x208E)) &&
        ((c < 0x2329) || (c > 0x232A)) &&
        ((c < 0x3001) || (c > 0x3003)) &&
        ((c < 0x3008) || (c > 0x3011)) &&
        ((c < 0x3014) || (c > 0x301F)) &&
        ((c < 0xFD3E) || (c > 0xFD3F)) &&
        ((c < 0xFE30) || (c > 0xFE44)) &&
        ((c < 0xFE49) || (c > 0xFE52)) &&
        ((c < 0xFE54) || (c > 0xFE61)) &&
        ((c < 0xFE6A) || (c > 0xFE6B)) &&
        ((c < 0xFF01) || (c > 0xFF03)) &&
        ((c < 0xFF05) || (c > 0xFF0A)) &&
        ((c < 0xFF0C) || (c > 0xFF0F)) &&
        ((c < 0xFF1A) || (c > 0xFF1B)) &&
        ((c < 0xFF1F) || (c > 0xFF20)) &&
        ((c < 0xFF3B) || (c > 0xFF3D)) &&
        ((c < 0xFF61) || (c > 0xFF65)) &&
        (c != 0xFE63) &&
        (c != 0xFE68) &&
        (c != 0x3030) &&
        (c != 0x30FB) &&
        (c != 0xFF3F) &&
        (c != 0xFF5B) &&
        (c != 0xFF5D)) {
        return NO_BREAKABLE;
    }

    if (((c >= 0x201A) && (c <= 0x201C)) ||
        ((c >= 0x201E) && (c <= 0x201F))) {
        return BREAKABLE_BEFORE;
    }
    switch (c) {
        case 0x2018:
        case 0x2039:
        case 0x2045:
        case 0x207D:
        case 0x208D:
        case 0x2329:
        case 0x3008:
        case 0x300A:
        case 0x300C:
        case 0x300E:
        case 0x3010:
        case 0x3014:
        case 0x3016:
        case 0x3018:
        case 0x301A:
        case 0x301D:
        case 0xFD3E:
        case 0xFE35:
        case 0xFE37:
        case 0xFE39:
        case 0xFE3B:
        case 0xFE3D:
        case 0xFE3F:
        case 0xFE41:
        case 0xFE43:
        case 0xFE59:
        case 0xFE5B:
        case 0xFE5D:
        case 0xFF08:
        case 0xFF3B:
        case 0xFF5B:
        case 0xFF62:
            return BREAKABLE_BEFORE;
    }
    return BREAKABLE_AFTER;
}

/**
 * 特殊字符转换 标点符号返回 0 大写字母返回小写
 * @param c
 * @return
 */
size_t ZLUnicodeUtil::changeUcs2Char(Ucs2Char c) {
    if (((c >= 0xFF00) && (c <= 0xFFEF)) || //全角ASCII、全角中英文标点、半宽片假名、半宽平假名、半宽韩文字母：FF00-FFEF
        ((c >= 0x2E80) && (c <= 0x2EFF)) || //CJK部首补充：2E80-2EFF
        ((c >= 0x3000) && (c <= 0x303F)) ||//CJK标点符号：3000-303F
        ((c >= 0x31C0) && (c <= 0x31EF)) ||//CJK笔划：31C0-31EF
        ((c >= 0x2F00) && (c <= 0x2FDF)) ||//康熙部首：2F00-2FDF
        ((c >= 0x2FF0) && (c <= 0x2FFF)) ||//汉字结构描述字符：2FF0-2FFF
        ((c >= 0x3100) && (c <= 0x312F)) ||//8）注音符号：3100-312F
        ((c >= 0x31A0) && (c <= 0x31BF)) ||//9）注音符号（闽南语、客家语扩展）：31A0-31BF
        ((c >= 0x3040) && (c <= 0x309F)) ||//10）日文平假名：3040-309F
        ((c >= 0x30A0) && (c <= 0x30FF)) ||//11）日文片假名：30A0-30FF
        ((c >= 0x31F0) && (c <= 0x31FF)) ||//12）日文片假名拼音扩展：31F0-31FF
        ((c >= 0xAC00) && (c <= 0xD7AF)) ||//13）韩文拼音：AC00-D7AF
        ((c >= 0x1100) && (c <= 0x11FF)) ||//14）韩文字母：1100-11FF
        ((c >= 0x3130) && (c <= 0x318F)) ||//15）韩文兼容字母：3130-318F
        //((c>=0x1D300)&&(c<=0x1D35F))||//16）太玄经符号：1D300-1D35F
        ((c >= 0x4DC0) && (c <= 0x4DFF)) ||//17）易经六十四卦象：4DC0-4DFF
        ((c >= 0xA000) && (c <= 0xA48F)) ||//18）彝文音节：A000-A48F
        ((c >= 0xA490) && (c <= 0xA4CF)) ||//19）彝文部首：A490-A4CF
        ((c >= 0x2800) && (c <= 0x28FF)) ||//20）盲文符号：2800-28FF
        ((c >= 0x3200) && (c <= 0x32FF)) ||//21）CJK字母及月份：3200-32FF
        ((c >= 0x3300) && (c <= 0x33FF)) ||//22）CJK特殊符号（日期合并）：3300-33FF
        ((c >= 0x2700) && (c <= 0x27BF)) ||//23）装饰符号（非CJK专用）：2700-27BF
        ((c >= 0x2600) && (c <= 0x26FF)) ||//24）杂项符号（非CJK专用）：2600-26FF
        ((c >= 0xFE10) && (c <= 0xFE1F)) ||//25）中文竖排标点：FE10-FE1F
        ((c >= 0xFE30) && (c <= 0xFE4F)) ||//26）CJK兼容符号（竖排变体、下划线、顿号）：FE30-FE4F
        ((c >= 0x0000) && (c <= 0x002F)&& (c != 0x0020)) ||//
        ((c >= 0x003A) && (c <= 0x0040)) ||//
        ((c >= 0x005B) && (c <= 0x0060)) ||//
        ((c >= 0x007B) && (c <= 0x007F))
            ) {
        return 0;
    }

    if (((c >= 0x0041) && (c <= 0x005A))) {
        return c+0x20;
    }
    return c;
}
/**
 * 比较两个Ucs2String的大小
 * @param first
 * @param two
 * @return
 */
long ZLUnicodeUtil::compareTo(Ucs2String first,Ucs2String two){
    long fLen = first.size();
    long tLen = two.size();
    long len = fLen<tLen?fLen:tLen;
    for(int i=0;i<len;i++){
        long com = first[i]-two[i];
        if(com!=0){
            return com;
        }
    }
    return fLen - tLen;
}
/*
ZLUnicodeUtil::Ucs4Char ZLUnicodeUtil::toLower(Ucs4Char ch) {
	initUnicodeTable();
	std::map<ZLUnicodeUtil::Ucs4Char,ZLUnicodeData>::const_iterator it = UNICODE_TABLE.find(ch);
	return (it != UNICODE_TABLE.end()) ? it->second.LowerCase : ch;
}

void ZLUnicodeUtil::toLower(Ucs4String &str) {
	for (Ucs4String::iterator it = str.begin(); it != str.end(); ++it) {
		*it = toLower(*it);
	}
}
*/

std::string ZLUnicodeUtil::toLowerAscii(const std::string &utf8String) {
    const int size = utf8String.size();
    if (size == 0) {
        return utf8String;
    }

    std::string result(size, ' ');
    for (int i = size - 1; i >= 0; --i) {
        if ((utf8String[i] & 0x80) == 0) {
            result[i] = std::tolower(utf8String[i]);
        } else {
            result[i] = utf8String[i];
        }
    }
    return result;
}

/*
ZLUnicodeUtil::Ucs4Char ZLUnicodeUtil::toUpper(Ucs4Char ch) {
	initUnicodeTable();
	std::map<ZLUnicodeUtil::Ucs4Char,ZLUnicodeData>::const_iterator it = UNICODE_TABLE.find(ch);
	return (it != UNICODE_TABLE.end()) ? it->second.UpperCase : ch;
}

void ZLUnicodeUtil::toUpper(Ucs4String &str) {
	for (Ucs4String::iterator it = str.begin(); it != str.end(); ++it) {
		*it = toUpper(*it);
	}
}
*/

std::string ZLUnicodeUtil::toUpperAscii(const std::string &utf8String) {
    const int size = utf8String.size();
    if (size == 0) {
        return utf8String;
    }

    std::string result(size, ' ');
    for (int i = size - 1; i >= 0; --i) {
        if ((utf8String[i] & 0x80) == 0) {
            result[i] = std::toupper(utf8String[i]);
        } else {
            result[i] = utf8String[i];
        }
    }
    return result;
}

bool ZLUnicodeUtil::equalsIgnoreCaseAscii(const std::string &s0, const std::string &s1) {
    const size_t size = s0.size();
    if (s1.size() != size) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        if (s0[i] == s1[i]) {
            continue;
        }
        if ((s0[i] & 0x80) != 0 || (s1[i] & 0x80) != 0) {
            return false;
        }
        if (std::tolower(s0[i]) != std::tolower(s1[i])) {
            return false;
        }
    }
    return true;
}

bool ZLUnicodeUtil::equalsIgnoreCaseAscii(const std::string &s0, const char *s1) {
    const size_t size = s0.size();
    if (std::strlen(s1) != size) {
        return false;
    }
    for (size_t i = 0; i < size; ++i) {
        if (s0[i] == s1[i]) {
            continue;
        }
        if ((s0[i] & 0x80) != 0 || (s1[i] & 0x80) != 0) {
            return false;
        }
        if (std::tolower(s0[i]) != std::tolower(s1[i])) {
            return false;
        }
    }
    return true;
}

void ZLUnicodeUtil::utf8Trim(std::string &utf8String) {
    std::size_t counter = 0;
    std::size_t length = utf8String.length();
    Ucs4Char chr;
    while (counter < length) {
        const std::size_t l = firstChar(chr, utf8String.data() + counter);
        if (isSpace(chr)) {
            counter += l;
        } else {
            break;
        }
    }
    utf8String.erase(0, counter);
    length -= counter;

    std::size_t r_counter = length;
    while (r_counter > 0) {
        const std::size_t l = lastChar(chr, utf8String.data() + r_counter);
        if (isSpace(chr)) {
            r_counter -= l;
        } else {
            break;
        }
    }
    utf8String.erase(r_counter, length - r_counter);
}
