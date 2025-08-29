#include "VideoPlayerAndroid.h"

#ifdef __ANDROID__

#include <gpu/ganesh/GrBackendSurface.h>
#include <gpu/ganesh/SkImageGanesh.h>
#include <gpu/ganesh/gl/GrGLBackendSurface.h>
#include <gpu/ganesh/gl/GrGLTypes.h>
#include <android/log.h>
#include <EGL/egl.h>

// 외부에서 보관하는 JavaVM 포인터 (android/app/src/main/jni/OnLoad.cpp 에서 g_vm = vm; 으로 포인터를 전역 보관해놓기)
/**
 * 자바 세계로 접근하기 위한 JVM(자바 가상머신) "전역" 핸들
 * 프로세스 전역에 1개만 존재하기 때문에 전역 변수로 보관해도 안전함.
 *
 * JNIEnv 는 특정 스레드(ex> 현재 실행 중인 네이티브 c++ 스레드)에서 자바 세계로 들어가서
 * Java/Kotlin 코드에 구현된 android 관련 클래스나 메서드에 접근 및 호출하기 위해 사용하는 "스레드 전용" 핸들.
 * c++ 네이티브 스레드가 자바에게 말을 걸고 싶을 때 필요한 일종의 열쇠 또는 통역사 개념.
 * 스레드마다 다르기 때문에 캐시 금지.
 *
 * 네이티브 스레드라면 JavaVM->AttachCurrentThread() 로 붙여서 얻음.
 * 끝나면 가능하면 DetachCurrentThread로 정리.
 */
extern JavaVM* g_vm;

// OES 텍스처를 화면 사각형으로 샘플링하는 매우 단순한 셰이더
// Vertex Shader
static const char* kVS = R"(
attribute vec4 aPos;
attribute vec2 aUv;
uniform mat4 uTexMatrix;

varying vec2 vUv;

void main() {
  gl_Position = aPos;
  vec4 t = uTexMatrix * vec4(aUv, 0.0, 1.0);
  vUv = t.xy;
}
)";

// Fragment Shader
static const char* kFS = R"(
#extension GL_OES_EGL_image_external : require
precision mediump float;

varying vec2 vUv;
uniform samplerExternalOES uTex;

void main() {
  gl_FragColor = texture2D(uTex, vUv);
}
)";

// 생성자: SurfaceTexture를 GlobalRef로 승격하고 각 멤버 초기화
VideoPlayerAndroid::VideoPlayerAndroid(GrDirectContext* gr, JNIEnv* env, jobject surfaceTextureGlobal, GLuint oesTex)
  : m_gr(gr), m_oesTex(oesTex) {
  /**
   * JNI Reference 종류와 사용 의도
   *
   * - LocalRef
   *   • JNI 호출 단위에서만 유효 (함수 리턴 시 자동 해제됨)
   *   • 객체 자체는 자바 쪽 참조가 있으면 GC 대상이 아님
   *
   * - GlobalRef
   *   • 네이티브 어디서든(다른 스레드 포함) 장기간 사용 가능
   *   • GC가 수거하지 못하도록 강한 참조를 유지함
   *   • 반드시 네이티브 스레드의 소멸자 함수에서 DeleteGlobalRef()로 직접 객체의 자원을 해제해야 함
   *
   * - SurfaceTexture를 GlobalRef로 승격하는 이유
   *   • android.graphics.SurfaceTexture 는 기본적으로 Kotlin 단에서 LocalRef로 생성됨
   *     → JNI 호출 종료 시 java 의 gc 가 자동으로 해당 객체 메모리를 수거하면서 무효화됨.
   *   • c++ 네이티브 스레드(특히, 렌더 스레드) 등 "다른 스레드/오랜 기간"에서 참조해야 하므로,
   *     GlobalRef로 보관해서 해당 객체의 수명을 오래 붙잡아둘 수 있게 됨으로써 안전하게 사용할 수 있음
   */
  m_surfaceTexture = env->NewGlobalRef(surfaceTextureGlobal);
};

// 자원 정리: SurfaceTexture GlobalRef 해제, GL 리소스(FBO/텍스처/프로그램) 파괴
VideoPlayerAndroid::~VideoPlayerAndroid() {
  if (JNIEnv* env = getEnv()) {
    if (m_surfaceTexture) {
      env->DeleteGlobalRef(m_surfaceTexture);
    }
  }
  destroyGL();
};

// 단순 미디어 파일 경로 보관(옵션) – 실제 디코딩은 Kotlin/Java 쪽에서 수행된다고 가정
bool VideoPlayerAndroid::open(const std::string& path) {
  // 경로 저장 외에 별도 작업 없음(실제 decoding 작업은 Java/Kotlin 측(android/app/src/main/java/com/sampleapp/video/MediaCodecVideoDecoder.kt))
  m_path = path;
  return true;
};

// 렌더링 가능 상태로 전환 (VideoPlayerAndroid::update() 내에서 프레임을 처리하도록 플래그 설정)
bool VideoPlayerAndroid::start() {
  m_running = true;
  return true;
};

// 비디오 렌더링 중단 및 현재 SkImage 해제(다음 프레임부터 그리지 않음)
void VideoPlayerAndroid::stop() {
  m_running = false;
  m_image.reset();
};

/**
 * 렌더링 스레드에서 VideoRect::update() 함수 내에서 호출할 함수.
 * - android.graphics.SurfaceTexture.updateTexImage() & 변환행렬 취득
 * - 신규 video frame 으로 갱신된 OES 텍스처를 RGBA 로 변환하여 offscreen 렌더 타겟으로 blit
 * - RGBA 로 변환된 프레임이 기록된 GL 텍스처를 SkImage로 래핑
 */
void VideoPlayerAndroid::update() {
  if (!m_running) return;                           // start() / stop() 에 의한 update 루프 처리 결정
  if (!m_frameReady.exchange(false)) return;        // android.graphics.SurfaceTexture 에 새로운 프레임이 enqueue 되지 않았으면 무시
  if (!m_surfaceTexture || m_oesTex == 0) return;   // android.graphics.SurfaceTexture 또는 OES 텍스쳐가 준비되지 않았으면 무시

  // 현재 네이티브 스레드에서 자바 클래스에 접근하기 위한 스레드 핸들 가져오기
  JNIEnv* env = getEnv();
  if (!env) return;

  // android.graphics.SurfaceTexture 에 정의된 두 메서드를 참조 및 호출하기 위한 id 가져오기
  jclass cls = env->GetObjectClass(m_surfaceTexture);
  static jmethodID midUpd = env->GetMethodID(cls, "updateTexImage", "()V");
  static jmethodID midMat = env->GetMethodID(cls, "getTransformMatrix", "([F)V");

  // 1. android.graphics.SurfaceTexture 에 업데이트된 신규 video frame -> OES 텍스쳐에 갱신
  env->CallVoidMethod(m_surfaceTexture, midUpd);

  // 2. 갱신된 video frame 에 대한 변환행렬 취득
  jfloatArray arr = env->NewFloatArray(16);
  env->CallVoidMethod(m_surfaceTexture, midMat, arr);
  env->GetFloatArrayRegion(arr, 0, 16, m_texMatrix);
  env->DeleteLocalRef(arr);

  // 3. 비디오 크기 초기화 (실제 해상도는 Java 측에서 알려주면 갱신 가능)
  if (m_w == 0 || m_h == 0) {
    // 최초에는 현재 타깃 크기 또는 기본값 사용
    m_w = m_allocW ? m_allocW : 640;
    m_h = m_allocH ? m_allocH : 360;
  }

  // video frame 이 갱신된 OES 텍스쳐 -> RGBA 변환에 필요한 쉐이더 프로그램 & 렌더 타겟 생성되었는지 확인
  if (!createShaderProgram()) return;
  if (!createRenderTarget(m_w, m_h)) return;

  // OES 텍스쳐 -> RGBA 변환 후 offscreen 렌더 타겟에 복사하기 수행
  blitOesToRgba();

  // 변환된 RGBA 가 렌더링된 offscreen color attachment 텍스쳐 버퍼 -> SkImage 로 wrapping
  wrapSkImage();
};

// 매개변수로 전달된 크기에 맞는 RGBA 로 변환된 프레임을 담을 offscreen 렌더 타깃(FBO/텍스처) 생성
bool VideoPlayerAndroid::createRenderTarget(int w, int h) {
  // 이미 offscreen 프레임버퍼의 color attachment 버퍼가 존재하고, 크기가 리사이징되지 않았다면 생략
  if (m_colorTex && w == m_allocW && h == m_allocH) {
    return true;
  }

  // 기존 GL 리소스 정리 후 새로 생성
  destroyGL();
  m_allocW = w;
  m_allocH = h;

  // offscreen 프레임버퍼에 바인딩할 color attachment 생성
  glGenTextures(1, &m_colorTex);
  glBindTexture(GL_TEXTURE_2D, m_colorTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // offscreen 프레임버퍼 생성 및 color attachment 텍스쳐 바인딩
  glGenFramebuffers(1, &m_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTex, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return true;
};

// 원시 프레임(YUV) -> RGBA 로 변환 시 사용할 OES 샘플링 셰이더 프로그램 생성 및 링킹
bool VideoPlayerAndroid::createShaderProgram() {
  if (m_prog) {
    return true;
  }

  auto compileShader = [&](GLenum type, const char* src) -> GLuint {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint isCompiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (!isCompiled) {
      glDeleteShader(shader);
      return 0;
    }
    return shader;
  };

  GLuint vs = compileShader(GL_VERTEX_SHADER, kVS);
  GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFS);
  if (!vs || !fs) {
    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);
    return false;
  }

  m_prog = glCreateProgram();
  glAttachShader(m_prog, vs);
  glAttachShader(m_prog, fs);

  // 각 attribute 변수 location 할당
  glBindAttribLocation(m_prog, 0, "aPos");
  glBindAttribLocation(m_prog, 1, "aUv");

  glLinkProgram(m_prog);
  glDeleteShader(vs);
  glDeleteShader(fs);

  GLint linked = GL_FALSE;
  glGetProgramiv(m_prog, GL_LINK_STATUS, &linked);
  if (!linked) {
    glDeleteProgram(m_prog);
    m_prog = 0;
    return false;
  }

  m_locPos = 0;
  m_locUv = 1;
  m_locMat = glGetUniformLocation(m_prog, "uTexMatrix");

  return true;
};

// 원시 프레임(YUV)이 갱신된 OES 텍스처를 RGBA 로 변환된 프레임을 담는 offscreen 렌더 타깃에 렌더링(복사)
void VideoPlayerAndroid::blitOesToRgba() {
  // OES 텍스쳐를 적용할 quad 정점 데이터
  static const GLfloat quad[] = {
    //  xy   uv
    -1.f,-1.f, 0.f,0.f,
     1.f,-1.f, 1.f,0.f,
    -1.f, 1.f, 0.f,1.f,
     1.f, 1.f, 1.f,1.f
  };

  // offscreen 프레임버퍼 생성 및 바인딩 전, 기존에 바인딩되어 있던 프레임버퍼 id 저장해 둠.
  GLint prevFbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

  // RGBA 로 변환된 프레임을 렌더링할 offscreen 프레임버퍼 바인딩
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
  glViewport(0, 0, m_allocW, m_allocH);
  glDisable(GL_BLEND);

  // 원시 프레임(YUV) -> RGBA 변환용 쉐이더 프로그램 바인딩
  glUseProgram(m_prog);

  // 쉐이더 프로그램에 uniform 변수 전송 및 sampler 에 texture unit 전송
  glUniformMatrix4fv(m_locMat, 1, GL_FALSE, m_texMatrix);

  // 쉐이더 프로그램에서 OES 텍스쳐 1개만 사용하므로, 자동으로 0번 텍스쳐 유닛을 샘플링 할 것임.
  // -> 0번 텍스쳐 유닛 위치에 바인딩되도록 0번 위치 활성화 및 바인딩
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, m_oesTex);

  // 쉐이더 프로그램의 각 attribute 변수 활성화 및 전송할 정점 데이터 해석 방식 정의
  glEnableVertexAttribArray(m_locPos);
  glEnableVertexAttribArray(m_locUv);
  glVertexAttribPointer(m_locPos, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad);
  glVertexAttribPointer(m_locUv, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), quad + 2);

  // OES 텍스쳐를 입힌 quad 드로우 콜 호출
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  // attribute 변수 비활성화
  glDisableVertexAttribArray(m_locPos);
  glDisableVertexAttribArray(m_locUv);

  // offscreen 렌더링 완료 후 기존에 바인딩되어 있던 프레임버퍼로 rollback
  glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
};

// RGBA 로 변환된 프레임이 기록된 GL 텍스처를 SkImage로 래핑 (Skia가 그릴 수 있도록 변환)
void VideoPlayerAndroid::wrapSkImage() {
  // GrDirectContext 및 offscreen color attachment(RGBA 변환된 video frame 이 렌더링됨) 이 없으면 무시
  if (!m_gr || !m_colorTex) return;

  // GL 텍스처 핸들(m_colorTex) -> Skia의 BackendTexture 로 변환
  GrGLTextureInfo info{GL_TEXTURE_2D, m_colorTex, GL_RGBA8};
  auto beTex = GrBackendTextures::MakeGL(m_allocW, m_allocH, skgpu::Mipmapped::kNo, info);

  // GL 텍스처를 빌려와서(참조하여) Skia의 SkImage처럼 다루도록 wrapping (텍스쳐 자체는 여전히 OpenGL 컨텍스트에서 관리)
  m_image = SkImages::BorrowTextureFrom(
    m_gr,
    beTex,
    kTopLeft_GrSurfaceOrigin,
    SkColorType::kRGBA_8888_SkColorType,
    kPremul_SkAlphaType,
    nullptr,
    nullptr
  );
};

// 생성한 모든 GL 리소스 파괴(FBO/텍스처/프로그램)
void VideoPlayerAndroid::destroyGL() {
  if (m_fbo) {
    glDeleteFramebuffers(1, &m_fbo);
    m_fbo = 0;
  }
  if (m_colorTex) {
    glDeleteTextures(1, &m_colorTex);
    m_colorTex = 0;
  }
  if (m_prog) {
    glDeleteProgram(m_prog);
    m_prog = 0;
  }
  m_image.reset();
};

// 현재 스레드에서 자바 가상머신에 접근할 수 있는 JNIEnv 얻기 (없으면 JavaVM->AttachCurrentThread() 시도)
JNIEnv* VideoPlayerAndroid::getEnv() {
  if (!g_vm) {
    return nullptr;
  }

  JNIEnv* env = nullptr;

  if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
    if (g_vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      return nullptr;
    }
  }

  return env;
};

#endif