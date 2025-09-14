#include "Timeline.h"
#include <algorithm>
#include <cmath>

void Timeline::setSegments(std::vector<Timeline::Segment>& segs) {
  // 주어진 클립 목록의 메모리 소유권을 멤버변수로 "이동" 후 시간 순 정렬
  m_segments = std::move(segs);
  std::sort(m_segments.begin(), m_segments.end(), [](const Timeline::Segment& a, const Timeline::Segment& b){
    return a.start < b.start;
  });

  // 클립 목록 기준으로 전체 영상 길이 재계산
  recomputeDuration();
};

void Timeline::render(const RenderContext& ctx) const {
  if (!ctx.canvas) return;
  if (m_segments.empty()) return;

  // skia canvas 배경색 초기화
  ctx.canvas->clear(SK_ColorBLACK);

  // 현재 시간(ctx.timeSec)에 해당하는 클립 찾기
  const double t = ctx.timeSec;
  int currIdx = -1;
  for (int i = 0; i < (int)m_segments.size(); i++)
  {
    const auto& seg = m_segments[i];
    if (t >= seg.start && t < seg.start + seg.duration) {
      currIdx = i;
      break;
    }
  }
  if (currIdx < 0) {
    // 현재 시간에 해당하는 클립을 찾지 못했다면 마지막 클립으로 지정
    currIdx = (int)m_segments.size() - 1;
  }
  const auto& cur = m_segments[currIdx];

  // fade 구간 여부 판단
  const double tEnd = cur.start + cur.duration;                                   // 현재 시간에 해당하는 클립의 종료 시간
  const double fadeLen = std::max(0.0, cur.xfade);                                // 현재 시간에 해당되는 클립의 fade 길이 (0이면 페이드 없음)
  const double fadeStart = std::max(cur.start, tEnd - fadeLen);                   // 현재 클립이 끝나기 직전, 페이드가 시작되는 시각
  const bool hasNext = (currIdx + 1) < (int)m_segments.size();                    // 다음 클립 존재 여부
  const bool inFade = (fadeLen > 0.0) && hasNext && (t >= fadeStart && t < tEnd); // 현재 시간이 현재 클립의 fade 구간 내에 존재하는지 여부

  if (inFade) {
    /** 현재 시간이 fade 구간에 속하는 경우 */
    // 현재 클립과 blending 할 다음 클립 가져오가
    const auto& next = m_segments[currIdx + 1];

    // 현재 시간을 기반으로 다음 클립에 적용할 투명도 보간 (시간이 지날수록 0 -> 1 로 증가하도록 계산)
    const double a = std::clamp((t - fadeStart) / fadeLen, 0.0, 1.0);

    SkPaint pCur, pNext;
    pCur.setAlpha((int)std::lround((1.0 - a) * 255.0)); // 현재 클립의 투명도는 (1.0 - a) * 255.0 로 지정
    pNext.setAlpha((int)std::lround(a * 255.0));        // 다음 클립의 투명도는 a * 255.0 로 지정

    // 현재 클립과 다음 클립을 보간된 투명도로 각각 그린다.
    if (cur.clip.image) ctx.canvas->drawImageRect(cur.clip.image, cur.clip.dst, SkSamplingOptions(), &pCur);
    if (next.clip.image) ctx.canvas->drawImageRect(next.clip.image, next.clip.dst, SkSamplingOptions(), &pNext);
  } else {
    /** 현재 시간이 fade 구간에 속하지 않는 경우 */
    // 현재 클립만 투명도 100% 로 렌더링
    SkPaint paint;
    paint.setAlpha(255);
    if (cur.clip.image) {
      ctx.canvas->drawImageRect(cur.clip.image, cur.clip.dst, SkSamplingOptions(), &paint);
    }
  }
};

std::shared_ptr<Timeline> Timeline::FromClipRenderData(const std::vector<ClipRenderData>& renderDataList, double clipDuration, double xfade) {
  auto tl = std::make_shared<Timeline>();
  std::vector<Timeline::Segment> segs;  // 생성된 클립들을 저장할 컨테이너
  double cursor = 0.0;                  // 다음에 생성할 클립이 시작될 시간(초)을 계산하기 위해 사용하는 누산값

  for (size_t i = 0; i < renderDataList.size(); i++) {
    // 클립 생성 후 목록에 추가
    segs.push_back(Timeline::Segment{ renderDataList[i], clipDuration, cursor, xfade });
    /**
     * 다음 클립의 시작 시간은 "클립을 보여줄 시간 - 두 클립이 겹치는 시간(xfade)"만큼 앞으로 당김.
     * 이렇게 시작 시간을 계산하면 두 클립의 끝부분이 서로 겹치며 부드럽게 바뀜.
     */
    cursor += clipDuration - std::max(0.0, xfade);
  }

  // 계산된 클립 목록을 시간 순 정렬 및 총 영상 길이 재계산
  tl->setSegments(segs);

  return tl;
};

void Timeline::recomputeDuration() {
  m_totalDuration = 0.0;
  for (auto& seg : m_segments) {
    m_totalDuration = std::max(m_totalDuration, seg.start + seg.duration);
  }
};
