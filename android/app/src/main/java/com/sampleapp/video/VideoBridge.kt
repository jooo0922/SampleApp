package com.sampleapp.video

import android.content.Context

/**
 * C++/JNI에서 호출하기 쉬운 정적 헬퍼
 * - start(context, pathOrUri, loop=false)
 * - stop()
 *
 * 실전에서는 동시 다중 인스턴스가 필요하다면 인스턴스 관리 테이블을 두고 "세션 ID"로 구분하는 구조 권장.
 */
object VideoBridge {
  @Volatile private var decoder: MediaCodecVideoDecoder? = null
  private val lock = Any()

  fun start(context: Context, source: String): Boolean {
    synchronized(lock) {
      if (decoder != null) {
        // 이미 실행 중이면 재시작 방지 또는 stop 후 시작
        return false
      }
      val surf = VideoSurfaceHelper.prepare()
      val d = MediaCodecVideoDecoder()
      val ok = d.open(context, source, surf)
      if (!ok) {
        VideoSurfaceHelper.release()
        return false
      }
      decoder = d
      d.start()
      return true
    }
  }

  fun stop() {
    synchronized(lock) {
      decoder?.stop()
      decoder = null
      VideoSurfaceHelper.release()
    }
  }
}
