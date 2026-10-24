<script setup>
import { computed, onUnmounted, ref } from 'vue'
import { ArrowLeft, ArrowRight, Cpu, FileCode2, MemoryStick, Pause, Play, RotateCcw, StepForward } from '@lucide/vue'
import { FLASH_EXAMPLE, FLASH_STEPS, MCU_TOPOLOGY, advanceFlash, createFlashState, hexAddress } from '../mcu-architecture.mjs'

const state = ref(createFlashState())
const playing = ref(false)
const current = computed(() => FLASH_STEPS[state.value.step])
const done = computed(() => state.value.step === FLASH_STEPS.length - 1)
const flashActive = computed(() => ['flash-core', 'flash-core-ram'].includes(current.value.flow))
const ramActive = computed(() => ['core-ram', 'flash-core-ram', 'ram-core'].includes(current.value.flow))
const ramReading = computed(() => current.value.flow === 'ram-core')
const numberOrUnknown = value => value === null ? '未知' : String(value)
const addressOrPending = value => value === null ? '未载入' : hexAddress(value)
let timer = null

function stopPlayback() {
  if (timer !== null) clearInterval(timer)
  timer = null
  playing.value = false
}
function advance() {
  state.value = advanceFlash(state.value)
  if (done.value) stopPlayback()
}
function nextStep() {
  stopPlayback()
  advance()
}
function togglePlay() {
  if (playing.value) return stopPlayback()
  if (done.value) return
  playing.value = true
  timer = setInterval(advance, 1400)
}
function reset() {
  stopPlayback()
  state.value = createFlashState()
}
onUnmounted(stopPlayback)
</script>

<template>
  <section class="flash-execution" data-testid="flash-execution-lab" :data-step="state.step" :data-playing="playing" aria-label="主 Flash 启动与 RAM 初始化教学轨迹">
    <header class="flash-heading"><FileCode2 :size="20" aria-hidden="true" /><h3>从主 Flash 启动</h3></header>
    <p class="flash-subtitle">复位向量 / 变量初始化 / main</p>

    <div class="flash-controls">
      <button type="button" class="flash-icon primary" :title="playing ? '暂停' : '播放'" :aria-label="playing ? '暂停' : '播放'" :disabled="done" data-testid="flash-play" @click="togglePlay"><Pause v-if="playing" :size="18" aria-hidden="true" /><Play v-else :size="18" aria-hidden="true" /></button>
      <button type="button" class="flash-icon" title="下一步" aria-label="下一步" :disabled="done" data-testid="flash-next" @click="nextStep"><StepForward :size="18" aria-hidden="true" /></button>
      <button type="button" class="flash-icon" title="重置" aria-label="重置" data-testid="flash-reset" @click="reset"><RotateCcw :size="18" aria-hidden="true" /></button>
      <span class="flash-counter">{{ state.step }} / {{ FLASH_STEPS.length - 1 }}</span>
      <span class="flash-play-state">{{ playing ? '播放中' : done ? '已完成' : '已暂停' }}</span>
    </div>
    <ol class="flash-track" aria-label="启动轨迹概览">
      <li :class="{ active: state.step >= 1 && state.step <= 3, complete: state.step > 3 }">硬件复位</li>
      <li :class="{ active: state.step >= 4 && state.step <= 6, complete: state.step > 6 }">软件初始化</li>
      <li :class="{ active: state.step >= 7 }">main</li>
    </ol>
    <div class="flash-phase" aria-live="polite" aria-atomic="true">
      <div><strong data-testid="flash-stage">{{ current.label }}</strong><span>{{ current.owner }}</span></div>
      <p>{{ current.detail }}</p>
    </div>

    <div class="flash-quick" aria-label="Flash 与 RAM 当前值">
      <span>Flash 初值 <strong>{{ state.flashInitial }}</strong></span>
      <span>RAM counter <strong data-testid="flash-counter-live">{{ numberOrUnknown(state.ram.counter) }}</strong></span>
      <span>RAM events <strong>{{ numberOrUnknown(state.ram.events) }}</strong></span>
    </div>

    <div class="flash-alias" :class="{ mapped: state.mapped }" data-testid="flash-mapping">
      <span>复位别名 <code>0x00000000</code></span>
      <ArrowRight :size="18" aria-hidden="true" />
      <span>主 Flash <code>0x08000000</code></span>
      <strong>{{ state.mapped ? '已映射' : '尚未复位' }}</strong>
    </div>

    <div class="flash-flow" :class="{ 'is-playing': playing }" :data-flow="current.flow" data-testid="flash-flow" role="group" aria-label="Flash、CPU 与 SRAM 的数据路径">
      <div class="flash-node flash-storage" :class="{ active: flashActive }">
        <h4><FileCode2 :size="17" aria-hidden="true" />主 Flash</h4>
        <p class="flash-node-label">代码与只读初值</p>
        <dl>
          <div><dt>启动代码</dt><dd><code>{{ hexAddress(FLASH_EXAMPLE.resetEntry) }}</code></dd></div>
          <div><dt>main 代码</dt><dd><code>{{ hexAddress(FLASH_EXAMPLE.mainEntry) }}</code></dd></div>
          <div class="flash-initial"><dt>.data 初值 <code>{{ hexAddress(FLASH_EXAMPLE.dataSource) }}</code></dt><dd><strong data-testid="flash-initial-value">{{ state.flashInitial }}</strong></dd></div>
        </dl>
      </div>
      <div class="flash-link" :class="{ active: flashActive }" aria-label="Flash 到 CPU" data-testid="flash-read-path">
        <ArrowRight :size="26" aria-hidden="true" /><span>{{ flashActive ? '读取' : '待机' }}</span>
      </div>
      <div class="flash-node flash-core" :class="{ active: current.flow !== 'none' && current.flow !== 'map' }">
        <h4><Cpu :size="18" aria-hidden="true" />CPU</h4>
        <p class="flash-node-label">执行与数据搬运</p>
        <dl>
          <div><dt>指令来源</dt><dd data-testid="flash-code-location">{{ state.codeLocation }}</dd></div>
          <div><dt>当前代码</dt><dd data-testid="flash-instruction">{{ state.instruction }}</dd></div>
          <div><dt>示意运算值</dt><dd><strong data-testid="flash-accumulator">{{ state.accumulator === null ? '未载入' : state.accumulator }}</strong></dd></div>
        </dl>
      </div>
      <div class="flash-link" :class="{ active: ramActive }" :aria-label="ramReading ? 'SRAM 到 CPU' : 'CPU 到 SRAM'" data-testid="flash-ram-path">
        <ArrowLeft v-if="ramReading" :size="26" aria-hidden="true" /><ArrowRight v-else :size="26" aria-hidden="true" />
        <span>{{ ramActive ? ramReading ? '读取' : '写入' : '待机' }}</span>
      </div>
      <div class="flash-node flash-ram" :class="{ active: ramActive }">
        <h4><MemoryStick :size="18" aria-hidden="true" />SRAM</h4>
        <p class="flash-node-label">运行时变量 / 可读写</p>
        <dl>
          <div><dt>counter / .data <code>{{ hexAddress(FLASH_EXAMPLE.counterAddress) }}</code></dt><dd><strong data-testid="flash-ram-counter">{{ numberOrUnknown(state.ram.counter) }}</strong></dd></div>
          <div><dt>events / .bss <code>{{ hexAddress(FLASH_EXAMPLE.eventsAddress) }}</code></dt><dd><strong data-testid="flash-ram-events">{{ numberOrUnknown(state.ram.events) }}</strong></dd></div>
        </dl>
      </div>
    </div>

    <div class="flash-transfer" aria-label="当前数据或指令传输">
      <span>本步内容</span><strong data-testid="flash-payload">{{ current.payload }}</strong>
    </div>
    <table class="flash-state" aria-label="启动状态与地址">
      <tbody>
        <tr><th scope="row">内核 MSP</th><td><code data-testid="flash-msp">{{ addressOrPending(state.msp) }}</code></td></tr>
        <tr><th scope="row">复位向量值</th><td><code data-testid="flash-reset-vector">{{ addressOrPending(state.resetVector) }}</code></td></tr>
        <tr><th scope="row">Thumb 标志</th><td data-testid="flash-thumb">{{ state.thumb === null ? '未载入' : 'bit 0 = 1' }}</td></tr>
        <tr><th scope="row">复位实际入口</th><td><code data-testid="flash-reset-entry">{{ state.resetVector === null ? '未载入' : hexAddress(FLASH_EXAMPLE.resetEntry) }}</code></td></tr>
        <tr><th scope="row">PC 所在代码</th><td><code data-testid="flash-pc">{{ addressOrPending(state.pc) }}</code><small>入口地址示意，非逐指令 PC</small></td></tr>
        <tr><th scope="row">本步来源</th><td data-testid="flash-source">{{ current.source }}</td></tr>
        <tr><th scope="row">本步目标</th><td data-testid="flash-target">{{ current.target }}</td></tr>
      </tbody>
    </table>

    <p class="flash-note">固定教学轨迹：地址与变量均为示例，并非实际 ELF 的布局。只摘出相关启动动作，不表示所有工具链的完整固定顺序或时钟周期。</p>
    <details class="flash-boundary">
      <summary>代码与 RAM 的边界</summary>
      <p>此例的静态存储期变量为 counter = 7 与零初始化的 events；counter++ 在初始化完成后发生。被复制的是 .data 初值，Flash 代码不会整体搬入 RAM。</p>
      <p>{{ MCU_TOPOLOGY.memoryNote }}</p>
    </details>
  </section>
</template>

<style scoped>
.flash-execution {
  --flash-brand: var(--vp-c-brand-1, #087f8c);
  --flash-text: var(--vp-c-text-1, #263332);
  --flash-muted: var(--vp-c-text-2, #5c6a69);
  --flash-line: var(--vp-c-divider, #dfe6e4);
  --flash-bg: var(--vp-c-bg, #fff);
  --flash-soft: var(--vp-c-bg-soft, #f5f7f6);
  --flash-runtime: #b74832;
  box-sizing: border-box; width: 100%; min-width: 0; margin: 24px 0; padding: 20px;
  border: 1px solid var(--flash-line); border-radius: 8px; background: var(--flash-bg);
  color: var(--flash-text); font-size: 14px; line-height: 1.65;
}
:global(.dark) .flash-execution { --flash-runtime: #ffa58d; }
.flash-execution *, .flash-execution *::before, .flash-execution *::after { box-sizing: border-box; }
.flash-heading { display: flex; align-items: center; gap: 9px; }
.flash-heading svg { flex: none; color: var(--flash-brand); }
.flash-execution .flash-heading h3 { margin: 0; font-size: 17px; line-height: 1.5; overflow-wrap: anywhere; }
.flash-execution .flash-subtitle { margin: 5px 0 16px; color: var(--flash-muted); font-size: 12px; }
.flash-controls { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; margin-bottom: 16px; }
.flash-icon { display: inline-flex; justify-content: center; align-items: center; width: 36px; height: 36px; flex: 0 0 36px; padding: 0; border: 1px solid var(--flash-line); border-radius: 6px; background: var(--flash-bg); color: var(--flash-text); cursor: pointer; }
.flash-icon.primary { background: var(--flash-brand); color: var(--flash-bg); border-color: var(--flash-brand); }
.flash-icon:disabled { opacity: .4; cursor: default; }
.flash-icon:not(:disabled):hover { border-color: var(--flash-brand); }
.flash-counter { margin-left: auto; font-variant-numeric: tabular-nums; white-space: nowrap; }
.flash-play-state { min-width: 3em; font-size: 12px; color: var(--flash-muted); }
.flash-execution button:focus-visible, .flash-boundary summary:focus-visible { outline: 2px solid var(--flash-brand); outline-offset: 3px; }
.flash-execution .flash-track { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 4px; padding: 0; margin: 0; list-style: none; }
.flash-execution .flash-track li { border-top: 3px solid var(--flash-line); padding: 7px 2px 0; margin: 0; color: var(--flash-muted); font-size: 12px; text-align: center; }
.flash-execution .flash-track li.active { border-top-color: var(--flash-brand); color: var(--flash-brand); font-weight: 600; }
.flash-execution .flash-track li.complete { border-top-color: var(--flash-brand); }
.flash-quick { display: grid; grid-template-columns: repeat(auto-fit, minmax(112px, 1fr)); gap: 8px 12px; padding: 12px 0; border-bottom: 1px solid var(--flash-line); font-size: 13px; }
.flash-quick > span { display: flex; align-items: center; justify-content: space-between; gap: 8px; }
.flash-quick strong { min-width: 2.2em; text-align: right; color: var(--flash-runtime); font-variant-numeric: tabular-nums; }
.flash-quick > span:first-child strong { color: var(--flash-brand); }
.flash-phase { margin: 16px 0; }
.flash-phase > div { display: flex; align-items: baseline; flex-wrap: wrap; gap: 12px; }
.flash-phase strong { font-size: 15px; }
.flash-phase > div > span { color: var(--flash-muted); font-size: 12px; }
.flash-execution .flash-phase p { margin: 5px 0 0; min-height: 6.8em; font-size: 13px; line-height: 1.7; color: var(--flash-muted); overflow-wrap: anywhere; }
.flash-alias { display: flex; flex-wrap: wrap; align-items: center; gap: 8px 12px; padding: 12px 0; border-block: 1px solid var(--flash-line); font-size: 12px; color: var(--flash-muted); }
.flash-alias > span { display: flex; flex-direction: column; }
.flash-alias > strong { margin-left: auto; font-size: 12px; font-weight: 500; }
.flash-alias.mapped { color: var(--flash-brand); }
.flash-execution code { padding: 0; background: none; color: inherit; font: 13px var(--vp-font-family-mono, monospace); overflow-wrap: anywhere; }
.flash-flow { display: grid; grid-template-columns: minmax(0, 1fr) 42px minmax(0, 1fr) 42px minmax(0, 1fr); align-items: stretch; margin: 22px 0; }
.flash-node { min-width: 0; padding: 12px 10px; border: 1px solid var(--flash-line); }
.flash-node.active { border-color: var(--flash-brand); box-shadow: inset 0 3px var(--flash-brand); }
.flash-execution .flash-node h4 { display: flex; align-items: center; gap: 6px; margin: 0; font-size: 14px; line-height: 1.5; }
.flash-node h4 svg { flex: none; }
.flash-execution .flash-node-label { margin: 6px 0 12px; font-size: 12px; line-height: 1.6; color: var(--flash-muted); }
.flash-node dl { margin: 0; }
.flash-node dl > div { margin: 0; padding: 8px 0; border-top: 1px solid var(--flash-line); }
.flash-node dt { color: var(--flash-muted); font-size: 12px; }
.flash-node dt > code { display: block; margin-top: 3px; }
.flash-node dd { margin: 2px 0 0; font-variant-numeric: tabular-nums; font-size: 13px; }
.flash-node dd > strong { display: block; min-height: 29px; font-size: 19px; font-weight: 600; line-height: 29px; }
.flash-initial dd > strong { color: var(--flash-brand); }
.flash-ram dd > strong { color: var(--flash-runtime); }
.flash-link { display: flex; flex-direction: column; align-items: center; justify-content: center; gap: 4px; min-width: 0; color: var(--flash-muted); }
.flash-link span { font-size: 12px; }
.flash-link.active { color: var(--flash-brand); }
.is-playing .flash-link.active svg { animation: flash-signal 1.4s ease-in-out infinite; }
.flash-transfer { display: flex; flex-wrap: wrap; gap: 8px 20px; padding: 10px 8px; background: var(--flash-soft); font-size: 13px; }
.flash-transfer > span { color: var(--flash-muted); }
.flash-transfer strong { font-variant-numeric: tabular-nums; overflow-wrap: anywhere; }
.flash-execution .flash-state { display: table; table-layout: fixed; width: 100%; margin: 0; border-collapse: collapse; overflow: visible; font-size: 13px; }
.flash-execution .flash-state tr { background: transparent; border: 0; }
.flash-execution .flash-state th, .flash-execution .flash-state td { height: 52px; min-width: 0; padding: 8px; border: 0; border-bottom: 1px solid var(--flash-line); text-align: left; vertical-align: middle; white-space: normal; overflow-wrap: anywhere; }
.flash-execution .flash-state th { width: 36%; background: transparent; color: var(--flash-muted); font-weight: 500; }
.flash-state small { display: block; color: var(--flash-muted); font-size: 11px; line-height: 1.6; }
.flash-execution .flash-note, .flash-execution .flash-boundary p { margin: 12px 0 0; color: var(--flash-muted); font-size: 12px; line-height: 1.75; overflow-wrap: anywhere; }
.flash-boundary { border-top: 1px solid var(--flash-line); margin-top: 16px; padding-top: 12px; }
.flash-boundary summary { font-size: 13px; cursor: pointer; }
@keyframes flash-signal { 50% { opacity: .45; } }
@media (max-width: 760px) {
  .flash-flow { grid-template-columns: minmax(0, 1fr); }
  .flash-node { padding: 12px; }
  .flash-node dl { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 0 14px; }
  .flash-node dl > div:last-child:nth-child(odd) { grid-column: 1 / -1; }
  .flash-link { flex-direction: row; min-height: 48px; }
  .flash-link svg { transform: rotate(90deg); }
}
@media (max-width: 520px) {
  .flash-execution { padding: 14px 12px; }
  .flash-play-state { flex-basis: 100%; }
  .flash-node dl { grid-template-columns: minmax(0, 1fr); }
  .flash-alias { gap: 6px; }
  .flash-alias > strong { flex-basis: 100%; margin: 2px 0 0; }
  .flash-execution .flash-phase p { min-height: 8.5em; }
}
@media (prefers-reduced-motion: reduce) { .flash-execution *, .flash-execution *::before, .flash-execution *::after { animation: none !important; transition: none !important; } }
</style>
