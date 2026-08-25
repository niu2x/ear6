import type { SystemType } from './types'
import './TitleBar.css'

interface TitleBarProps {
  status: 'idle' | 'running' | 'paused'
  system: SystemType
  isRunning: boolean
}

const STATUS_DOT_COLOR: Record<string, string> = {
  idle: '#666',
  running: '#2ec4b6',
  paused: '#e6c229',
}

export function TitleBar({ status, system, isRunning }: TitleBarProps) {
  return (
    <div className="title-bar">
      <div className="title-left">
        <span className="title-dot" style={{ background: STATUS_DOT_COLOR[status] }} />
        <span className="title-text">Ear6</span>
      </div>
      <div className="title-center">
        {system === 'nes' && isRunning && (
          <div className="system-leds">
            <span className="led led-power on" />
            <span className="led led-running blink" />
            <span className="led led-error" />
          </div>
        )}
      </div>
      <div className="title-right">
        <span className="deco-dot" />
        <span className="deco-dot" />
        <span className="deco-dot" />
      </div>
    </div>
  )
}
