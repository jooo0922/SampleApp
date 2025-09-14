#pragma once
#include <vector>
#include <memory>
#include <cstdint>
#include <core/SkCanvas.h>
#include <core/SkImage.h>
#include <core/SkPaint.h>
#include <core/SkRect.h>

// 캔버스 렌더링에 필요한 정보
struct RenderContext
{
  SkCanvas* canvas = nullptr;     // 화면에 렌더링할 캔버스 (skia canvas)
  int width = 0;                  // 캔버스 너비
  int height = 0;                 // 캔버스 높이
  double timeSec = 0.0;          // 현재 시간(초)

  RenderContext() = default;
  RenderContext(SkCanvas* c, int w, int h, double t = 0.0)
  : canvas(c), width(w), height(h), timeSec(t) {}
};

/**
 * 주어진 시간(RenderContext::timeSec)에 따라 어떤 장면을 렌더링할 지 결정하는 Timeline 모델
 * Preview 및 Encoder 모듈에서 모두 동일한 방식으로 이미지 시퀀스를 렌더링할 수 있도록 설계된 공통 인터페이스
 */
class Timeline
{
public:
  // 렌더링에 필요한 추가 정보 (미래 확장성 고려)
  struct ClipRenderData
  {
    sk_sp<SkImage> image;             // 클립에서 보여줄 이미지
    SkRect dst = SkRect::MakeEmpty(); // 이미지를 skia canvas 내에서 "어디에, 얼마나 크게" 그릴지(위치/크기)

    ClipRenderData() = default;
    ClipRenderData(sk_sp<SkImage> img, const SkRect& dstRect)
      : image(std::move(img)), dst(dstRect) {}
  };

  // "Clip" 을 추상화한 구조체. -> 한 장의 이미지를 언제부터, 얼마나, 어디에 그릴 지 정의하는 단위
  struct Segment
  {
    ClipRenderData clip;              // 클립에서 보여줄 이미지 및 위치/크기 정보
    double duration = 0.0;            // 이미지를 "얼마 동안" 보여줄 지 정의 (초 단위)
    double start = 0.0;               // 이미지를 "언제부터" 보여줄 지 정의 (초 단위)
    double xfade = 0.0;               // 클립이 끝날 때 "다음 이미지로 부드럽게 바뀌는 시간"(cross fade)

    Segment() = default;
    Segment(ClipRenderData c,
          double durationSec,
          double startSec,
          double xfadeSec = 0.0)
    : clip(std::move(c)),
      duration(durationSec),
      start(startSec),
      xfade(xfadeSec) {}
  };

  Timeline() = default;

  /**
   * 주어진 클립 목록을 시간 순으로 재정렬한 뒤,
   * 전체 길이(m_totalDuration) 를 재계산하는 함수
   */
  void setSegments(std::vector<Segment>& segs);

  // 현재 타임라인의 "전체 길이"(초) 반환
  double totalDuration() const { return m_totalDuration; };

  /**
   * 지금 시간(ctx.timeSec)에 맞는 클립을 렌더링하는 함수.
   * 만약 곧 다음 클립으로 넘어갈 시간이면, 두 클립의 이미지를 살짝 섞어(cross fade) 부드럽게 보여줌.
   */
  void render(const RenderContext& ctx) const;

  /**
   * 여러 개의 ClipRenderData를 전달받아 간단히 타임라인 생성
   * - clipDuration: 각 이미지를 몇 초 보여줄지
   * - xfade: 이미지가 바뀔 때, 두 이미지 몇 초 동안 겹쳐서 부드럽게 바꿀지
   */
  static std::shared_ptr<Timeline> FromClipRenderData(const std::vector<ClipRenderData>& renderDataList, double clipDuration, double xfade);

private:
  // 재구축된 클립 목록을 보고 전체 길이를 재계산
  void recomputeDuration();

private:
  std::vector<Segment> m_segments;          // 클립 목록
  double m_totalDuration;                   // 전체 길이(모든 클립을 다 보면 몇 초인지)
};
