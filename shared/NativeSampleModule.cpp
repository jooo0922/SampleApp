#include "NativeSampleModule.h"
#include "drawables/RotatingRect.h"
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release
#include <GLES2/gl2ext.h>

namespace facebook::react {

std::shared_ptr<NativeSampleModule> gModule;

NativeSampleModule::NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker)
    : NativeSampleModuleCxxSpec(std::move(jsInvoker)) {}

std::string NativeSampleModule::reverseString(jsi::Runtime &rt,
                                              std::string input) {
  return std::string(input.rbegin(), input.rend());
}

void NativeSampleModule::startDecoding(jsi::Runtime &rt, const std::string& filePath) {

};

void NativeSampleModule::stopDecoding(jsi::Runtime &rt) {

};

void NativeSampleModule::initSurface(ANativeWindow *window) {
  if (!window) return;
  if (m_bStarted) return; // double init 방지

  // renderer nullptr check
  if (!m_pRenderer) {
    m_pRenderer = std::make_shared<Renderer>();
  }

  /** drawable 객체 생성 및 추가 */
  // 회전 사각형 생성
  auto pRect = std::make_shared<RotatingRect>();
  pRect->setSize(100.0f, 100.0f);
  pRect->setSpeed(120.0f);
  pRect->setColor(SK_ColorRED);
  m_pRenderer->addDrawable(pRect);

  // renderer 초기화
  m_pRenderer->start(window);
  m_bStarted = true;
}

void NativeSampleModule::changeSurface(int width, int height) {
  if (m_bStarted && m_pRenderer) {
    m_pRenderer->resize(width, height);
  }
};

void NativeSampleModule::destroySurface() {
  if (!m_bStarted) return; // 초기화도 안된 상태에서 메모리 해제 시도 방지
  if (m_pRenderer) {
    m_pRenderer->clearDrawables();
    m_pRenderer->stop();
  }
  m_bStarted = false;
};

GLuint NativeSampleModule::createOESTexture() {
  m_pRenderer->assertEGLContextCurrent();
  if (m_oesTex == 0) {
    glGenTextures(1, &m_oesTex);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_oesTex);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  return m_oesTex;
};

} // namespace facebook::react

/** JNI 함수 정의 */
// ====== SurfaceView.kt JNI 구현 ======
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

// ====== VideoSurfaceHelper.kt JNI 구현 ======
extern "C" JNIEXPORT jint JNICALL Java_com_sampleapp_video_VideoSurfaceHelper_nativeCreateOESTexture(JNIEnv*, jclass) {
  if (!facebook::react::gModule) {
    facebook::react::gModule = std::make_shared<facebook::react::NativeSampleModule>(nullptr);
  }
  return static_cast<jint>(facebook::react::gModule->createOESTexture());
};

extern "C" JNIEXPORT void JNICALL Java_com_sampleapp_video_VideoSurfaceHelper_nativeOnVideoFrameAvailable(JNIEnv*, jclass) {
  // TODO : IVideoPlayer::notifyFrame() 호출 -> SurfaceTexture 에 디코딩된 프레임 데이터가 업데이트 되었다는 신호를 렌더링 스레드에 전달
};
