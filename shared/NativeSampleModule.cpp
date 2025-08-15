#include "NativeSampleModule.h"
#include "drawables/RotatingRect.h"
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release

namespace facebook::react {

std::shared_ptr<NativeSampleModule> gModule;

NativeSampleModule::NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker)
    : NativeSampleModuleCxxSpec(std::move(jsInvoker)) {}

std::string NativeSampleModule::reverseString(jsi::Runtime &rt,
                                              std::string input) {
  return std::string(input.rbegin(), input.rend());
}

void NativeSampleModule::initSurface(ANativeWindow *window) {
  // renderer nullptr check
  if (!m_pRenderer) {
    m_pRenderer = std::make_shared<Renderer>();
  }

  /** drawable 객체 생성 및 추가 */
  // 회전 사각형 생성
  auto rect = std::make_shared<RotatingRect>();
  rect->setSize(100.0f, 100.0f);
  rect->setSpeed(120.0f);
  rect->setColor(SK_ColorRED);
  m_pRenderer->addDrawable(rect);

  // renderer 초기화
  m_pRenderer->start(window);
}

void NativeSampleModule::changeSurface(int width, int height) {
  if (m_pRenderer) {
    m_pRenderer->resize(width, height);
  }
};

void NativeSampleModule::destroySurface() {
  if (m_pRenderer) {
    m_pRenderer->clearDrawables();
    m_pRenderer->stop();
  }
};

} // namespace facebook::react

// JNI 함수 정의
extern "C" JNIEXPORT void JNICALL Java_com_sampleapp_SkiaView_nativeInitSurface(JNIEnv *env, jobject, jobject surface) {
  ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
  if (!facebook::react::gModule) {
    facebook::react::gModule = std::make_shared<facebook::react::NativeSampleModule>(nullptr);
  }
  facebook::react::gModule->initSurface(window);
}

extern "C" JNIEXPORT void JNICALL Java_com_sampleapp_SkiaView_nativeChangeSurface(JNIEnv *env, jobject, jint width, jint height) {
  if (!facebook::react::gModule) {
    facebook::react::gModule = std::make_shared<facebook::react::NativeSampleModule>(nullptr);
  }
  facebook::react::gModule->changeSurface(width, height);
}

extern "C" JNIEXPORT void JNICALL Java_com_sampleapp_SkiaView_nativeDestroySurface(JNIEnv *, jobject) {
  if (facebook::react::gModule) {
    facebook::react::gModule->destroySurface();
  }
}
