import './style.css'
import { SliderWebSocket } from './websocket'
import { initUI } from './ui'

const ws = new SliderWebSocket()
initUI(ws)
ws.connect()
