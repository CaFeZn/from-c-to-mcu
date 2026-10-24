// Fixed teaching traces, not an ISA, timing model, linker map, or board simulator.
// Physical labels are kept here so the STM32F407 chapter can refine the diagram.
export const MCU_TOPOLOGY = Object.freeze({
  core: 'Cortex-M4',
  instructionPort: 'I-Code',
  instructionFabric: '总线矩阵',
  flashInterface: 'Flash 接口',
  dataPort: 'System',
  dataFabric: '总线矩阵',
  gpioBus: 'AHB1',
  flashDataPort: 'D-Code',
  flashDataNote: 'Flash 中的代码取指走 I-Code；代码区常量等数据读取可走 D-Code。SRAM 与外设的数据访问走 System。',
  gpioAssumption: 'GPIO 示例预设：GPIOA 时钟已开启，PA5 已配置为推挽输出，初始输出低电平。BSRR 不是内核寄存器，也不是 APB 外设。',
  memoryNote: '本例只在主 Flash 执行代码。F407 普通 SRAM 可用于执行代码，但需正确放置代码并满足执行权限；CCM 数据 RAM 不可取指，且不能由 DMA 访问。',
})

export const BUS_MODES = Object.freeze([
  Object.freeze({ id: 'flash', label: 'Flash取指', medium: 'Flash', action: '取指', address: 0x08000400, target: 'Flash 指令存储区', payload: '示例指令', direction: 'read' }),
  Object.freeze({ id: 'sram', label: 'SRAM读数', medium: 'SRAM', action: '读数', address: 0x20000000, target: 'SRAM', payload: 42, direction: 'read' }),
  Object.freeze({ id: 'gpio', label: 'GPIO写入', medium: 'GPIO', action: '写入', address: 0x40020018, target: 'GPIOA_BSRR', payload: 0x20, direction: 'write' }),
])

export const BUS_PHASES = Object.freeze(['未执行', '地址发出', '选中目标', '传输', '状态更新'])

export function hexAddress(value) {
  return value === null ? '未发生' : '0x' + value.toString(16).toUpperCase().padStart(8, '0')
}

export function busMode(mode) {
  const result = BUS_MODES.find(item => item.id === mode)
  if (!result) throw new RangeError('Unknown bus mode: ' + mode)
  return result
}

export function createBusState(mode = 'flash') {
  busMode(mode)
  return {
    mode,
    step: 0,
    address: null,
    target: null,
    payload: null,
    pc: 0x08000400,
    r0: 0,
    instructionReady: false,
    sramValue: 42,
    gpioWrite: null,
    pa5: false,
  }
}

export function switchBusMode(_previous, mode) {
  return createBusState(mode)
}

export function advanceBus(state) {
  const mode = busMode(state.mode)
  if (state.step >= 4) return state
  const next = { ...state, step: state.step + 1 }
  if (next.step === 1) next.address = mode.address
  if (next.step === 2) next.target = mode.target
  if (next.step === 3) next.payload = mode.payload
  if (next.step === 4) {
    if (mode.id === 'flash') next.instructionReady = true
    if (mode.id === 'sram') next.r0 = state.sramValue
    if (mode.id === 'gpio') {
      next.gpioWrite = mode.payload
      next.pa5 = true
    }
  }
  return next
}

export function busExplanation(state) {
  const mode = busMode(state.mode)
  if (state.step === 0) return '尚无访问。内核寄存器与外设状态显示的是本例的起始条件。'
  if (state.step === 1) return mode.id === 'flash'
    ? 'CPU 以 PC 指向的示例地址发起取指请求，经 I-Code 接口和总线矩阵到达 Flash 接口。'
    : 'CPU 发出数据访问地址，经 System 接口进入总线矩阵。'
  if (state.step === 2) return mode.id === 'gpio'
    ? '地址译码选中 AHB1 上的 GPIOA_BSRR，它是外设寄存器，不是 CPU 内的 R0。'
    : '地址译码选中 ' + mode.target + '；此时还没有提交 CPU 状态变化。'
  if (state.step === 3) return mode.id === 'gpio'
    ? 'CPU 向 BSRR 写入 0x20，即低半部的 bit 5；这是一笔写操作，不是读取 BSRR。'
    : mode.id === 'sram'
      ? 'SRAM 把数值 42 返回 CPU；SRAM 原值不因读取而改变。'
      : 'Flash 接口把示例指令内容返回 CPU；这里不解码机器码或推断指令长度。'
  return mode.id === 'gpio'
    ? 'BSRR 的置位动作使 ODR bit 5 变为 1；在本例输出配置下，PA5 输出高电平。'
    : mode.id === 'sram'
      ? '本例将读到的 42 写入内核寄存器 R0；外设与 Flash 状态不变。'
      : 'CPU 已取得指令，尚未演示执行；PC 保留示例取指地址，R0 不变。'
}

export const FLASH_EXAMPLE = Object.freeze({
  aliasBase: 0x00000000,
  flashBase: 0x08000000,
  msp: 0x20020000,
  resetVector: 0x08000201,
  resetEntry: 0x08000200,
  dataSource: 0x08000800,
  counterAddress: 0x20000000,
  eventsAddress: 0x20000004,
  mainEntry: 0x08000400,
  initialCounter: 7,
})

export const FLASH_STEPS = Object.freeze([
  Object.freeze({ id: 'idle', label: '未执行', owner: '初态', flow: 'none', payload: '无传输', source: '未发生', target: '未发生', detail: 'Flash 已保存示例程序与初值；SRAM 在本轨迹开始前的内容未知，不预设为 0。' }),
  Object.freeze({ id: 'reset-map', label: '主 Flash 启动映射', owner: '硬件', flow: 'map', payload: '地址别名', source: '0x00000000', target: '0x08000000', detail: '本例选定从主 Flash 启动。复位后的 0x00000000 映射到主 Flash 起始位置，这不是复制代码。' }),
  Object.freeze({ id: 'load-msp', label: '载入 MSP', owner: '硬件', flow: 'flash-core', payload: '0x20020000', source: '向量表第 0 项', target: '内核 MSP', detail: '硬件从别名 0x00000000（主 Flash 0x08000000）读取初始栈顶，载入 MSP。栈顶地址不代表初始化 SRAM 内容。' }),
  Object.freeze({ id: 'load-reset-vector', label: '载入复位入口', owner: '硬件', flow: 'flash-core', payload: '0x08000201', source: '向量表第 1 项', target: 'PC 与 Thumb 状态', detail: '从别名 0x00000004 读取复位向量 0x08000201；bit 0 = 1 表示 Thumb，实际指令入口为 0x08000200。' }),
  Object.freeze({ id: 'fetch-startup', label: '启动代码取指', owner: '软件', flow: 'flash-core', payload: '启动指令', source: 'Flash 0x08000200', target: 'CPU', detail: 'CPU 从主 Flash 的复位入口执行启动代码。本图省略了 SystemInit 等步骤；核对的 ST GCC 模板先调用 SystemInit，再初始化数据。' }),
  Object.freeze({ id: 'copy-data', label: '复制 .data 初值', owner: '软件', flow: 'flash-core-ram', payload: '7', source: 'Flash 0x08000800', target: 'SRAM 0x20000000', detail: '启动代码由 CPU 读取 Flash 中的初值 7，再写入 SRAM 的 counter；不是 Flash 与 SRAM 自行传输，也不是把全部代码搬到 RAM。' }),
  Object.freeze({ id: 'zero-bss', label: '清零 .bss', owner: '软件', flow: 'core-ram', payload: '0', source: 'CPU 写零', target: 'SRAM 0x20000004', detail: '启动代码将 events 所在的 .bss 清零。此刻 counter 已为 7，events 才从未知变为 0。' }),
  Object.freeze({ id: 'enter-main', label: '进入 main', owner: '软件', flow: 'flash-core', payload: 'main 指令', source: 'Flash 0x08000400', target: 'CPU', detail: '进入示例 main 入口 0x08000400，仍从 Flash 取指；counter 与 events 的运行时存储位于 SRAM。' }),
  Object.freeze({ id: 'load-counter', label: '读取 counter', owner: '软件', flow: 'ram-core', payload: '7', source: 'SRAM 0x20000000', target: 'CPU 运算值', detail: '把 counter++ 展开为数据流：先读取 RAM 中已经初始化的 7。这里的运算值不是指定某个机器寄存器。' }),
  Object.freeze({ id: 'increment', label: 'CPU 加 1', owner: '软件', flow: 'core', payload: '7 + 1 = 8', source: 'CPU 运算值', target: 'CPU 运算值', detail: 'CPU 的示意运算值变为 8；尚未写回，所以此时 RAM counter 仍然是 7。' }),
  Object.freeze({ id: 'store-counter', label: '写回 counter', owner: '软件', flow: 'core-ram', payload: '8', source: 'CPU 运算值', target: 'SRAM 0x20000000', detail: '把 8 写回 RAM counter；events 仍为 0，Flash 中保存的初值始终为 7，程序代码仍在 Flash。' }),
])

export function createFlashState() {
  return {
    step: 0,
    mapped: false,
    msp: null,
    resetVector: null,
    thumb: null,
    pc: null,
    codeLocation: 'Flash',
    instruction: '未取指',
    accumulator: null,
    flashInitial: FLASH_EXAMPLE.initialCounter,
    ram: { counter: null, events: null },
  }
}

export function advanceFlash(state) {
  if (state.step >= FLASH_STEPS.length - 1) return state
  const next = { ...state, step: state.step + 1, ram: { ...state.ram } }
  switch (FLASH_STEPS[next.step].id) {
    case 'reset-map': next.mapped = true; break
    case 'load-msp': next.msp = FLASH_EXAMPLE.msp; break
    case 'load-reset-vector':
      next.resetVector = FLASH_EXAMPLE.resetVector
      next.thumb = Boolean(FLASH_EXAMPLE.resetVector & 1)
      next.pc = FLASH_EXAMPLE.resetEntry
      break
    case 'fetch-startup': next.instruction = '启动代码'; break
    case 'copy-data': next.ram.counter = state.flashInitial; break
    case 'zero-bss': next.ram.events = 0; break
    case 'enter-main':
      next.pc = FLASH_EXAMPLE.mainEntry
      next.instruction = 'main'
      break
    case 'load-counter': next.accumulator = state.ram.counter; break
    case 'increment': next.accumulator = state.accumulator + 1; break
    case 'store-counter': next.ram.counter = state.accumulator; break
  }
  return next
}
