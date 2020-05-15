LOCAL_PATH := $(call my-dir)

ifndef GSTREAMER_ROOT_ANDROID
$(error GSTREAMER_ROOT_ANDROID is not defined!)
endif

ifndef NNSTREAMER_ROOT
$(error NNSTREAMER_ROOT is not defined!)
endif

ifeq ($(TARGET_ARCH_ABI),armeabi-v7a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/armv7
else ifeq ($(TARGET_ARCH_ABI),arm64-v8a)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/arm64
else ifeq ($(TARGET_ARCH_ABI),x86)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86
else ifeq ($(TARGET_ARCH_ABI),x86_64)
GSTREAMER_ROOT        := $(GSTREAMER_ROOT_ANDROID)/x86_64
else
$(error Target arch ABI not supported: $(TARGET_ARCH_ABI))
endif

#------------------------------------------------------
# API build option
#------------------------------------------------------
NNSTREAMER_API_OPTION := all

# tensorflow-lite (nnstreamer tf-lite subplugin)
ENABLE_TF_LITE := false

# SNAP (Samsung Neural Acceleration Platform)
ENABLE_SNAP := false

# NNFW (On-device neural network inference framework, Samsung Research)
ENABLE_NNFW := false

# SNPE (Snapdragon Neural Processing Engine)
ENABLE_SNPE := false

ifeq ($(ENABLE_SNAP),true)
  ifeq ($(ENABLE_SNPE),true)
   $(error DO NOT enable SNAP and SNPE both. The app would fail to use DSP or NPU runtime.)
  endif
endif

# Common libraries for sub-plugins
NNS_API_LIBS := nnstreamer gstreamer_android
NNS_API_FLAGS :=
NNS_SUBPLUGINS :=

ifeq ($(NNSTREAMER_API_OPTION),single)
NNS_API_FLAGS += -DNNS_SINGLE_ONLY=1
endif

include $(NNSTREAMER_ROOT)/jni/nnstreamer.mk
NNS_API_FLAGS += -DVERSION=\"$(NNSTREAMER_VERSION)\"

#------------------------------------------------------
# external libs and sub-plugins
#------------------------------------------------------
ifeq ($(ENABLE_TF_LITE),true)
NNS_API_FLAGS += -DENABLE_TENSORFLOW_LITE=1
# define types in tensorflow-lite sub-plugin. This assumes tensorflow-lite >= 1.13 (older versions don't have INT8/INT16)
NNS_API_FLAGS += -DTFLITE_INT8=1 -DTFLITE_INT16=1
NNS_SUBPLUGINS += tensorflow-lite-subplugin

include $(LOCAL_PATH)/Android-tensorflow-lite.mk
endif

ifeq ($(ENABLE_SNAP),true)
NNS_API_FLAGS += -DENABLE_SNAP=1
NNS_SUBPLUGINS += snap-subplugin

include $(LOCAL_PATH)/Android-snap.mk
endif

ifeq ($(ENABLE_NNFW),true)
NNS_API_FLAGS += -DENABLE_NNFW=1
NNS_SUBPLUGINS += nnfw-subplugin

include $(LOCAL_PATH)/Android-nnfw.mk
endif

ifeq ($(ENABLE_SNPE),true)
NNS_API_FLAGS += -DENABLE_SNPE=1
NNS_SUBPLUGINS += snpe-subplugin

include $(LOCAL_PATH)/Android-snpe.mk
endif

#------------------------------------------------------
# nnstreamer
#------------------------------------------------------
include $(LOCAL_PATH)/Android-nnstreamer.mk

# Remove any duplicates.
NNS_SUBPLUGINS := $(sort $(NNS_SUBPLUGINS))

#------------------------------------------------------
# native code for api
#------------------------------------------------------
include $(CLEAR_VARS)

LOCAL_MODULE := nnstreamer-native

LOCAL_SRC_FILES := \
    nnstreamer-native-api.c \
    nnstreamer-native-singleshot.c

ifneq ($(NNSTREAMER_API_OPTION),single)
LOCAL_SRC_FILES += \
    nnstreamer-native-customfilter.c \
    nnstreamer-native-pipeline.c
endif

LOCAL_CFLAGS := -O2 -fPIC
LOCAL_SHARED_LIBRARIES := $(NNS_API_LIBS) $(NNS_SUBPLUGINS)

include $(BUILD_SHARED_LIBRARY)

#------------------------------------------------------
# gstreamer for android
#------------------------------------------------------
GSTREAMER_NDK_BUILD_PATH := $(GSTREAMER_ROOT)/share/gst-android/ndk-build/
include $(LOCAL_PATH)/Android-gst-plugins.mk

GSTREAMER_PLUGINS        := $(GST_REQUIRED_PLUGINS)
GSTREAMER_EXTRA_DEPS     := $(GST_REQUIRED_DEPS) gio-2.0 gmodule-2.0 json-glib-1.0
GSTREAMER_EXTRA_LIBS     := $(GST_REQUIRED_LIBS) -liconv

GSTREAMER_INCLUDE_FONTS := no
GSTREAMER_INCLUDE_CA_CERTIFICATES := no

include $(GSTREAMER_NDK_BUILD_PATH)/gstreamer-1.0.mk

#------------------------------------------------------
# NDK cpu-features
#------------------------------------------------------
$(call import-module, android/cpufeatures)
