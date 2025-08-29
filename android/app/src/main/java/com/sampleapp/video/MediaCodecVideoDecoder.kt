package com.sampleapp.video

import android.content.Context
import android.media.*
import android.net.Uri
import android.util.Log
import android.view.Surface
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean

/**
 * MediaCodecVideoDecoder
 *
 * 역할:
 * - MediaExtractor: 동영상 파일에서 "압축된 데이터"(H.264/H.265 등 비트스트림) 읽어옴
 * - MediaCodec: 하드웨어 디코더. 압축 데이터 → 디코딩된 원시 프레임으로 변환
 * - Surface: 디코딩된 프레임이 쓰여질 곳. (OES 텍스처로 연결된 SurfaceTexture)
 *
 * MediaCodec 내부에는 "두 개의 별도 버퍼 큐"가 있음:
 * 1. 입력 버퍼 큐 (Input Buffer Queue)
 * ```
 *      - 우리가 비트스트림(압축 데이터)을 써 넣는 공간
 *      - 앱이 비트스트림을 읽어 넣으면, 코덱이 꺼내서 디코딩
 * ```
 * 2. 출력 버퍼 큐 (Output Buffer Queue)
 * ```
 *      - 디코더가 처리한 결과(디코딩된 프레임)가 쌓이는 공간
 *      - 앱은 여기서 프레임을 dequeue하고, Surface로 내보낼 수 있음
 * ```
 * 즉, "하나의 버퍼를 입출력 양방향으로 쓰는 것"이 아니라 "입력 큐 = 압축 데이터", "출력 큐 = 디코딩된 프레임" → 두 개의 독립 버퍼 큐가 존재.
 *
 * 실행 흐름: open(context, source, surface) → start() → (내부 스레드에서) 입력 큐에 비트스트림 공급 → 출력 큐에서 Surface로 프레임
 * 방출 → stop() 시 리소스 해제
 */
class MediaCodecVideoDecoder {

  private var extractor: MediaExtractor? = null // 비디오 파일/스트림 읽어오는 도구
  private var codec: MediaCodec? = null // 하드웨어 디코더
  private val running = AtomicBoolean(false) // 디코딩 루프 실행 여부
  @Volatile private var thread: Thread? = null // 디코딩 작업용 스레드

  /**
   * open()
   * - MediaExtractor로 소스 열고, 비디오 트랙 찾아서 MediaCodec에 연결
   * - MediaCodec은 Surface 출력 모드로 설정 (즉 CPU 메모리에 복사하지 않고 SurfaceTexture로 직접 전달)
   */
  fun open(context: Context, source: String, surface: Surface): Boolean {
    val ext = MediaExtractor()
    try {
      // 데이터 소스 설정
      // - 파일 경로, content:// URI, http(s):// 등 다양한 입력 지원
      val uri = Uri.parse(source)
      if (uri.scheme == "content" || uri.scheme == "file" || uri.scheme == "android.resource") {
        ext.setDataSource(context, uri, null)
      } else {
        ext.setDataSource(source)
      }

      // 비디오 트랙 찾기
      var track = -1
      var format: MediaFormat? = null
      for (i in 0 until ext.trackCount) {
        val f = ext.getTrackFormat(i)
        val mime = f.getString(MediaFormat.KEY_MIME) ?: continue
        if (mime.startsWith("video/")) {
          track = i
          format = f
          break
        }
      }
      if (track < 0 || format == null) {
        ext.release()
        return false
      }

      // 선택된 트랙 활성화
      ext.selectTrack(track)

      // 디코더(MediaCodec) 생성
      val mime = format.getString(MediaFormat.KEY_MIME)!!
      val c = MediaCodec.createDecoderByType(mime)

      // 디코더 설정 (출력 Surface를 OES 텍스처로 연된 SurfaceTexture로 지정)
      // configure(format, surface, crypto, flags)
      // - format : 입력 비디오 정보 (폭/높이, 코덱 종류 등)
      // - surface : 디코딩 결과가 그려질 Surface (OES 텍스처로 연결된 SurfaceTexture)
      // - crypto  : DRM용 (null이면 일반 콘텐츠)
      // - flags   : 인코더/디코더 모드 지정 (0 = 디코더)
      c.configure(format, surface, null, 0)

      extractor = ext
      codec = c
      return true
    } catch (e: Exception) {
      try {
        ext.release()
      } catch (_: Exception) {}
      Log.e("MediaCodecVideoDecoder", "open failed: ${e.message}", e)
      return false
    }
  }

  /**
   * start()
   * - MediaCodec 시작 후, 별도 스레드에서 디코딩 루프를 돌림
   * - 별도 스레드에서 "입력 큐 → 디코딩 → 출력 큐" 반복
   */
  fun start() {
    val c = codec ?: return
    if (running.get()) return
    c.start() // 디코더 시작
    running.set(true)

    thread =
            Thread {
              val e = extractor!!
              val info = MediaCodec.BufferInfo() // 출력 버퍼 상태 구조체
              var inputEOS = false // End-Of-Stream 도달 여부 플래그

              try {
                while (running.get()) {
                  // =========================
                  // (1) 입력 버퍼 큐 처리 (입력 큐에 압축되어 있는 비트스트림 넣기)
                  // =========================
                  if (!inputEOS) {
                    // 빈 입력 버퍼 하나 얻기
                    val inIdx = c.dequeueInputBuffer(10_000)
                    if (inIdx >= 0) {
                      val buf: ByteBuffer? = c.getInputBuffer(inIdx) // 압축 데이터 쓸 공간
                      val size = e.readSampleData(buf!!, 0) // MediaExtractor → 비트스트림 읽기
                      if (size < 0) {
                        // 데이터 없음 → 입력 끝났음을 디코더에 알림
                        c.queueInputBuffer(inIdx, 0, 0, 0, MediaCodec.BUFFER_FLAG_END_OF_STREAM)
                        inputEOS = true
                      } else {
                        val ptsUs = e.sampleTime // 현재 샘플의 표시 시간 (Presentation Time Stamp(= PTS))
                        val flags = e.sampleFlags // 키프레임 여부 등
                        // 읽어온 데이터를 디코더 입력 큐에 집어넣음
                        c.queueInputBuffer(inIdx, 0, size, ptsUs, flags)
                        e.advance() // 다음 샘플로 이동
                      }
                    }
                  }

                  // =========================
                  // (2) 출력 버퍼 큐 처리 (디코딩된 원시 프레임 꺼내기)
                  // (원시 프레임 : MediaCodec 이 디코딩을 끝내고 내놓는 “원시 프레임(raw frame)”이라는 건 보통 YUV 계열 포맷의 영상
                  // 데이터)
                  // =========================
                  when (val outIdx = c.dequeueOutputBuffer(info, 10_000)) {
                    MediaCodec.INFO_TRY_AGAIN_LATER -> {
                      // 지금은 디코딩된 프레임 없음 → 잠시 대기
                    }
                    MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                      // 출력 포맷 바뀜 (해상도, 색상 등)
                      val newFormat = c.outputFormat
                      Log.d("Decoder", "Output format changed: $newFormat")
                    }
                    MediaCodec.INFO_OUTPUT_BUFFERS_CHANGED -> {
                      // (구버전 API에서만 의미 있음, 무시 가능)
                    }
                    else -> {
                      if (outIdx >= 0) {
                        // 디코딩 완료된 프레임 하나를 Surface로 방출
                        // render=true → 연결된 SurfaceTexture에 프레임 enqueue
                        c.releaseOutputBuffer(outIdx, /* render */ true)

                        // EOS 플래그 확인 (출력 측에서도 스트림 종료 알림)
                        if ((info.flags and MediaCodec.BUFFER_FLAG_END_OF_STREAM) != 0) {
                          Log.d("Decoder", "End of stream reached")
                          break
                        }
                      }
                    }
                  }
                }
              } catch (ex: Exception) {
                Log.e("Decoder", "decode loop error: ${ex.message}", ex)
              } finally {
                // 루프 종료 시 정리
                stop()
              }
            }
                    .apply {
                      name = "VideoDecodeThread"
                      start()
                    }
  }

  /**
   * stop()
   * - 디코딩 스레드 종료 및 MediaCodec/Extractor 리소스 해제
   */
  fun stop() {
    running.set(false)
    val t = thread
    if (t != null && t.isAlive) {
      try {
        t.join(1000)
      } catch (_: InterruptedException) {}
    }
    thread = null

    try {
      codec?.stop()
    } catch (_: Exception) {}
    try {
      codec?.release()
    } catch (_: Exception) {}
    codec = null

    try {
      extractor?.release()
    } catch (_: Exception) {}
    extractor = null
  }
}
