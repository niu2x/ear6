import { useRef, useEffect, useState } from 'react'

interface Ear6Module {
  HEAPU8: Uint8Array
  _ear6_web_create(system: number): number
  _ear6_web_destroy(ctx: number): void
  _ear6_web_load(ctx: number, data: number, size: number): number
  _ear6_web_step(ctx: number): number
  _ear6_web_get_framebuffer(ctx: number): number
  _ear6_web_get_frame_width(ctx: number): number
  _ear6_web_get_frame_height(ctx: number): number
}

declare global {
  interface Window {
    createEar6: () => Promise<Ear6Module>
  }
}

export function EmulatorCanvas() {
  const canvasRef = useRef<HTMLCanvasElement>(null)
  const [status, setStatus] = useState('loading wasm...')

  useEffect(() => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx2d = canvas.getContext('2d')!
    let running = true

    async function init() {
      const mod = await window.createEar6()
      const ctx = mod._ear6_web_create(0)
      if (!ctx) {
        setStatus('create failed')
        return
      }

      mod._ear6_web_load(ctx, 0, 0)
      setStatus('running')

      function loop() {
        if (!running) return

        mod._ear6_web_step(ctx)

        const fbPtr = mod._ear6_web_get_framebuffer(ctx)
        const w = mod._ear6_web_get_frame_width(ctx)
        const h = mod._ear6_web_get_frame_height(ctx)

        if (fbPtr && w && h) {
          if (canvas.width !== w || canvas.height !== h) {
            canvas.width = w
            canvas.height = h
          }
          const imageData = ctx2d.createImageData(w, h)
          const src = new Uint8Array(mod.HEAPU8.buffer, fbPtr, w * h * 4)
          imageData.data.set(src)
          ctx2d.putImageData(imageData, 0, 0)
        }

        requestAnimationFrame(loop)
      }

      requestAnimationFrame(loop)
    }

    init()

    return () => {
      running = false
    }
  }, [])

  return (
    <div>
      <canvas ref={canvasRef} width={256} height={240} />
      <p>{status}</p>
    </div>
  )
}
