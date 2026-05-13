import { useRef, useEffect } from 'react'
import type { Ear6Module } from './types'
import './CrtScreen.css'

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
    return () => cancelAnimationFrame(frameId)
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
