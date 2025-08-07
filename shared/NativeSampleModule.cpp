#include "NativeSampleModule.h"
#include <core/SkCanvas.h>
#include <core/SkPaint.h>
#include <core/SkRect.h>
#include <core/SkSurface.h>
#include <core/SkColorSpace.h>
#include <gpu/ganesh/SkSurfaceGanesh.h>
#include <gpu/ganesh/GrDirectContext.h>
#include <gpu/ganesh/GrBackendSurface.h>
#include <gpu/ganesh/gl/GrGLBackendSurface.h>
#include <gpu/ganesh/gl/GrGLDirectContext.h>
#include <gpu/ganesh/gl/GrGLInterface.h>
#include <android/native_window_jni.h> // ANativeWindow_fromSurface, ANativeWindow_release
#include <android/log.h>  // 로그 출력을 위한 헤더
#include <memory>
#include <thread>
#include <EGL/egl.h>
#include <GLES3/gl3.h> // OpenGL ES 3.0 API 사용

namespace facebook::react {

// egl 설정 관련 전역 변수
EGLDisplay eglDisplay = EGL_NO_DISPLAY;
EGLContext eglContext = EGL_NO_CONTEXT;
EGLSurface eglSurface = EGL_NO_SURFACE;

// ganesh gpu 백엔드 관련 전역 객체 (skia 버전 스마트 포인터(std::shared_ptr 과 유사)로 관리)
sk_sp<GrDirectContext> pGrContext;
sk_sp<SkSurface> pSkiaSurface;

ANativeWindow *nativeWindow = nullptr; // Android Surface를 저장할 전역 변수
std::atomic<bool> isRendering = false; // 렌더링 루프 상태
std::thread renderThread; // 렌더링 루프를 실행할 스레드
SkImageInfo imageInfo = SkImageInfo::MakeN32Premul(0, 0); // skia 내부에서 렌더링할 이미지(framebuffer)의 크기, color type 등 메타데이터를 담는 객체

std::shared_ptr<NativeSampleModule> gModule;

NativeSampleModule::NativeSampleModule(std::shared_ptr<CallInvoker> jsInvoker)
    : NativeSampleModuleCxxSpec(std::move(jsInvoker)) {}

std::string NativeSampleModule::reverseString(jsi::Runtime &rt,
                                              std::string input) {
  return std::string(input.rbegin(), input.rend());
}

void NativeSampleModule::initSurface(ANativeWindow *window) {
  nativeWindow = window;

  if (nativeWindow) {
    // 렌더링 루프 시작
    isRendering = true;
    // 렌더링 루프는 별도의 렌더링 스레드 생성하여 돌린다. (ui 스레드와 별개로 돌아가도록 하여 오버헤드를 줄이려는 목적)
    renderThread = std::thread([this]() { renderLoop(); });
  };
}

void NativeSampleModule::changeSurface(int width, int height) {
  // skia 내부 framebuffer 에 적용할 이미지 크기(메타데이터) 업데이트
  imageInfo = SkImageInfo::MakeN32Premul(width, height);
  if (nativeWindow) {
    // android surface 설정(해상도, pixel format)
    ANativeWindow_setBuffersGeometry(nativeWindow, width, height, WINDOW_FORMAT_RGBA_8888);

    // android surface 크기 변경 시 렌더링 루프에서 감지하여 SkSurface 재생성하도록 nullptr 초기화
    pSkiaSurface = nullptr;
  }
};

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
  // 렌더링 루프 진입 직전 EGL 초기화 수행
  if (!initEGL()) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "EGL initialization failed");
    return;
  }

  // initEGL() 함수에서 생성된 EGLContext 를 렌더링 스레드에 바인딩하여 skia ganesh 백엔드가 사용할 수 있도록 함.
  if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to bind EGL context in render thread");
    return;
  }

  // ganesh gpu 백엔드 기반 SkSurface 생성
  if (!setupSkiaSurface()) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to setup Skia surface");
    return;
  }

  SkCanvas* canvas = pSkiaSurface->getCanvas();
  float rotation = 0.0f; // 회전 각도
  int width = imageInfo.dimensions().width();
  int height = imageInfo.dimensions().height();

  while (isRendering)
  {
    // 렌더링 루프 시작 시 매 프레임마다 SkSurface 재생성이 필요한지 체크
    if (!pSkiaSurface) {
      // SkSurface 가 nullptr 초기화되어 있다면 재생성
      if (!setupSkiaSurface()) {
        __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to recreate Skia surface in rendering loop");
        continue;  // 혹은 break; 로 종료
      }

      // SkSurface가 재생성되었으면 canvas도 업데이트
      canvas = pSkiaSurface->getCanvas();
    }

    // 캔버스 초기화
    canvas->clear(SK_ColorLTGRAY);

    // 변환 적용 전 캔버스 상태 저장
    canvas->save();

    // 캔버스 상태 변환
    float_t cx = static_cast<float_t>(width * 0.5f);
    float_t cy = static_cast<float_t>(height * 0.5f);
    canvas->translate(cx, cy);
    canvas->rotate(rotation);

    // 출력 색상 및 antialiasing 상태 설정
    SkPaint paint;
    paint.setColor(SK_ColorBLUE);
    paint.setAntiAlias(true);

    // 100*100 사각형 draw call
    float_t rect_width = 100.0f;
    float_t rect_height = 100.0f;
    float_t rect_offset_x = -(rect_width * 0.5f);
    float_t rect_offset_y = -(rect_height * 0.5f);
    SkRect rect = SkRect::MakeXYWH(rect_offset_x, rect_offset_y, rect_width, rect_height);
    canvas->drawRect(rect, paint);

    // 변환 적용 전 상태로 캔버스 복원
    canvas->restore();

    // 회전 각도 업데이트 -> [0.0f, 360.0f] 각도 구간 반복
    rotation += 1.0f;
    if (rotation >= 360.0f) {
      rotation = 0.0f;
    }

    // __android_log_print(ANDROID_LOG_DEBUG, "NativeSampleModule", "Frame end");

    // Android Surface에 렌더링 결과 출력 (cpu 렌더링 시 구현 필요)
    // if (nativeWindow) {
    //   ANativeWindow_Buffer buffer;
    //   // android surface buffer 에 렌더링 결과를 복사하는 동안 다른 스레드가 접근하지 못하도록 잠금 (mutex)
    //   if (ANativeWindow_lock(nativeWindow, &buffer, nullptr) == 0) {
    //     // skia surface -> android surface buffer 에 렌더링 결과 복사
    //     pSkiaSurface->readPixels(imageInfo, buffer.bits, buffer.stride * 4, 0, 0);
    //     // bool bcopied = pSkiaSurface->readPixels(imageInfo, buffer.bits, buffer.stride * 4, 0, 0);
    //     // __android_log_print(ANDROID_LOG_DEBUG, "NativeSampleModule", "is copied? %d", (int)bcopied);
    //     // android surface buffer lock 해제
    //     ANativeWindow_unlockAndPost(nativeWindow);
    //   }
    // }

    // Skia 내부 command queue 에 쌓인 현재 프레임까지 요청된 모든 draw operation 들을 GPU 로 전송하여 실행 요청
    pGrContext->flush();

    // 디스플레이 vsync 시점에 맞춰 back buffer 와 front buffer 를 교체하여 화면 업데이트
    eglSwapBuffers(eglDisplay, eglSurface);

    // 16ms 대기 (약 60FPS)
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
  }

  // 렌더링 루프 종료 후 egl 자원 해제
  destroyEGL();
};

bool NativeSampleModule::initEGL() {
  // EGLDisplay 생성
  eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (eglDisplay == EGL_NO_DISPLAY) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Unable to get EGLDisplay");
    return false;
  }

  // egl 초기화
  if (!eglInitialize(eglDisplay, nullptr, nullptr)) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to initialize EGL");
    return false;
  }

  // EGLConfig 설정
  EGLint configAttribs[] = {
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_RED_SIZE,   8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE,  8,
    EGL_ALPHA_SIZE, 8,
    EGL_NONE
  };
  EGLConfig eglConfig;
  EGLint numConfigs;
  if (!eglChooseConfig(eglDisplay, configAttribs, &eglConfig, 1, &numConfigs) || numConfigs < 1) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to choose EGL config");
    return false;
  }

  // EGLContext 생성
  EGLint contextAttribs[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  eglContext = eglCreateContext(eglDisplay, eglConfig, EGL_NO_CONTEXT, contextAttribs);
  if (eglContext == EGL_NO_CONTEXT) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to create EGL context");
    return false;
  }

  // 생성된 EGLContext 기반으로 EGLSurface 생성 -> android native window 와 연결
  eglSurface = eglCreateWindowSurface(eglDisplay, eglConfig, nativeWindow, nullptr);
  if (eglSurface == EGL_NO_SURFACE) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to create EGL surface");
    return false;
  }

  // 생성된 EGLContext 를 현재 스레드에 바인딩하여 skia ganesh 백엔드가 사용할 수 있도록 함.
  if (!eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext)) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to make EGL context current");
    return false;
  }

  return true;
};

void NativeSampleModule::destroyEGL() {
  if (eglDisplay != EGL_NO_DISPLAY) {
    eglMakeCurrent(eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    // EGLContext 메모리 해제
    if (eglContext != EGL_NO_CONTEXT) {
      eglDestroyContext(eglDisplay, eglContext);
      eglContext = EGL_NO_CONTEXT;
    }

    // EGLSurface 메모리 해제
    if(eglSurface != EGL_NO_SURFACE) {
      eglDestroySurface(eglDisplay, eglSurface);
      eglSurface = EGL_NO_SURFACE;
    }

    // EGLDisplay 메모리 해제
    eglTerminate(eglDisplay);
    eglDisplay = EGL_NO_DISPLAY;
  }

  // ganesh gpu 백엔드 관련 전역 객체 자원 해제
  pGrContext = nullptr;
  pSkiaSurface = nullptr;
};

/**
 * GrDirectContext 및 SkSurface 생성 함수.
 * -> imageInfo (즉, render target 크기)가 유효할 때 한번 생성하거나, 크기가 변경되었으면 재생성함.
 */
bool NativeSampleModule::setupSkiaSurface() {
  // 이미지 크기(메타데이터)가 유효할 경우에만 SkSurface 생성
  int width = imageInfo.dimensions().width();
  int height = imageInfo.dimensions().height();
  if (width <= 0 || height <= 0) {
    __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Invalid image size");
    return false;
  }

  // GrDirectContext 생성 (내부적으로 현재 스레드에 바인딩된 egl context 사용)
  if (!pGrContext) {
    auto interface = GrGLMakeNativeInterface();
    pGrContext = GrDirectContexts::MakeGL(interface);
    if (!pGrContext) {
      __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to create GrDirectContext");
      return false;
    }
  }

  // 기존 SkSurface 크기가 현재 이미지 크기(메타데이터)와 맞지 않을 경우 재생성하도록 nullptr 초기화
  if (pSkiaSurface) {
    SkImageInfo currentInfo = pSkiaSurface->imageInfo();
    if (currentInfo.width() != width || currentInfo.height() != height) {
      pSkiaSurface = nullptr;
    }
  }

  // SkSurface (재)생성
  if (!pSkiaSurface) {
    // 1. OpenGL ES 컨텍스트에 바인딩된 default framebuffer(fbo id == 0) 설정을 GrGLFramebufferInfo 로 구성.
    GrGLFramebufferInfo fboInfo;
    fboInfo.fFBOID = (GrGLuint)0;
    fboInfo.fFormat = GL_RGBA8;

    // 2. OpenGL ES 컨텍스트에 바인딩된 default framebuffer 를 감싸는 skia 의 render target (interface) 생성.
    GrBackendRenderTarget backendRT = GrBackendRenderTargets::MakeGL(
      width,         // render target width
      height,        // render target height
      0,             // sampleCount (멀티샘플링 안 함)
      0,             // 스텐실 비트 수 (없다면 0)
      fboInfo        // OpenGL 기본 FBO 정보
    );

    // 3. WrapBackendRenderTarget 을 사용하여 default framebuffer 기반 SkSurface 를 생성.
    pSkiaSurface = SkSurfaces::WrapBackendRenderTarget(
      pGrContext.get(),                         // 이미 생성된 GrDirectContext
      backendRT,
      kBottomLeft_GrSurfaceOrigin,              // OpenGL은 좌하단 기준
      SkColorType::kRGBA_8888_SkColorType,      // SkImageInfo의 color type (예, kN32_SkColorType)
      nullptr,                                  // 컬러 공간 (필요에 따라 지정)
      nullptr                                   // SkSurfaceProps (필요에 따라)
    );

    if (!pSkiaSurface) {
      __android_log_print(ANDROID_LOG_ERROR, "NativeSampleModule", "Failed to create SkSurface from backend render target");
      return false;
    }
  }

  return true;
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
