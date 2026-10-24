<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, useId, watch } from 'vue'
import { Activity, ArrowDown, ArrowUp, CircuitBoard, Grid2X2, Pause, Play, RotateCcw, StepForward } from '@lucide/vue'
import BusSignals from './BusSignals.vue'
import { GPIOA_BSRR, GPIOA_ODR, bits16, bsrrForPin, evaluateRegisterWrite, formatHex16, formatHex32, parseUint32Hex, splitBsrr } from '../mcu-registers.mjs'

const id = useId()
const pin = ref(5)
const writeText = ref('0x00000020')
const clockEnabled = ref(true)
const outputMode = ref(true)
const odr = ref(0)
const before = ref(0)
const stage = ref(0)
const playing = ref(false)
const commandView = ref('bits')
const reducedMotion = ref(false)
const pinOptions = Array.from({ length: 16 }, (_, index) => index)
const stages = ['CPU 写入', '总线译码', 'BSRR 命令', 'ODR 锁存', '引脚逻辑']
let timer: ReturnType<typeof setInterval> | null = null
let motionQuery: MediaQueryList | null = null

const parsed = computed(() => parseUint32Hex(writeText.value))
const masks = computed(() => parsed.value.ok ? splitBsrr(parsed.value.value) : null)
const evaluation = computed(() => parsed.value.ok ? evaluateRegisterWrite({
  odrBefore: before.value,
  writeValue: parsed.value.value,
  pin: pin.value,
  clockEnabled: clockEnabled.value,
  outputMode: outputMode.value,
}) : null)
const allowed = computed(() => evaluation.value?.allowed === true)
const outcome = computed(() => !allowed.value ? 'blocked' : stage.value === 5 ? 'complete' : 'pending')
const commandRows = computed(() => [
  { key: 'reset', title: '高 16 位 · 复位命令 BR', mask: masks.value?.resetMask ?? 0, offset: 16 },
  { key: 'set', title: '低 16 位 · 置位命令 BS', mask: masks.value?.setMask ?? 0, offset: 0 },
])
const beforeBits = computed(() => bits16(before.value))
const latchBits = computed(() => bits16(odr.value))
const pinState = computed(() => stage.value === 5 && allowed.value ? String(evaluation.value.pinAfter) : '待本次引脚阶段')
const result = computed(() => {
  if (!parsed.value.ok) return '本模型未执行：' + parsed.value.message
  if (!allowed.value) {
    const missing = evaluation.value.missing.map(item => item === 'clock' ? 'GPIOA 时钟未开启' : '所选引脚未设为输出模式')
    return '前置条件不足，本模型不执行：' + missing.join('；') + '。这是教学门槛，不推断关时钟访问的真实总线行为。'
  }
  if (stage.value === 0) return '本次请求尚未执行；当前 ODR 锁存值为 ' + formatHex16(odr.value) + '。'
  if (stage.value === 1) return 'CPU 发出 32 位写事务：地址 ' + formatHex32(GPIOA_BSRR) + '，数据 ' + parsed.value.hex + '。'
  if (stage.value === 2) return '总线译码到 GPIOA 基址 0x40020000 + 偏移 0x18，也就是 BSRR。ODR 尚未改变。'
  if (stage.value === 3) return masks.value.conflictMask ? '同一引脚的 BS 与 BR 同为 1，置位优先；命令位为 0 的引脚保持不变。ODR 尚未改变。' : 'BS 的 1 发出置位命令，BR 的 1 发出复位命令；0 不影响对应位。ODR 尚未改变。'
  if (stage.value === 4) return '命令已作用于 ODR：' + formatHex16(before.value) + ' → ' + formatHex16(odr.value) + '。BSRR 不是保存这份写入值的普通 RAM。'
  return '本次完成：ODR[' + pin.value + '] = ' + evaluation.value.pinAfter + '，PA' + pin.value + ' 的输出逻辑为 ' + evaluation.value.pinAfter + '；这不是引脚电压测量。'
})
const codeText = computed(() => parsed.value.ok ? '/* GPIOA_BSRR: ' + formatHex32(GPIOA_BSRR) + ' */\nGPIOA->BSRR =\n    ' + parsed.value.hex + 'u;' : '/* 写入值无效：未生成写操作 */')
const playLabel = computed(() => playing.value ? '暂停寄存器写入' : reducedMotion.value ? '单步播放寄存器写入（减少动态效果）' : '播放寄存器写入')

function stop() {
  if (timer !== null) clearInterval(timer)
  timer = null
  playing.value = false
}
function invalidate() {
  stop()
  stage.value = 0
  before.value = odr.value
}
function reset() {
  stop()
  stage.value = 0
  odr.value = 0
  before.value = 0
}
function advance() {
  if (!allowed.value || stage.value === 5) { stop(); return }
  if (stage.value === 0) before.value = odr.value
  stage.value += 1
  if (stage.value === 4) odr.value = evaluation.value.after
  if (stage.value === 5) stop()
}
function stepOnce() { stop(); advance() }
function togglePlay() {
  if (playing.value) { stop(); return }
  if (!allowed.value || stage.value === 5) return
  if (reducedMotion.value) { advance(); return }
  playing.value = true
  timer = setInterval(advance, 850)
}
function prepare(command: 'set' | 'reset') {
  invalidate()
  writeText.value = formatHex32(bsrrForPin(pin.value, command))
}
function updateMotion() {
  reducedMotion.value = motionQuery?.matches ?? false
  if (reducedMotion.value) stop()
}
watch([writeText, pin, clockEnabled, outputMode], invalidate, { flush: 'sync' })
onMounted(() => {
  if (typeof window.matchMedia !== 'function') return
  motionQuery = window.matchMedia('(prefers-reduced-motion: reduce)')
  updateMotion()
  motionQuery.addEventListener('change', updateMotion)
})
onUnmounted(() => { stop(); motionQuery?.removeEventListener('change', updateMotion) })
</script>

<template>
  <section class="register-lab" data-testid="register-write-lab" :data-stage="stage" :data-playing="playing" aria-label="GPIOA BSRR 寄存器写入实验">
    <header class="lab-heading"><CircuitBoard :size="20" aria-hidden="true" /><h3>写 BSRR，改变 ODR 锁存</h3><span class="model-label">STM32F407 · GPIOA</span></header>
    <p class="intro">BSRR 接收置位 / 复位命令，ODR 保存输出锁存状态。PA{{ pin }} 是示例引脚，不保证连接板载 LED。</p>

    <div class="request-fields">
      <label :for="id + '-pin'">观察引脚
        <select :id="id + '-pin'" v-model.number="pin" data-testid="register-pin"><option v-for="item in pinOptions" :key="item" :value="item">PA{{ item }}</option></select>
      </label>
      <label :for="id + '-write'">32 位十六进制写入值
        <input :id="id + '-write'" v-model="writeText" type="text" autocomplete="off" autocapitalize="off" :spellcheck="false" :aria-invalid="!parsed.ok" :aria-describedby="id + '-result'" data-testid="register-write-value" />
      </label>
    </div>
    <div class="quick-commands" role="group" aria-label="准备所选引脚的写入命令">
      <button type="button" class="command-button" :title="'准备 PA' + pin + ' 置位命令'" :aria-label="'准备 PA' + pin + ' 置位命令'" data-testid="register-set" @click="prepare('set')"><ArrowUp :size="17" aria-hidden="true" />置位</button>
      <button type="button" class="command-button" :title="'准备 PA' + pin + ' 复位命令'" :aria-label="'准备 PA' + pin + ' 复位命令'" data-testid="register-clear" @click="prepare('reset')"><ArrowDown :size="17" aria-hidden="true" />复位</button>
      <span class="prepared-value" data-testid="register-write-hex">{{ parsed.ok ? parsed.hex : '无效写入值' }}</span>
    </div>
    <fieldset class="preconditions"><legend>本模型的执行前置条件</legend>
      <label><input v-model="clockEnabled" type="checkbox" data-testid="register-clock" />GPIOA 时钟已开启</label>
      <label><input v-model="outputMode" type="checkbox" data-testid="register-output-mode" />PA{{ pin }} 已设为输出模式</label>
    </fieldset>

    <div class="toolbar" role="group" aria-label="寄存器写入播放控制">
      <button class="icon-button primary" type="button" :title="playLabel" :aria-label="playLabel" :disabled="!playing && (!allowed || stage === 5)" data-testid="register-play" @click="togglePlay"><Pause v-if="playing" :size="19" aria-hidden="true" /><Play v-else :size="19" aria-hidden="true" /></button>
      <button class="icon-button" type="button" title="下一步寄存器写入" aria-label="下一步寄存器写入" :disabled="!allowed || stage === 5" data-testid="register-step" @click="stepOnce"><StepForward :size="19" aria-hidden="true" /></button>
      <button class="icon-button" type="button" title="重置进度与 ODR 锁存值" aria-label="重置进度与 ODR 锁存值" data-testid="register-reset" @click="reset"><RotateCcw :size="19" aria-hidden="true" /></button>
      <span class="progress" data-testid="register-stage">{{ stage }} / 5 · {{ playing ? '播放中' : reducedMotion ? '单步模式' : stage === 5 ? '已完成' : '已暂停' }}</span>
    </div>
    <ol class="write-route" aria-label="CPU 到引脚的执行路径">
      <li v-for="(name, index) in stages" :key="name" :class="{ reached: stage > index, active: stage === index + 1 }" :aria-current="stage === index + 1 ? 'step' : undefined"><span class="step-number">{{ index + 1 }}</span><strong>{{ name }}</strong></li>
    </ol>
    <p :id="id + '-result'" class="result" :class="{ warning: !allowed }" role="status" aria-live="polite" data-testid="register-result" :data-outcome="outcome">{{ result }}</p>

    <div class="command-views" role="group" aria-label="BSRR 观察视图">
      <button type="button" :aria-pressed="commandView === 'bits'" data-testid="register-view-bits" @click="commandView = 'bits'"><Grid2X2 :size="16" aria-hidden="true" />命令位图</button>
      <button type="button" :aria-pressed="commandView === 'signals'" data-testid="register-view-signals" @click="commandView = 'signals'"><Activity :size="16" aria-hidden="true" />并行信号</button>
    </div>
    <div class="command-visual">
    <div v-show="commandView === 'bits'" class="command-map" :class="{ executing: allowed && stage === 3, flowing: playing && allowed && stage === 3 }">
      <h4>本次 BSRR 命令 <span>{{ stage >= 3 ? '已译码' : '待发送' }}</span></h4>
      <p class="register-address">地址 <code>{{ formatHex32(GPIOA_BSRR) }}</code> · 这是写命令，不是 BSRR 存储快照</p>
      <div v-for="row in commandRows" :key="row.key" class="command-row" :class="row.key">
        <div class="bit-heading"><span>{{ row.title }}</span><output :data-testid="'register-' + row.key + '-mask'">{{ masks ? formatHex16(row.mask) : '无效' }}</output></div>
        <div class="bit-grid" :aria-label="row.title" :data-testid="'register-' + row.key + '-bits'">
          <div v-for="bit in bits16(row.mask)" :key="bit.index" class="bit" :class="{ on: masks && bit.on, selected: bit.index === pin, conflict: masks && ((masks.conflictMask >>> bit.index) & 1) }" :data-testid="'register-' + row.key + '-bit-' + bit.index" :data-on="masks ? Number(bit.on) : 'invalid'" :aria-label="'bit ' + (bit.index + row.offset) + '，PA' + bit.index + '，' + (masks ? Number(bit.on) : '无效')">
            <span class="bit-index">{{ bit.index + row.offset }}</span><strong>{{ masks ? Number(bit.on) : '?' }}</strong>
          </div>
        </div>
      </div>
      <p class="mask-rule">1 发出命令，0 不影响；同位 BS 与 BR 都为 1 时，置位优先。</p>
    </div>

    <BusSignals v-show="commandView === 'signals'" :address="GPIOA_BSRR" :address-active="allowed && (stage === 1 || stage === 2)"
      :value="parsed.ok ? parsed.value : null" :active="allowed && stage === 3" :blocked="!allowed" :focus-bit="pin"
      :playing="playing && commandView === 'signals'" />
    </div>
    <div class="latch-view" :class="{ executing: stage === 4 }">
      <h4>ODR 输出锁存 <code>{{ formatHex32(GPIOA_ODR) }}</code></h4>
      <div class="latch-values"><div><span>本次写入前</span><output data-testid="register-odr-before">{{ formatHex16(before) }}</output></div><div><span>本次写入后</span><output data-testid="register-odr-after">{{ stage >= 4 ? formatHex16(odr) : '待写入' }}</output></div><div><span>当前锁存</span><output data-testid="register-odr-current">{{ formatHex16(odr) }}</output></div></div>
      <div class="bit-heading"><span>写入前 · bit 15 至 0</span></div>
      <div class="bit-grid before-bits" aria-label="写入前 ODR 位图"><div v-for="bit in beforeBits" :key="bit.index" class="bit" :class="{ on: bit.on, selected: bit.index === pin }" :data-testid="'register-before-bit-' + bit.index"><span class="bit-index">{{ bit.index }}</span><strong>{{ Number(bit.on) }}</strong></div></div>
      <div class="bit-heading"><span>当前锁存 · bit 15 至 0</span></div>
      <div class="bit-grid odr-bits" aria-label="当前 ODR 位图"><div v-for="bit in latchBits" :key="bit.index" class="bit" :class="{ on: bit.on, selected: bit.index === pin, changed: stage >= 4 && bit.on !== beforeBits[15 - bit.index].on }" :data-testid="'register-odr-bit-' + bit.index" :data-on="Number(bit.on)"><span class="bit-index">{{ bit.index }}</span><strong>{{ Number(bit.on) }}</strong></div></div>
    </div>

    <div class="pin-state" :class="{ high: stage === 5 && evaluation?.pinAfter === 1 }"><CircuitBoard :size="24" aria-hidden="true" /><div><strong>PA{{ pin }} 输出逻辑</strong><output data-testid="register-pin-state">{{ pinState }}</output></div></div>
    <pre class="write-code" data-testid="register-code" aria-label="本次 BSRR 写入代码"><code>{{ codeText }}</code></pre>
    <p class="footnote">仅为教学模型，不访问主机真实硬件、不代表引脚电压测量。这里的执行门槛不概括所有芯片的关时钟访问行为。</p>
  </section>
</template>

<style scoped>
.register-lab { box-sizing: border-box; min-width: 0; margin: 24px 0; padding: 18px; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 8px; background: var(--vp-c-bg, #fff); color: var(--vp-c-text-1, #263332); font-size: 14px; line-height: 1.65; --reset-color: #ad4e3c; --reset-soft: #fbeee9; }
:global(html.dark) .register-lab { --reset-color: #ffaf99; --reset-soft: #3b2924; }
.register-lab *, .register-lab *::before, .register-lab *::after { box-sizing: border-box; }
.lab-heading { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
.lab-heading > svg { color: var(--vp-c-brand-1, #087f8c); flex: none; }
.register-lab h3 { margin: 0; font-size: 17px; line-height: 1.5; }
.register-lab h4 { margin: 0 0 10px; font-size: 14px; line-height: 1.7; }
.model-label { margin-left: auto; color: var(--vp-c-text-2, #5c6a69); font-size: 13px; }
.register-lab .intro { margin: 12px 0 18px; font-size: 14px; line-height: 1.75; }
.request-fields { display: grid; grid-template-columns: minmax(100px, .65fr) minmax(0, 1.35fr); gap: 14px; }
.request-fields label { display: grid; gap: 6px; min-width: 0; font-weight: 600; }
.request-fields input, .request-fields select { width: 100%; min-width: 0; height: 42px; padding: 6px 10px; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 4px; background: var(--vp-c-bg, #fff); color: inherit; font: inherit; }
.request-fields input { font-family: var(--vp-font-family-mono, monospace); font-size: 16px; font-weight: 400; }
.request-fields input[aria-invalid='true'] { border-color: var(--reset-color); }
.register-lab :is(button, input, select):focus-visible { outline: 2px solid var(--vp-c-brand-1, #087f8c); outline-offset: 3px; }
.quick-commands { display: flex; align-items: center; flex-wrap: wrap; gap: 8px; margin: 12px 0 18px; }
.command-button { display: inline-flex; align-items: center; justify-content: center; gap: 6px; min-height: 40px; padding: 6px 12px; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 4px; background: var(--vp-c-bg, #fff); color: inherit; font: inherit; cursor: pointer; }
.prepared-value { flex: none; width: 12ch; margin-left: auto; text-align: right; font: 14px/1.65 var(--vp-font-family-mono, monospace); }
.preconditions { display: flex; flex-wrap: wrap; gap: 10px 22px; min-width: 0; margin: 0; padding: 12px 0; border: 0; border-top: 1px solid var(--vp-c-divider, #dfe6e4); }
.preconditions legend { padding: 0 8px 0 0; font-size: 13px; color: var(--vp-c-text-2, #5c6a69); }
.preconditions label { display: flex; align-items: center; gap: 8px; min-width: 0; }
.preconditions input { width: 18px; height: 18px; margin: 0; flex: none; accent-color: var(--vp-c-brand-1, #087f8c); }
.toolbar { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; padding: 12px 0 18px; }
.icon-button { display: inline-flex; align-items: center; justify-content: center; flex: none; width: 42px; height: 42px; padding: 0; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 4px; background: var(--vp-c-bg, #fff); color: inherit; cursor: pointer; }
.icon-button.primary { background: var(--vp-c-brand-1, #087f8c); color: var(--vp-c-bg, #fff); border-color: transparent; }
.icon-button:disabled { opacity: .4; cursor: not-allowed; }
.register-lab button:not(:disabled):hover { border-color: var(--vp-c-brand-1, #087f8c); }
.progress { margin-left: auto; font-variant-numeric: tabular-nums; color: var(--vp-c-text-2, #5c6a69); white-space: nowrap; }
.register-lab .write-route { display: grid; grid-template-columns: repeat(5, minmax(0, 1fr)); gap: 10px; list-style: none; margin: 0; padding: 0; }
.write-route li { display: flex; align-items: center; gap: 6px; min-width: 0; min-height: 66px; margin: 0; padding: 10px 0; border-top: 2px solid var(--vp-c-divider, #dfe6e4); }
.write-route strong { font-size: 13px; line-height: 1.6; overflow-wrap: anywhere; }
.step-number { display: inline-flex; align-items: center; justify-content: center; width: 24px; height: 24px; flex: none; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 50%; font-size: 13px; }
.write-route li.reached { border-color: var(--vp-c-brand-1, #087f8c); }
.write-route li.active .step-number { background: var(--vp-c-brand-1, #087f8c); color: var(--vp-c-bg, #fff); border-color: transparent; }
.register-lab .result { min-height: 90px; margin: 12px 0 18px; padding: 12px 0 12px 12px; border-left: 3px solid var(--vp-c-brand-1, #087f8c); font-size: 14px; line-height: 1.75; overflow-wrap: anywhere; }
.register-lab .result.warning { border-color: var(--reset-color); }
.command-views { display: flex; gap: 4px; border-bottom: 1px solid var(--vp-c-divider, #dfe6e4); }
.command-views button { display: inline-flex; align-items: center; justify-content: center; gap: 6px; min-height: 36px; padding: 6px 10px; border: 0; border-bottom: 2px solid transparent; color: var(--vp-c-text-2, #5c6a69); font: inherit; font-size: 12px; cursor: pointer; }
.command-views button[aria-pressed="true"] { color: var(--vp-c-brand-1, #087f8c); border-color: var(--vp-c-brand-1, #087f8c); }
.command-visual { height: 440px; min-width: 0; }
.command-map.flowing .bit { animation: command-window .85s ease-in-out infinite; }
@keyframes command-window { 50% { border-bottom-width: 4px; } }
@media (max-width: 660px) { .command-visual { height: 530px; } }
.command-map, .latch-view { border-top: 1px solid var(--vp-c-divider, #dfe6e4); padding: 16px 0; }
.command-map h4, .latch-view h4 { display: flex; align-items: baseline; flex-wrap: wrap; justify-content: space-between; gap: 8px; }
.command-map h4 > span { color: var(--vp-c-text-2, #5c6a69); font-weight: 400; font-size: 13px; }
.register-lab .register-address, .register-lab .mask-rule, .register-lab .footnote { margin: 8px 0 12px; font-size: 13px; line-height: 1.75; color: var(--vp-c-text-2, #5c6a69); overflow-wrap: anywhere; }
.register-address code, .latch-view h4 code { background: none; padding: 0; color: inherit; font: 13px/1.7 var(--vp-font-family-mono, monospace); }
.bit-heading { display: flex; flex-wrap: wrap; align-items: baseline; justify-content: space-between; gap: 8px; margin: 12px 0 6px; font-size: 13px; }
.bit-heading output { font-family: var(--vp-font-family-mono, monospace); }
.bit-grid { display: grid; grid-template-columns: repeat(16, minmax(0, 1fr)); gap: 4px; }
.bit { display: flex; flex-direction: column; align-items: center; justify-content: center; min-width: 0; height: 52px; border: 1px solid var(--vp-c-divider, #dfe6e4); background: var(--vp-c-bg-soft, #f5f7f6); color: var(--vp-c-text-2, #5c6a69); font-variant-numeric: tabular-nums; }
.bit-index { font-size: 12px; line-height: 18px; }
.bit strong { font: 600 16px/22px var(--vp-font-family-mono, monospace); }
.bit.on { background: var(--vp-c-brand-soft, #e8f4f4); color: var(--vp-c-brand-1, #087f8c); }
.reset .bit.on { background: var(--reset-soft); color: var(--reset-color); }
.bit.selected { border-color: var(--vp-c-brand-1, #087f8c); box-shadow: inset 0 -3px var(--vp-c-brand-1, #087f8c); }
.bit.conflict { border-style: dashed; border-color: var(--reset-color); }
.bit.changed { box-shadow: inset 0 0 0 2px var(--vp-c-brand-1, #087f8c); }
.executing { border-top-color: var(--vp-c-brand-1, #087f8c); }
.latch-values { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 10px; margin: 10px 0 18px; }
.latch-values span { display: block; font-size: 13px; color: var(--vp-c-text-2, #5c6a69); }
.latch-values output { display: block; font: 600 16px/1.8 var(--vp-font-family-mono, monospace); min-height: 30px; overflow-wrap: anywhere; }
.pin-state { display: flex; align-items: center; gap: 12px; min-height: 82px; border-top: 1px solid var(--vp-c-divider, #dfe6e4); border-bottom: 1px solid var(--vp-c-divider, #dfe6e4); padding: 12px 0; }
.pin-state > svg { flex: none; color: var(--vp-c-text-2, #5c6a69); }
.pin-state > div { min-width: 0; }
.pin-state output { display: block; min-height: 26px; font-size: 16px; overflow-wrap: anywhere; }
.pin-state.high > svg, .pin-state.high output { color: var(--vp-c-brand-1, #087f8c); }
.register-lab .write-code { white-space: pre-wrap; overflow-wrap: anywhere; min-height: 98px; margin: 16px 0; padding: 12px; background: var(--vp-c-bg-soft, #f5f7f6); font: 13px/1.8 var(--vp-font-family-mono, monospace); }
.write-code code { padding: 0; background: none; color: inherit; font: inherit; }
@media (max-width: 650px) {
  .register-lab { padding: 14px; }
  .model-label { margin-left: 28px; }
  .request-fields { grid-template-columns: minmax(0, 1fr); }
  .bit-grid { grid-template-columns: repeat(8, minmax(0, 1fr)); }
  .register-lab .write-route { grid-template-columns: minmax(0, 1fr); gap: 0; }
  .write-route li { min-height: 44px; border-top: 0; border-left: 2px solid var(--vp-c-divider, #dfe6e4); padding: 6px 10px; }
  .write-route strong { font-size: 14px; }
  .register-lab .result { min-height: 174px; }
  .register-lab .write-code { min-height: 150px; }
  .latch-values { grid-template-columns: minmax(0, 1fr); gap: 6px; }
  .latch-values > div { display: flex; flex-wrap: wrap; align-items: baseline; justify-content: space-between; gap: 8px; min-height: 30px; }
}
@media (prefers-reduced-motion: reduce) { .register-lab *, .register-lab *::before, .register-lab *::after { animation: none !important; transition: none !important; } }
</style>
