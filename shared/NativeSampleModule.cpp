#include "NativeSampleModule.h"
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release
#include <android/log.h>  // 로그 출력을 위한 헤더
#include "./engine/Engine.h"

namespace facebook::react {
NativeSampleModule::NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker)
    : NativeSampleModuleCxxSpec(std::move(jsInvoker)) {};

void NativeSampleModule::initSurface(ANativeWindow *window) {
  Engine::instance().initSurface(window);
}

void NativeSampleModule::changeSurface(int width, int height) {
  Engine::instance().changeSurface(width, height);
}

void NativeSampleModule::destroySurface() {
  Engine::instance().destroySurface();
};

void NativeSampleModule::setImageSequence(jsi::Runtime &rt, const std::vector<std::string>& paths, double clipDurSec, double xfadeSec) {
  Engine::instance().setImageSequence(paths, clipDurSec, xfadeSec);
}

void NativeSampleModule::previewPlay(jsi::Runtime &rt) {
  Engine::instance().previewPlay();
};

void NativeSampleModule::previewPause(jsi::Runtime &rt) {
  Engine::instance().previewPause();
};

void NativeSampleModule::previewStop(jsi::Runtime &rt) {
  Engine::instance().previewStop();
};

double NativeSampleModule::getTimelineDuration(jsi::Runtime &rt) {
  return Engine::instance().getTimelineDuration();
};

void NativeSampleModule::startEncoding(jsi::Runtime &rt, int width, int height, int fps, int bitrate, const std::string& mime,const std::string& outputPath) {
  EncoderConfig config;
  config.width = width;
  config.height = height;
  config.fps = fps;
  config.bitrate = bitrate;
  config.mime = mime;
  config.outputPath = outputPath;

  Engine::instance().startEncoding(config);
}

void NativeSampleModule::cancelEncoding(jsi::Runtime &rt) {
  Engine::instance().cancelEncoding();
}

bool NativeSampleModule::isEncoding(jsi::Runtime &rt) {
  return Engine::instance().isEncoding();
}

std::string NativeSampleModule::getLastEncodedPath(jsi::Runtime &rt) {
  return Engine::instance().getLastEncodedPath();
}

double NativeSampleModule::getEncodingProgress(jsi::Runtime &rt) {
  return Engine::instance().getEncodingProgress();
}

} // namespace facebook::react

// JNI 함수 정의
extern "C" JNIEXPORT void JNICALL Java_com_sampleapp_SkiaView_nativeInitSurface(JNIEnv *env, jobject, jobject surface) {
  ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
  if (!window) {
    return;
  }
  Engine::instance().initSurface(window);
}

extern "C" JNIEXPORT void JNICALL Java_com_sampleapp_SkiaView_nativeChangeSurface(JNIEnv *env, jobject, jint width, jint height) {
  Engine::instance().changeSurface(width, height);
}

extern "C" JNIEXPORT void JNICALL Java_com_sampleapp_SkiaView_nativeDestroySurface(JNIEnv *, jobject) {
  Engine::instance().destroySurface();
}
