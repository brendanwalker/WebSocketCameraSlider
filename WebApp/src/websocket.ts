import type { ServerMessage, CommandMessage } from './types'

type MessageHandler = (msg: ServerMessage) => void
type ConnectionHandler = (connected: boolean) => void

export class SliderWebSocket {
  private ws: WebSocket | null = null
  private reconnectTimer: ReturnType<typeof setTimeout> | null = null
  private reconnectDelay = 1000
  private messageHandlers: MessageHandler[] = []
  private connectionHandlers: ConnectionHandler[] = []
  private pendingResponses = new Map<string, (data: string) => void>()
  private commandCounter = 0

  onMessage(handler: MessageHandler) {
    this.messageHandlers.push(handler)
  }

  onConnection(handler: ConnectionHandler) {
    this.connectionHandlers.push(handler)
  }

  connect() {
    const url = `ws://${window.location.hostname}/ws`
    console.log(`Connecting to ${url}`)
    this.ws = new WebSocket(url)

    this.ws.onopen = () => {
      console.log('WebSocket connected')
      this.reconnectDelay = 1000
      this.connectionHandlers.forEach(h => h(true))
    }

    this.ws.onclose = () => {
      console.log('WebSocket disconnected')
      this.connectionHandlers.forEach(h => h(false))
      this.scheduleReconnect()
    }

    this.ws.onerror = (err) => {
      console.error('WebSocket error', err)
    }

    this.ws.onmessage = (event) => {
      try {
        const msg = JSON.parse(event.data) as ServerMessage
        if (msg.type === 'response') {
          const resolver = this.pendingResponses.get(msg.id)
          if (resolver) {
            this.pendingResponses.delete(msg.id)
            resolver(msg.data)
          }
        }
        this.messageHandlers.forEach(h => h(msg))
      } catch (e) {
        console.error('Failed to parse message:', event.data, e)
      }
    }
  }

  sendCommand(cmd: string): Promise<string> {
    return new Promise((resolve) => {
      const id = String(++this.commandCounter)
      const msg: CommandMessage = { cmd, id }
      this.pendingResponses.set(id, resolve)
      this.ws?.send(JSON.stringify(msg))

      // Timeout after 5 seconds
      setTimeout(() => {
        if (this.pendingResponses.has(id)) {
          this.pendingResponses.delete(id)
          resolve('')
        }
      }, 5000)
    })
  }

  sendCommandNoReply(cmd: string) {
    const msg: CommandMessage = { cmd }
    this.ws?.send(JSON.stringify(msg))
  }

  get isConnected(): boolean {
    return this.ws?.readyState === WebSocket.OPEN
  }

  private scheduleReconnect() {
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer)
    this.reconnectTimer = setTimeout(() => {
      this.reconnectDelay = Math.min(this.reconnectDelay * 2, 10000)
      this.connect()
    }, this.reconnectDelay)
  }
}
