import { defineConfig, normalizePath } from 'vite'
import react from '@vitejs/plugin-react'
import { viteStaticCopy } from 'vite-plugin-static-copy'
import { execFileSync } from 'node:child_process'
import { resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const uiDir = fileURLToPath(new URL('.', import.meta.url))
const wasmAssets = normalizePath(
  resolve(uiDir, '../../../build-web/web-public/ear6/*'),
)

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
  plugins: [
    react(),
    viteStaticCopy({
      targets: [
        {
          src: wasmAssets,
          dest: 'ear6',
        },
      ],
    }),
  ],
  build: {
    outDir: resolve(uiDir, '../../../build-web/site'),
    emptyOutDir: true,
  },
  define: {
    __EAR6_GIT_REV__: JSON.stringify(getGitRevision()),
    __EAR6_BUILD_TIME__: JSON.stringify(new Date().toISOString()),
  },
})
