// Server → Client messages
export interface PositionMessage {
  type: 'position'
  slider: number  // [-1, 1]
  pan: number     // [-1, 1]
  tilt: number    // [-1, 1]
  speed: number   // [0, 1]
  accel: number   // [0, 1]
}

export interface StatusMessage {
  type: 'status'
  value: string
}

export interface ResponseMessage {
  type: 'response'
  id: string
  data: string
}

export type ServerMessage = PositionMessage | StatusMessage | ResponseMessage

// Client → Server commands
export interface CommandMessage {
  cmd: string
  id?: string
}
