import { useRef, useEffect } from 'react'
import type { Ear6Module } from './types'
import './CrtScreen.css'

const AUDIO_SAMPLE_RATE = 96000

interface CrtScreenProps {
  modRef: React.RefObject<Ear6Module | null>
  activeCtxRef: React.RefObject<number>
  isRunningRef: React.RefObject<boolean>
  isTransitioning: boolean
  hasRom: boolean
  flashAnim: boolean
  wakeAnim: boolean
  onFps: (fps: number) => void
}

function createAudioContext(): AudioContext | null {
  try {
    const ctx = new AudioContext({ sampleRate: AUDIO_SAMPLE_RATE })
    return ctx
  } catch {
    return null
  }
}

function scheduleAudio(
  audioCtx: AudioContext,
  samples: Int16Array,
  nextTimeRef: { current: number },
) {
  if (samples.length === 0) return
  const buffer = audioCtx.createBuffer(1, samples.length, AUDIO_SAMPLE_RATE)
  const channel = buffer.getChannelData(0)
  for (let i = 0; i < samples.length; i++) {
    channel[i] = samples[i] / 32768.0
  }
  const source = audioCtx.createBufferSource()
  source.buffer = buffer
  source.connect(audioCtx.destination)
  let t = nextTimeRef.current
  const now = audioCtx.currentTime
  if (t < now) t = now
  source.start(t)
  nextTimeRef.current = t + buffer.duration
}

export function CrtScreen({
  modRef,
  activeCtxRef,
  isRunningRef,
  isTransitioning,
  hasRom,
  flashAnim,
  wakeAnim,
  onFps,
}: CrtScreenProps) {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const containerRef = useRef<HTMLDivElement>(null)
  const onFpsRef = useRef(onFps)
  onFpsRef.current = onFps
  const audioCtxRef = useRef<AudioContext | null>(null)
  const nextAudioTimeRef = useRef<number>(0)
  const audioStartedRef = useRef(false)

  const animClass = [
    isTransitioning && 'transitioning',
    flashAnim && 'flash',
    wakeAnim && 'wake',
  ].filter(Boolean).join(' ')

  useEffect(() => {
    const el = canvasRef.current
    if (!el) return
    const cvs = el
    const ctx = cvs.getContext('2d')!
    let frameId = 0
    let frameCount = 0
    let lastFpsTime = 0

    function render(time: number) {
      const mod = modRef.current
      const activeCtx = activeCtxRef.current

      if (mod && activeCtx) {
        if (isRunningRef.current) {
          mod._ear6_web_step(activeCtx)

          if (!audioCtxRef.current) {
            audioCtxRef.current = createAudioContext()
          }
          const audioCtx = audioCtxRef.current
          if (audioCtx && audioCtx.state === 'suspended') {
            audioCtx.resume()
          }

          if (audioCtx) {
            const numSamples = mod._ear6_web_get_audio_num_samples(activeCtx)
            if (numSamples > 0) {
              const ptr = mod._ear6_web_get_audiobuffer(activeCtx)
              if (ptr) {
                const samples = new Int16Array(mod.HEAPU8.buffer, ptr, numSamples)
                scheduleAudio(audioCtx, samples, nextAudioTimeRef)
                if (!audioStartedRef.current) {
                  audioStartedRef.current = true
                }
              }
            }
          }
        }

        const fbPtr = mod._ear6_web_get_framebuffer(activeCtx)
        const w = mod._ear6_web_get_frame_width(activeCtx)
        const h = mod._ear6_web_get_frame_height(activeCtx)

        if (fbPtr && w > 0 && h > 0) {
          if (cvs.width !== w || cvs.height !== h) {
            cvs.width = w
            cvs.height = h
          }
          const imageData = ctx.createImageData(w, h)
          const src = new Uint8Array(mod.HEAPU8.buffer, fbPtr, w * h * 4)
          imageData.data.set(src)
          ctx.putImageData(imageData, 0, 0)

          const container = containerRef.current
          if (container) {
            const maxW = container.clientWidth
            const maxH = container.clientHeight
            const scale = Math.min(maxW / w, maxH / h)
            cvs.style.width = `${Math.floor(w * scale)}px`
            cvs.style.height = `${Math.floor(h * scale)}px`
          }
        }
      }

      frameCount++
      if (time - lastFpsTime >= 1000) {
        onFpsRef.current(frameCount)
        frameCount = 0
        lastFpsTime = time
      }

      frameId = requestAnimationFrame(render)
    }

    frameId = requestAnimationFrame(render)
    return () => {
      cancelAnimationFrame(frameId)
      if (audioCtxRef.current) {
        audioCtxRef.current.close()
        audioCtxRef.current = null
      }
    }
  }, [modRef, activeCtxRef, isRunningRef])

  return (
    <div className={`crt-screen ${animClass}`}>
      <div className="crt-screen-inner" ref={containerRef}>
        <canvas ref={canvasRef} />
        {!hasRom && !isTransitioning && (
          <div className="standby-text">
            <span className="standby-label">NO SIGNAL</span>
            <span className="standby-cursor">&#x258D;</span>
          </div>
        )}
      </div>
    </div>
  )
}
