#pragma once

#include <memory>
#include <atomic>
#include <functional>
#include <string>
#include "../video/Timeline.h"
#include "../EncoderConfig.h"

/*
 * IEncoder
 * - iOS/Android 등 다양한 플랫폼에서 동일한 인코딩 워크플로우를 공유하기 위한 공용 인터페이스입니다.
 * - 구현체는 플랫폼별 디렉터리에서 이 인터페이스를 상속받아 실제 인코딩을 수행합니다.
 *     예) Android: shared/encoder/android/AndroidMediaCodecEncoder
 *
 * 사용 흐름(권장):
 *   1) setTimeline(...)     : 미리보기(Preview)에서 사용하는 Timeline을 그대로 연결
 *   2) prepare(config)      : 출력 해상도/프레임레이트/비트레이트/경로 등 설정에 따라 초기화
 *   3) encodeBlocking(...)  : 실제 인코딩 실행(모든 프레임 처리). 호출한 메인 스레드는 끝날 때까지 기다림(== 함수 내부가 동기적으로 구현되어 있음.)
 *   4) release()            : 내부 리소스 정리
 *
 * 스레드 모델:
 * - encodeBlocking(...)은 블로킹 함수(인코딩 동기적으로 구현된 함수 == 내부에서 자체적인 작업 스레드를 생성하지 않음.)이므로,
 *   메인(UI) 스레드에서 직접 호출하면 인코딩이 끝날 때까지 메인 스레드는 아무런 작업도 하지 못하게 됩니다.
 * - 메인 스레드를 막지 않으려면, encodeBlocking은 반드시 별도 작업 스레드를 만들어서 그 안에서 호출하세요.
 *   (ex> Engine::m_encodeThread). -> GPT5 는 이런 걸 "백그라운드 스레드" 라는 어려운 말로 표현하기도 한답니다.
 * - setTimeline/prepare/release는 호출자가 동기화 책임을 집니다(일반적으로 Engine에서 단일 스레드 소유).
 *
 * 타임라인 공유:
 * - Preview 경로와 결과 일치를 위해 Timeline::render를 그대로 사용하도록 의도되었습니다.
 * - Renderer가 보유한 Timeline을 snapshot으로 받아 setTimeline에 전달하는 패턴을 권장합니다.
 */

/**
 * @brief 공용 인코더 인터페이스(플랫폼 불문)
 */
class IEncoder
{
public:
  virtual ~IEncoder() = default;

  /**
   * @brief 인코딩 소스로 사용할 Timeline 설정
   * @param tl Preview에서 사용 중인 Timeline (스냅샷/shared_ptr)
   * @note 구현체는 이 Timeline을 사용하여 동일한 렌더링 결과를 생성해야 합니다.
   */
  virtual void setTimeline(std::shared_ptr<Timeline> tl) = 0;

  /**
   * @brief 인코더 준비(리소스/포맷/EGL/Surface 등 초기화)
   * @param cfg 해상도/프레임레이트/비트레이트/코덱 MIME/출력 경로 등
   * @return 성공 여부
   * @note 성공 시 encodeBlocking을 호출할 수 있습니다.
   */
  virtual bool prepare(const EncoderConfig& cfg) = 0;

  /**
   * @brief 인코딩을 동기(블로킹) 방식으로 수행
   * @param cancelFlag 외부에서 true로 설정 시 안전하게 중단 시도
   * @param onProgress 0.0 ~ 1.0 범위의 진행률 콜백(선택)
   * @return 성공 여부
   * @note 이 함수는 "블로킹(동기적으로 실행되는 함수)"이다: 작업이 끝날 때까지 해당 함수를 호출한 메인 스레드는 다음 줄로 진행하지 않는다.
   * - 메인 스레드를 막지 않으려면 별도 작업 스레드(ex> Engine::m_encodeThread)를 생성 후 그 안에서 작업을 호출하도록 멀티스레딩 처리한다.
   * - 외부에서 cancelFlag가 true가 되면 프레임 인코딩 루프를 멈추고 안전하게 인코딩 작업 종료를 시도한다.
   * - onProgress(0.0~1.0)는 진행률 콜백이며, 인코딩 스레드에서 호출된다.
   */
  virtual bool encodeBlocking(std::atomic<bool>& cancelFlag, std::function<void(double)> onProgress) = 0;

  /**
   * @brief 내부 리소스(EGL/Surface/Codec/Muxer 등) 해제
   * @note prepare/encodeBlocking 과정이 끝난 후 항상 호출하세요.
   */
  virtual void release() = 0;

  /**
   * @brief 최종 출력 파일의 절대 경로 반환
   * @return 출력 경로(prepare 시 설정한 경로)
   */
  virtual std::string outputPath() const = 0;
};
