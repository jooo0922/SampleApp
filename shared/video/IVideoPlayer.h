#pragma once
#include <core/SkImage.h>
#include <string>

// IVideoPlayer
// 비디오 디코딩 + GPU 업로드 + Skia 래핑 파이프라인을 추상화하는 최소 인터페이스.
// 구현체(예: Android MediaCodec 기반)는 다음을 책임짐:
//  1) 비디오 소스 열기 및 디코더 구성
//  2) Surface / SurfaceTexture (OES) 경로를 통한 프레임 수신
//  3) onFrameAvailable 신호(스레드 상관 없음) -> 렌더 스레드 update 시점에 GL updateTexImage
//  4) OES 텍스처를 일반 2D 텍스처/FBO로 blit 후 SkImage 형태로 wrapping
//  5) 프레임 크기(width/height) 추적 및 포맷 체인지 처리
// 스레드 모델(권장):
//  - open / start / stop : 제어 스레드 (JS → C++ 진입 스레드) 에서 호출
//  - notifyFrame         : Java/Kotlin SurfaceTexture 콜백 스레드(JNI) → lock-free 플래그 세팅
//  - update / currentFrame: 렌더 스레드 루프 내부
//  - width / height       : 어느 스레드든 가능 (초기 0 가능)
// 라이프사이클: open() -> start() -> (notifyFrame/update 반복) -> stop() -> 재사용 가능
// 예외 / 실패 처리: open(), start() 실패 시 false 반환하고 내부 상태는 안전한 초기 상태 유지.
class IVideoPlayer {
public:
  virtual ~IVideoPlayer() = default;

  // open
  //  - 재생할 비디오 자원의 경로/URI/식별자를 설정.
  //  - 디코더 구성이 아직 완료되지 않았더라도 경로 메타데이터 저장 수준은 수행.
  //  - 이미 열린 상태에서 다른 경로로 재호출 시, 구현체 정책에 따라 기존 리소스 정리 후 교체하거나 false 반환.
  //  - start() 이전에만 의미 있으며, start() 후 다시 open()은 일반적으로 허용하지 않음(필요 시 stop() 후 open()).
  virtual bool open(const std::string& path) = 0; // (Kotlin MediaCodec 측에서 미리 prepare 했다고 가정 가능; 여기서는 경로 저장/검증)

  // start
  //  - 디코딩을 실제로 시작. MediaCodec start, SurfaceTexture listener 등록 등.
  //  - 이미 start 상태라면 안전하게 no-op 처리 (idempotent) 가능.
  //  - 성공 시 true, 실패 시 false(로그 남기고 내부 상태 롤백).
  virtual bool start() = 0;

  // stop
  //  - 디코딩 중지 & 스레드, Surface, 텍스처, 큐 등 리소스 해제.
  //  - 여러 번 호출해도 안전해야 하며 (guard) 이후 재 start() 가능하도록 상태 초기화.
  virtual void stop() = 0;

  // notifyFrame
  //  - 새로운 디코드 프레임이 SurfaceTexture 에 도착했다는 신호.
  //  - 비-렌더 스레드(예: Java main/UI 스레드)에서 호출됨 → 내부에서 atomic 플래그만 세팅 (lock-heavy 작업 금지).
  //  - 실제 GL updateTexImage() / FBO blit 는 update()에서 처리.
  virtual void notifyFrame() = 0;   // SurfaceTexture onFrameAvailable 콜백 대응

  // update
  //  - 렌더 스레드 루프에서 주기적으로 호출.
  //  - notifyFrame()으로 세팅된 frameAvailable 플래그가 있으면:
  //      1) updateTexImage() 호출
  //      2) OES → 2D 텍스처/FBO blit (색 공간 변환 필요 시 수행)
  //      3) SkImage 재생성/업데이트
  //  - frameAvailable 없으면 빠르게 리턴 (오버헤드 최소화).
  virtual void update() = 0;        // (updateTexImage + OES→2D blit + SkImage 준비)

  // currentFrame
  //  - 가장 최근 update()에서 준비한 SkImage 반환.
  //  - 아직 프레임이 없다면 nullptr.
  //  - 호출자는 sk_sp 복사로 참조 카운트 증가 후 렌더 시 사용 (thread-safe 복사 가정).
  virtual sk_sp<SkImage> currentFrame() const = 0;

  // width / height
  //  - 비디오 스트림의 디코드된 원본 프레임 크기.
  //  - 아직 알 수 없으면 0. 포맷 체인지 시 내부 값 갱신.
  virtual int width() const = 0;
  virtual int height() const = 0;
};