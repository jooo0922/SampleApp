package com.sampleapp.video

import android.graphics.SurfaceTexture
import android.os.Handler
import android.os.HandlerThread
import android.view.Surface

/**
 * VideoSurfaceHelper
 *
 * 역할:
 * - 네이티브(C++)가 만든 OES(OpenGL External) 텍스처 ID를 받아서 SurfaceTexture를 생성.
 * - SurfaceTexture는 BufferQueue의 consumer로 동작하고, MediaCodec이 디코딩한 결과를 받음.
 * - SurfaceTexture를 Surface로 래핑해서 MediaCodec.configure(..., surface, ...)에 넘김.
 * - MediaCodec → Surface에 프레임 디코딩 → BufferQueue → SurfaceTexture → OES 텍스처에 연결.
 *
 * 즉,
 *   MediaCodec 디코딩 결과가 CPU 복사 없이 GPU 텍스처에 들어오는 near zero-copy 파이프라인.
 *
 * 중요:
 * - Surface 자체는 단순 핸들(버퍼 producer). 이 Surface가 어떤 consumer에 연결되느냐에 따라
 *   1. SurfaceView의 Surface → 바로 화면 출력
 *   2. SurfaceTexture의 Surface → OES 텍스처 업데이트(오프스크린 후처리) 로 달라짐.
 *
 * - SurfaceTexture와 OES 텍스처의 동기화는 자동이 아님.
 *   반드시 GL 스레드에서 `updateTexImage()`를 호출해야 OES 텍스처가 최신 프레임으로 갱신됨.
 *   (→ onFrameAvailable()에서 네이티브로 "프레임 들어왔다" 신호만 주고,
 *      실제 updateTexImage 호출은 GL 렌더 스레드에서 수행해야 안전.)
 */
object VideoSurfaceHelper : SurfaceTexture.OnFrameAvailableListener {

    init { System.loadLibrary("appmodules") }

    // 네이티브 구현 함수
    // - nativeCreateOESTexture(): glGenTextures(GL_TEXTURE_EXTERNAL_OES) 등으로 텍스처 ID 생성
    // - nativeOnVideoFrameAvailable(): onFrameAvailable에서 호출되어 네이티브 렌더 스레드에 신호 전달
    private external fun nativeCreateOESTexture(): Int
    private external fun nativeOnVideoFrameAvailable()

    private var surfaceTexture: SurfaceTexture? = null
    var surface: Surface? = null
        private set

    // 콜백 스레드를 따로 두면 메인 스레드 혼잡을 줄일 수 있음
    private val callbackThread = HandlerThread("VideoFrameCallback").apply { start() }
    private val callbackHandler = Handler(callbackThread.looper)

    /**
     * prepare()
     * - OES 텍스처 ID를 네이티브에서 받아와서 SurfaceTexture를 생성
     * - SurfaceTexture를 Surface로 래핑하여 MediaCodec에 넘길 준비
     */
    fun prepare(): Surface {
        if (surface == null) {
            val texId = nativeCreateOESTexture()
            surfaceTexture = SurfaceTexture(texId).apply {
                // 프레임 도착 신호를 받을 리스너 등록
                // 실제 updateTexImage() 호출은 여기서 하지 않고, 네이티브로 신호만 전달
                setOnFrameAvailableListener(this@VideoSurfaceHelper, callbackHandler)
            }
            surface = Surface(surfaceTexture)
        }
        return surface!!
    }

    /**
     * updateDefaultBufferSize()
     * - MediaCodec 출력 포맷(width/height, crop 정보)에 맞게 SurfaceTexture 내부 버퍼 크기를 조정
     * - 이걸 맞춰주지 않으면 GL에서 샘플링할 때 스케일/블러/크롭 문제 생길 수 있음
     */
    fun updateDefaultBufferSize(width: Int, height: Int) {
        surfaceTexture?.setDefaultBufferSize(width, height)
    }

    /**
     * onFrameAvailable()
     * - SurfaceTexture에 새로운 프레임이 들어왔을 때 호출됨
     * - 여기서는 GL 호출 금지 (updateTexImage는 GL 컨텍스트 필요)
     * - 네이티브로 "프레임 들어왔다" 신호만 보냄 → GL 렌더 스레드가 updateTexImage() 호출하도록
     *
     * updateTexImage():
     * - SurfaceTexture 클래스에 이미 구현된 API (Java/Kotlin/NDK 모두 제공)
     * - BufferQueue에서 최신 프레임을 가져와 연결된 OES 텍스처의 내용으로 업데이트
     * - 반드시 GL 스레드에서 호출해야 안전
     * - 호출 후 getTransformMatrix()로 UV 매트릭스를 얻어야 영상 회전/크롭이 올바르게 표시됨
     */
    override fun onFrameAvailable(st: SurfaceTexture) {
        nativeOnVideoFrameAvailable()
    }

    fun getSurfaceTexture(): SurfaceTexture? = surfaceTexture

    fun release() {
        surface?.release(); surface = null
        surfaceTexture?.release(); surfaceTexture = null
        // callbackThread.quitSafely() // 앱 종료 시 정리 가능
    }
}
