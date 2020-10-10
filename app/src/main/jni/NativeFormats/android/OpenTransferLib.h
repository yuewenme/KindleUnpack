//
// Created by yuewen on 19-8-8.
//gong
//

#ifndef L_OPENTRANSFERLIB_H
#define L_OPENTRANSFERLIB_H
#define ENCRYPT 0
#define DECRYPT 1

// 定义返回值
#define ERR_SUCCESS                     0   // 成功
#define ERR_PARAMETER_INVALID           1   // 输入参数错误
#define ERR_MEMORY_ALLOCATION           2   // 内存分配错误
#define ERR_FILE_RW                     3   // 文件读写错误
#define ERR_ENCRYPT_DECRYPT             4   // 加解密错误
#define ERR_VERSION_INVALID             5   // 版本错误
#define ERR_OTHER                      (-1) // 其他错误


#ifdef WIN32
#ifdef WIN_DLL
#ifdef _EBOOK_DLL
#define EBOOK_OPEN_API  __declspec(dllexport)
#else
#define EBOOK_OPEN_API  __declspec(dllimport)
#endif //_EBOOK_DLL
#else
#define EBOOK_OPEN_API
#endif //WIN_DLL
#else  //Linux
#define EBOOK_OPEN_API
#endif //WIN32

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************************************************************************/
//函数：  创建文件结构并初始化
//参数：  path         [in] 文件路径
//返回值：bool 成功 true，失败 false
/***************************************************************************************************************************/
EBOOK_OPEN_API void CreateFile(const char * path);

/***************************************************************************************************************************/
//函数：  打开文件 建立文件快捷索引 mobi或azw为建立块索引
//返回值：bool 成功 0，失败 -1
/***************************************************************************************************************************/
EBOOK_OPEN_API int Open();

/***************************************************************************************************************************/
//函数：  获取Css数据
//参数：   index     [in]  第几个css文件
//        dataLen      [out] 文件长度
//返回值：char * 文件内容
/***************************************************************************************************************************/
EBOOK_OPEN_API char *GetCssData(int position, int *dataLen, char * data );
/***************************************************************************************************************************/
//函数：  获取Css数据
//参数：   index     [in]  第几个css文件
//        dataLen      [out] 文件长度
//返回值：char * 文件内容
/***************************************************************************************************************************/
EBOOK_OPEN_API void GetCssDataLen(char * value,int * position,int * dataLen);

/***************************************************************************************************************************/
//函数：  获取Css文件总个数
//返回值：int 文件总个数
/***************************************************************************************************************************/
EBOOK_OPEN_API int GetCssDataNumber();

/***************************************************************************************************************************/
//函数：  释放内存
//返回值：无
/***************************************************************************************************************************/
EBOOK_OPEN_API void FreeCssData();

/***************************************************************************************************************************/
//函数：  获取Chapter数据
//参数：   index     [in]  第几个css文件
//        dataLen      [out] 文件长度
//返回值：char * 文件内容
/***************************************************************************************************************************/
EBOOK_OPEN_API char *GetChapterData(int index, int *dataLen);

/***************************************************************************************************************************/
//函数：  获取Chapter数据
//参数：   index     [in]  第几个css文件
//        dataLen      [out] 文件长度
//        data      [out] 文件内容
//返回值：
/***************************************************************************************************************************/
EBOOK_OPEN_API void GetChapterData2(int index, int position, int *dataLen, char *data);

/***************************************************************************************************************************/
//函数：  获取Chapter长度数据
//参数：   index     [in]  第几个css文件
//        dataLen      [out] 文件长度
//返回值：无
/***************************************************************************************************************************/
EBOOK_OPEN_API void GetChapterDataLen(int index, int *position, int *dataLen);

/***************************************************************************************************************************/
//函数：  获取章节文件总个数
//返回值：int 文件总个数
/***************************************************************************************************************************/
EBOOK_OPEN_API int GetChapterDataNumber();
/***************************************************************************************************************************/
//函数：  获取章节文件信息
//index：int 第几个文件
//chapterIndex：int 文件偏移
//parentIndex：int 父目录级别
//title：int 目录名称
/***************************************************************************************************************************/
EBOOK_OPEN_API void GetNcxInfo(int index,int *chapterIndex, int *parentIndex, char *pos_aid, char *title);
/***************************************************************************************************************************/
//函数：  获取章节文件总个数
//返回值：int 文件总个数
/***************************************************************************************************************************/
EBOOK_OPEN_API int GetNcxLen();

/***************************************************************************************************************************/
//函数：  释放内存
//返回值：无
/***************************************************************************************************************************/
EBOOK_OPEN_API void FreeChapterData();

/***************************************************************************************************************************/
//函数：  释放内存
//返回值：无
/***************************************************************************************************************************/
EBOOK_OPEN_API void FreeFile();
/***************************************************************************************************************************/
//函数：  获取图片
//参数：  name     [in]  键名 目前支持 recindex和src
//        value  [in]  键值
//        dataLen    [out] 图片大小
//返回值：图片数据
/***************************************************************************************************************************/
EBOOK_OPEN_API void GetImageDataLen(char * name, char * value,int * position,int * dataLen);

/***************************************************************************************************************************/
//函数：  获取链接数据
//参数：  value     [in]  键名 实际为文件定位数据
//        link    [out] 封装链接信息 章节名+id
//返回值：图片数据
/***************************************************************************************************************************/
EBOOK_OPEN_API void GetLinkData(char *value, char * link);
/***************************************************************************************************************************/
//函数：  获取书籍显示的分辨率
//参数：
//        h  [out]  高
//        w    [out] 宽
//返回值：图片数据
/***************************************************************************************************************************/
EBOOK_OPEN_API void GetOriginalResolution(float * w,float * h);
/***************************************************************************************************************************/
//函数：  获取图片
//参数：  name     [in]  键名 目前支持 recindex和src
//        value  [in]  键值
//        dataLen    [out] 图片大小
//返回值：图片数据
/***************************************************************************************************************************/
EBOOK_OPEN_API void GetImageData(int position, int *dataLen, char *data);
/***************************************************************************************************************************/
//函数：  释放加解密句柄
//参数：  cipher                [in]  加解密句柄
//返回值：int 成功 0，失败 -1
/***************************************************************************************************************************/
EBOOK_OPEN_API int test(PalmDocStream * palmDocStream);



/***************************************************************************************************************************/
//函数：  计算文件的MD5值
//参数：  pFilePath  [in]  输入的全路径文件名
//        ppOutput   [out] 输出的散列字符串指针，内存由内部分配，使用完毕后外部调用FreePtr(void* ptr)释放内存
//        nOutputLen [out] 输出的散列字符串长度
//返回值：int 成功 0，失败 非0
/***************************************************************************************************************************/
EBOOK_OPEN_API int MD5File( const char* pFilePath, char** ppOutput, int* nOutputLen );

#ifdef __cplusplus
}
#endif

#endif //L_OPENTRANSFERLIB_H
