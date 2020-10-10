LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE                  := OpenTransfer
LOCAL_CFLAGS                  := -Wall #-Wl,-Map=test.map -g  
LOCAL_LDLIBS                  := -lz -llog
# LOCAL_CPP_FEATURES += exceptions
LOCAL_LDFLAGS += -fPIC
LOCAL_CFLAGS += \
	-DMOBI_LIB=1 \

LOCAL_SRC_FILES               := \
			NativeFormats/android/OpenTransferLib.cpp \
			NativeFormats/common/zlibrary/core/unix/filesystem/ZLUnixFileInputStream.cpp \
			NativeFormats/common/zlibrary/core/logger/ZLLogger.h \
			NativeFormats/common/zlibrary/core/util/ZLUnicodeUtil.cpp \
			NativeFormats/common/zlibrary/core/util/ZLLanguageUtil.cpp \
			NativeFormats/common/zlibrary/core/util/ZLStringUtil.cpp \
			NativeFormats/common/fbreader/formats/pdb/BitReader.cpp \
			NativeFormats/common/fbreader/formats/pdb/DocDecompressor.cpp \
			NativeFormats/common/fbreader/formats/pdb/PalmDocStream.cpp \
			NativeFormats/common/fbreader/formats/pdb/PdbStream.cpp \
			NativeFormats/common/fbreader/formats/pdb/PalmDocLikeStream.cpp \
			NativeFormats/common/fbreader/formats/pdb/PdbReader.cpp \
			NativeFormats/common/fbreader/formats/css/CSSInputStream.cpp \
			NativeFormats/common/fbreader/formats/pdb/HuffDecompressor.cpp \

LOCAL_C_INCLUDES              := \
	$(LOCAL_PATH)/NativeFormats/android \
	$(LOCAL_PATH)/NativeFormats/common/zlibrary/core/logger \
	$(LOCAL_PATH)/NativeFormats/common/zlibrary/core/util \
	$(LOCAL_PATH)/NativeFormats/common/zlibrary/core/unix/filesystem \

include $(BUILD_SHARED_LIBRARY)

