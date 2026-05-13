import type { SystemType } from './types'
import './ControlBar.css'

interface ControlBarProps {
  activeSystem: SystemType
  hasRom: boolean
  isRunning: boolean
  romName: string
  onOpenRom: () => void
  onReset: () => void
  onTogglePlay: () => void
  onRegionChange: (region: number) => void
}

export function ControlBar({
  activeSystem,
  hasRom,
  isRunning,
  romName,
  onOpenRom,
  onReset,
  onTogglePlay,
  onRegionChange,
}: ControlBarProps) {
  if (activeSystem === 'flash') {
    return (
      <div className="control-bar">
        <div className="flash-placeholder">Flash 模拟器 &middot; 即将推出</div>
      </div>
    )
  }

  if (activeSystem === 'test') {
    return (
      <div className="control-bar">
        <div className="controls-left">
          <button
            className="btn primary"
            onClick={onTogglePlay}
            disabled={!hasRom}
          >
            &#x25B6; 运行测试
          </button>
        </div>
      </div>
    )
  }

  return (
    <div className="control-bar">
      <div className="controls-left">
        <button className="btn" onClick={onOpenRom}>
          &#x23FB; 打开
        </button>
        <button className="btn" onClick={onReset} disabled={!hasRom}>
          &#x21BA; 重置
        </button>
        <button
          className="btn primary"
          onClick={onTogglePlay}
          disabled={!hasRom}
        >
          {isRunning ? '\u23F8' : '\u25B6'} {isRunning ? '暂停' : '运行'}
        </button>
        <select
          className="btn region-select"
          onChange={e => onRegionChange(Number(e.target.value))}
          defaultValue={0}
        >
          <option value={0}>NTSC</option>
          <option value={1}>PAL</option>
        </select>
      </div>
      <div className="controls-right">
        {romName && <span className="rom-name">{romName}</span>}
      </div>
    </div>
  )
}
