import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import { execFileSync } from 'node:child_process'

function getGitRevision() {
  try {
    return execFileSync('git', ['rev-parse', '--short=8', 'HEAD'], {
      encoding: 'utf8',
    }).trim()
  } catch {
    return process.env.GITHUB_SHA?.slice(0, 8) ?? 'unknown'
  }
}

export default defineConfig({
  plugins: [react()],
  define: {
    __EAR6_GIT_REV__: JSON.stringify(getGitRevision()),
    __EAR6_BUILD_TIME__: JSON.stringify(new Date().toISOString()),
  },
})
