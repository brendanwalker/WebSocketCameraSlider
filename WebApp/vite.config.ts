import { defineConfig } from 'vite'

// The ESP32's IP address - used when proxying WebSocket in dev mode.
// Override with: ESP32_IP=192.168.x.x npm run dev
const ESP32_IP = process.env.ESP32_IP ?? '192.168.1.100'

export default defineConfig({
  server: {
    proxy: {
      '/ws': {
        target: `ws://${ESP32_IP}`,
        ws: true,
        changeOrigin: true,
      },
    },
  },
  build: {
    outDir: '../ESP32Firmware/data',
    emptyOutDir: true,
  },
})
