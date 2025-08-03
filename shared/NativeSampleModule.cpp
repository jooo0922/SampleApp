#include "NativeSampleModule.h"
#include <core/SkCanvas.h>
#include <core/SkPaint.h>
#include <core/SkRect.h>
#include <core/SkSurface.h>
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release
#include <android/log.h>  // 로그 출력을 위한 헤더
#include <chrono>
#include <thread>

namespace facebook::react {

ANativeWindow *nativeWindow = nullptr; // Android Surface를 저장할 전역 변수
std::atomic<bool> isRendering = false; // 렌더링 루프 상태
std::thread renderThread; // 렌더링 루프를 실행할 스레드

NativeSampleModule::NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker)
    : NativeSampleModuleCxxSpec(std::move(jsInvoker)) {}

std::string NativeSampleModule::reverseString(jsi::Runtime &rt,
                                              std::string input) {
  return std::string(input.rbegin(), input.rend());
}

void NativeSampleModule::initSurface(ANativeWindow *window) {
  nativeWindow = window;

  if (nativeWindow) {
    // skia 내부 framebuffer 구성 정보(해상도, pixel format)에 적용하기 위해 지정
    ANativeWindow_setBuffersGeometry(nativeWindow, 800, 600, WINDOW_FORMAT_RGBA_8888);

    // 렌더링 루프 시작
    isRendering = true;
    // 렌더링 루프는 별도의 렌더링 스레드 생성하여 돌린다. (ui 스레드와 별개로 돌아가도록 하여 오버헤드를 줄이려는 목적)
    renderThread = std::thread([this]() { renderLoop(); });
  };
}

void NativeSampleModule::destroySurface() {
  if (renderThread.joinable()) {
    renderThread.join();
  }

  if (nativeWindow) {
    ANativeWindow_release(nativeWindow);
    nativeWindow = nullptr;
  }
};

void NativeSampleModule::renderLoop() {
  // 이미지 정보 생성
  SkImageInfo imageInfo = SkImageInfo::MakeN32Premul(800, 600);

  // TODO : SkSurfaces::Raster() 는 cpu 기반이라 Graphics Backend 를 사용하지 않는다고 함. gpu 렌더링 기반으로 예제 수정 필요.
  // TODO : third_party/skia/android/[architecture]/gen/... 디렉토리 제거하고 정상 빌드되는지 테스트
  auto skiaSurface = SkSurfaces::Raster(imageInfo);
  if (!skiaSurface) {
    return;
  }

  SkCanvas* canvas = skiaSurface->getCanvas();
  float rotation = 0.0f; // 회전 각도

  while (isRendering)
  {
    // 캔버스 초기화
    canvas->clear(SK_ColorWHITE);

    // 캔버스 중항으로 이동 및 회전 적용
    canvas->save();
    canvas->translate(400.0f, 300.0f);
    canvas->rotate(rotation);

    // 출력 색상 및 antialiasing 상태 설정
    SkPaint paint;
    paint.setColor(SK_ColorBLUE);
    paint.setAntiAlias(true);

    // 100*100 사각형 draw call
    SkRect rect = SkRect::MakeXYWH(-50.0f, -50.0f, 100.0f, 100.0f);
    canvas->drawRect(rect, paint);

    // canvas 상태 원복
    canvas->restore();

    // 회전 각도 업데이트 -> [0.0f, 360.0f] 각도 구간 반복
    rotation += 1.0f;
    if (rotation >= 360.0f) {
      rotation = 0.0f;
    }

    // __android_log_print(ANDROID_LOG_DEBUG, "NativeSampleModule", "Frame end");

    // Android Surface에 렌더링 결과 출력
    if (nativeWindow) {
      ANativeWindow_Buffer buffer;
      // android surface buffer 에 렌더링 결과를 복사하는 동안 다른 스레드가 접근하지 못하도록 잠금 (mutex)
      if (ANativeWindow_lock(nativeWindow, &buffer, nullptr) == 0) {
        // skia surface -> android surface buffer 에 렌더링 결과 복사
        skiaSurface->readPixels(imageInfo, buffer.bits, buffer.stride * 4, 0, 0);
        // bool bcopied = skiaSurface->readPixels(imageInfo, buffer.bits, buffer.stride * 4, 0, 0);
        // __android_log_print(ANDROID_LOG_DEBUG, "NativeSampleModule", "is copied? %d", (int)bcopied);
        // android surface buffer lock 해제
        ANativeWindow_unlockAndPost(nativeWindow);
      }
    }
  }

  // 16ms 대기 (약 60FPS)
  std::this_thread::sleep_for(std::chrono::milliseconds(16));
};

} // namespace facebook::react

// JNI 함수 정의
extern "C" JNIEXPORT void JNICALL Java_com_sampleapp_SkiaView_nativeInitSurface(
    JNIEnv *env, jobject, jobject surface) {
  ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
  auto module = std::make_shared<facebook::react::NativeSampleModule>(nullptr);
  module->initSurface(window);
}

extern "C" JNIEXPORT void JNICALL
Java_com_sampleapp_SkiaView_nativeDestroySurface(JNIEnv *, jobject) {
  auto module = std::make_shared<facebook::react::NativeSampleModule>(nullptr);
  module->destroySurface();
}