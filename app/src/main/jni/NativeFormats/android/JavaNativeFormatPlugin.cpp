/*
 * Copyright (C) 2011-2015 FBReader.com Limited <contact@fbreader.com>
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

extern "C"
JNIEXPORT jstring JNICALL
Java_com_ebook_reader_formats_NativeFormatPlugin_getDictionaryContentNative(
        JNIEnv *env, jobject thiz, jstring path, jstring jword) {
    const std::string bookPath = AndroidUtil::fromJavaString(env, path);
    const std::string word = AndroidUtil::fromJavaString(env, jword);
    shared_ptr<PalmDocContentStream> stream = new PalmDocContentStream(ZLFile(bookPath).inputStream(),
                                                                       OPEN_DICT_SEARCH_INDEX,
                                                                       word);
    bool b = stream->open();
    std::string string;
    if (b) {
        stream->openType = OPEN_DICT_SEARCH;
        int num = 0;
        for (int i = 0; i < stream->indexTable.size(); i++) {
            PalmDocStream::IndexTable indexTable1 = stream->indexTable[i];
            if (word.size() == indexTable1.fileName.size() &&
                word.compare(indexTable1.fileName.c_str()) == 0 ||
                (string.size() == stream->indexTable.size() - 1 && i == 0)) {
                stream->fastSeek(indexTable1.startPosition);
                char read[indexTable1.length];
                stream->read(read, indexTable1.length);
                std::string text;
                /*if(ZLUnicodeUtil::is_utf8_string(read)){*/
                std::string text2(read, indexTable1.length);
                text = text2;
                /*} else{
                    std::string text2;
                    ZLUnicodeUtil::Ucs2String ucs2String;
                    ZLUnicodeUtil::utf8ToUcs2(ucs2String,read,indexTable1.length);
                    ZLUnicodeUtil::ucs2ToUtf8(text,ucs2String);
                }*/
                //当首标签最大值小于内容长度-单词长度时-</h2>长度时判断<h2>标签包涵的是索引内容
                if (text.find("<h2>") == 0 || text.find("<h2 >") == 0) {
                    int index = text.find("</h2>");
                    if (index > 0 && index < text.size() - 5 - word.size()) {
                        text = text.substr(index + 5, indexTable1.length - index - 5);
                        while (text.find(" ") == 0) {
                            text = text.substr(1, indexTable1.length - 1);
                        }
                    }
                }
                ZLUnicodeUtil::changeUtf8String(text);
                if (num != 0)
                    string.append("<br/>");
                string.append(text.c_str());
                if (num++ == 10)//防止查询出现过多词义导致查询过慢的问题,现一个词最多查10个词义
                    break;
            }
        }

    }
    stream->close();
    return AndroidUtil::createJavaString(env, string);
}
extern "C"
JNIEXPORT jboolean JNICALL Java_com_ebook_reader_formats_NativeFormatPlugin_initDictionaryNative(
        JNIEnv *env, jobject thiz, jstring path) {
    return false;
}
extern "C"
JNIEXPORT jobjectArray JNICALL
Java_com_ebook_reader_formats_NativeFormatPlugin_getDictionaryContentListNative(
        JNIEnv *env, jobject thiz, jstring jword) {
    return 0;
}

extern "C"
JNIEXPORT jboolean JNICALL Java_com_ebook_reader_formats_NativeFormatPlugin_isDictionaryNative(
        JNIEnv *env, jobject thiz, jstring path) {
    const std::string bookPath = AndroidUtil::fromJavaString(env, path);
    shared_ptr<PalmDocContentStream> stream = new PalmDocContentStream(ZLFile(bookPath).inputStream(),
                                                                       OPEN_DICT_IS_DICT);
    bool b = stream->open();
    stream->close();
    return b;
}
extern "C"
JNIEXPORT jbooleanArray JNICALL
Java_com_ebook_reader_formats_NativeFormatPlugin_isDictionaryListNative(
        JNIEnv *env, jobject thiz, jstring path) {
    ZLFSManager::Instance().init();
    const std::string paths = AndroidUtil::fromJavaString(env, path);

    std::vector<std::string> bookPaths;
    std::vector<jboolean> bools;
    char *cstr, *p;
    //cstr = new char[sizeof(dicts)+1];

    int len = paths.size();
    cstr = new char[len];
    memset(cstr, 0, len);
    strcpy(cstr, paths.c_str());
    //LOGI("===================114%s",cstr);
    p = strtok(cstr, "///");
    while (p != NULL) {
        bookPaths.push_back(p);
        p = strtok(NULL, "///");
    }
    for (int i = 0; i < bookPaths.size(); i++) {
        shared_ptr<PalmDocContentStream> stream = new PalmDocContentStream(ZLFile(bookPaths[i]).inputStream(),
                                                                           OPEN_DICT_IS_DICT);
        bools.push_back(stream->open());
        stream->close();
    }
    return AndroidUtil::createJavaByteArray(env, bools);
}

