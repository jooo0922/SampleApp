#include "NativeSampleModule.h"
#include "drawables/RotatingRect.h"
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release
#include <android/log.h>  // 로그 출력을 위한 헤더

namespace facebook::react {

// TODO : 전역 변수를 사용하지 않아도 되도록 JSI 와 JNI 에서 동일한 TurboModule 인스턴스를 참조하도록 수정
std::shared_ptr<NativeSampleModule> gModule;
std::shared_ptr<Renderer> gRenderer;
std::shared_ptr<PreviewController> gPreviewController;
bool gRendererStarted = false;

NativeSampleModule::NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker)
    : NativeSampleModuleCxxSpec(std::move(jsInvoker)) {}

void NativeSampleModule::initSurface(ANativeWindow *window) {
  if (!window) return;
  if (gRendererStarted) return; // double init 방지

  // renderer nullptr check
  if (!gRenderer) {
    gRenderer = std::make_shared<Renderer>();
  }

  // preview controller nullptr check
  if (!gPreviewController) {
    gPreviewController = std::make_shared<PreviewController>(gRenderer);
  }

  /** drawable 객체 생성 및 추가 */
  // 회전 사각형 생성
  auto pRect = std::make_shared<RotatingRect>();
  pRect->setSize(100.0f, 100.0f);
  pRect->setSpeed(120.0f);
  pRect->setColor(SK_ColorRED);
  gRenderer->addDrawable(pRect);

  // renderer 초기화
  gRenderer->start(window);
  gRendererStarted = true;
}

void NativeSampleModule::changeSurface(int width, int height) {
  if (gRendererStarted && gRenderer) {
    gRenderer->resize(width, height);
  }
};

void NativeSampleModule::destroySurface() {
  if (!gRendererStarted) return; // 초기화도 안된 상태에서 메모리 해제 시도 방지
  if (gRenderer) {
    gRenderer->clearDrawables();
    gRenderer->stop();
  }
  gRendererStarted = false;
};

void NativeSampleModule::setImageSequence(jsi::Runtime &rt, const std::vector<std::string>& paths, double clipDurSec, double xfadeSec) {
    // renderer nullptr check
  if (!gRenderer) {
    gRenderer = std::make_shared<Renderer>();
  }

  // preview controller nullptr check
  if (!gPreviewController) {
    gPreviewController = std::make_shared<PreviewController>(gRenderer);
  }

  if (gPreviewController) {
    if (gPreviewController->setImageSequence(paths, clipDurSec, xfadeSec)) {
      m_lastTimelineDurationSec = gPreviewController->durationSec();
    }
  }
};

void NativeSampleModule::previewPlay(jsi::Runtime &rt) {
  if (gPreviewController) {
    gPreviewController->previewPlay();
  }
};

void NativeSampleModule::previewPause(jsi::Runtime &rt) {
  if (gPreviewController) {
    gPreviewController->previewPause();
  }
};

void NativeSampleModule::previewStop(jsi::Runtime &rt) {
  if (gPreviewController) {
    gPreviewController->previewStop();
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
