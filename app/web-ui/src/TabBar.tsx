import type { SystemType } from './types'
import { SYSTEM_LABELS } from './types'
import './TabBar.css'

const SYSTEMS: SystemType[] = ['nes', 'test', 'flash']

interface TabBarProps {
  active: SystemType
  onChange: (system: SystemType) => void
  isTransitioning: boolean
}

export function TabBar({ active, onChange, isTransitioning }: TabBarProps) {
  return (
    <div className="tab-bar">
      {SYSTEMS.map(sys => (
        <div
          key={sys}
          className={`tab${active === sys ? ' active' : ''}`}
          data-system={sys}
          onClick={() => !isTransitioning && onChange(sys)}
        >
          {SYSTEM_LABELS[sys]}
        </div>
      ))}
    </div>
  )
}
