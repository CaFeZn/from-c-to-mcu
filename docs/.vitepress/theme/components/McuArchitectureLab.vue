<script setup>
import { computed, onUnmounted, ref } from 'vue'
import { Activity, ArrowDown, ArrowUp, Cpu, Network, Pause, Play, RotateCcw, StepForward } from '@lucide/vue'
import BusSignals from './BusSignals.vue'
import { architectureSignals } from '../bus-signals.mjs'
import {
  BUS_MODES, BUS_PHASES, MCU_TOPOLOGY, advanceBus, busExplanation,
  busMode, createBusState, hexAddress, switchBusMode,
} from '../mcu-architecture.mjs'

const state = ref(createBusState())
const playing = ref(false)
const view = ref('path')
const signals = computed(() => architectureSignals(state.value))
const mode = computed(() => busMode(state.value.mode))
const explanation = computed(() => busExplanation(state.value))
const done = computed(() => state.value.step === 4)
const active = computed(() => state.value.step > 0)
const returning = computed(() => state.value.step >= 3 && mode.value.direction === 'read')
const payload = computed(() => state.value.payload === null ? '未传输'
  : mode.value.id === 'gpio' ? '0x20' : String(state.value.payload))
const route = computed(() => mode.value.id === 'flash'
  ? MCU_TOPOLOGY.instructionPort + ' / ' + MCU_TOPOLOGY.instructionFabric + ' / ' + MCU_TOPOLOGY.flashInterface
  : MCU_TOPOLOGY.dataPort + ' / ' + MCU_TOPOLOGY.dataFabric)
let timer = null

function stopPlayback() {
  if (timer !== null) clearInterval(timer)
  timer = null
  playing.value = false
}
function advance() {
  state.value = advanceBus(state.value)
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
  timer = setInterval(advance, 1100)
}
function selectMode(id) {
  if (id === state.value.mode) return
  stopPlayback()
  state.value = switchBusMode(state.value, id)
}
function reset() {
  stopPlayback()
  state.value = createBusState(state.value.mode)
}
onUnmounted(stopPlayback)
</script>

<template>
  <section class="architecture-lab" data-testid="mcu-bus-lab" :data-step="state.step" :data-playing="playing" aria-label="MCU 总线访问教学模型">
    <header class="arch-heading">
      <Cpu :size="20" aria-hidden="true" />
      <h3>MCU 内部：谁在访问谁</h3>
    </header>
    <p class="arch-subtitle">STM32F407 / 固定访问示例</p>

    <div class="arch-modes" role="group" aria-label="访问类型">
      <button v-for="item in BUS_MODES" :key="item.id" type="button"
        :aria-label="item.label" :title="item.label" :aria-pressed="state.mode === item.id"
        :data-testid="'mcu-mode-' + item.id" @click="selectMode(item.id)">
        <span>{{ item.medium }}</span><span>{{ item.action }}</span>
      </button>
    </div>

    <div class="arch-controls">
      <button type="button" class="arch-icon primary" :title="playing ? '暂停' : '播放'"
        :aria-label="playing ? '暂停' : '播放'" :disabled="done" data-testid="mcu-bus-play" @click="togglePlay">
        <Pause v-if="playing" :size="18" aria-hidden="true" /><Play v-else :size="18" aria-hidden="true" />
      </button>
      <button type="button" class="arch-icon" title="下一步" aria-label="下一步" :disabled="done" data-testid="mcu-bus-next" @click="nextStep"><StepForward :size="18" aria-hidden="true" /></button>
      <button type="button" class="arch-icon" title="重置" aria-label="重置" data-testid="mcu-bus-reset" @click="reset"><RotateCcw :size="18" aria-hidden="true" /></button>
      <span class="arch-counter">{{ state.step }} / 4</span>
      <span class="arch-play-state">{{ playing ? '播放中' : done ? '已完成' : '已暂停' }}</span>
    </div>

    <div class="arch-progress" aria-hidden="true"><i v-for="step in 4" :key="step" :class="{ complete: state.step >= step }"></i></div>
    <div class="arch-phase" aria-live="polite" aria-atomic="true">
      <strong data-testid="mcu-bus-stage">{{ BUS_PHASES[state.step] }}</strong>
      <p>{{ explanation }}</p>
    </div>

    <div class="arch-views" role="group" aria-label="总线观察视图">
      <button type="button" :aria-pressed="view === 'path'" data-testid="mcu-view-path" @click="view = 'path'"><Network :size="16" aria-hidden="true" />数据通路</button>
      <button type="button" :aria-pressed="view === 'signals'" data-testid="mcu-view-signals" @click="view = 'signals'"><Activity :size="16" aria-hidden="true" />并行信号</button>
    </div>
    <div class="arch-visual">
    <div v-show="view === 'path'" class="arch-diagram" role="img" :aria-label="mode.label + '，' + route + '，' + BUS_PHASES[state.step]" :class="{ 'is-playing': playing }">
      <div class="arch-core" :class="{ updated: done && state.mode !== 'gpio' }">
        <Cpu :size="22" aria-hidden="true" />
        <div><strong>{{ MCU_TOPOLOGY.core }}</strong><span>内核寄存器 R0 / PC</span><span class="arch-core-values"><span>PC <code>{{ hexAddress(state.pc) }}</code></span><span>R0 <code data-testid="mcu-r0-live">{{ state.r0 }}</code></span></span></div>
      </div>
      <div class="arch-lanes">
        <div class="arch-lane" :class="{ selected: state.mode === 'flash' && active }">
          <div class="arch-wire" data-testid="mcu-icode-path">
            <ArrowUp v-if="state.mode === 'flash' && returning" :size="22" aria-hidden="true" />
            <ArrowDown v-else :size="22" aria-hidden="true" />
            <strong>{{ MCU_TOPOLOGY.instructionPort }}</strong>
            <span>{{ MCU_TOPOLOGY.instructionFabric }} / {{ MCU_TOPOLOGY.flashInterface }}</span>
            <span class="arch-transfer" :class="{ visible: state.mode === 'flash' && state.step > 0 && state.step < 4, returning }">{{ state.step < 3 ? hexAddress(state.address) : '指令位未建模' }}</span>
          </div>
          <div class="arch-target flash-target" :class="{ chosen: state.mode === 'flash' && state.step >= 2 }">
            <strong>Flash</strong><span>代码 / 只读初值</span>
          </div>
        </div>
        <div class="arch-lane" :class="{ selected: state.mode !== 'flash' && active }">
          <div class="arch-wire" data-testid="mcu-system-path">
            <ArrowUp v-if="state.mode === 'sram' && returning" :size="22" aria-hidden="true" />
            <ArrowDown v-else :size="22" aria-hidden="true" />
            <strong>{{ MCU_TOPOLOGY.dataPort }}</strong>
            <span>{{ MCU_TOPOLOGY.dataFabric }}</span>
            <span class="arch-transfer" :class="{ visible: state.mode !== 'flash' && state.step > 0 && state.step < 4, returning }">{{ state.step < 3 ? hexAddress(state.address) : state.mode === 'gpio' ? '写入 0x20' : '返回 42' }}</span>
          </div>
          <div class="arch-branches">
            <div class="arch-target" :class="{ chosen: state.mode === 'sram' && state.step >= 2 }"><strong>SRAM</strong><span>数据：42</span></div>
            <div class="arch-target gpio-target" :class="{ chosen: state.mode === 'gpio' && state.step >= 2, updated: state.pa5 }">
              <strong>GPIOA</strong><span>{{ MCU_TOPOLOGY.gpioBus }} / 外设</span><span>BSRR → PA5</span>
              <span class="arch-pin"><i :class="{ high: state.pa5 }"></i>{{ state.pa5 ? '高电平' : '低电平' }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>

    <BusSignals v-show="view === 'signals'" v-bind="signals" :playing="playing && view === 'signals'" />
    </div>
    <table class="arch-state" aria-label="总线与寄存器状态">
      <tbody>
        <tr><th scope="row">访问支路</th><td data-testid="mcu-bus-route">{{ route }}</td></tr>
        <tr><th scope="row">总线地址</th><td><code data-testid="mcu-bus-address">{{ hexAddress(state.address) }}</code></td></tr>
        <tr><th scope="row">选中目标</th><td data-testid="mcu-bus-target">{{ state.target || '未选中' }}</td></tr>
        <tr><th scope="row">传输内容</th><td data-testid="mcu-bus-payload">{{ payload }}</td></tr>
        <tr><th scope="row">内核 PC</th><td><code data-testid="mcu-bus-pc">{{ hexAddress(state.pc) }}</code><small>示例取指地址</small></td></tr>
        <tr><th scope="row">内核 R0</th><td data-testid="mcu-bus-r0">{{ state.r0 }}</td></tr>
        <tr><th scope="row">指令状态</th><td data-testid="mcu-bus-instruction">{{ state.instructionReady ? '已取指，未执行' : '未取指' }}</td></tr>
        <tr><th scope="row">PA5 输出</th><td data-testid="mcu-bus-pa5">{{ state.pa5 ? '高电平（1）' : '低电平（0）' }}</td></tr>
      </tbody>
    </table>

    <p class="arch-note">{{ MCU_TOPOLOGY.gpioAssumption }}</p>
    <details class="arch-boundary">
      <summary>路径与模型边界</summary>
      <p>{{ MCU_TOPOLOGY.flashDataNote }}</p>
      <p>图中只保留本例相关连接，未展开仲裁、缓存、桥接与等待周期。取指不等于执行；这不是可运行任意代码的 ISA 模拟器。</p>
    </details>
  </section>
</template>

<style scoped>
.architecture-lab {
  --arch-brand: var(--vp-c-brand-1, #087f8c);
  --arch-text: var(--vp-c-text-1, #263332);
  --arch-muted: var(--vp-c-text-2, #5c6a69);
  --arch-line: var(--vp-c-divider, #dfe6e4);
  --arch-bg: var(--vp-c-bg, #fff);
  --arch-soft: var(--vp-c-bg-soft, #f5f7f6);
  --arch-output: #b74832;
  width: 100%; min-width: 0; margin: 24px 0; padding: 20px;
  border: 1px solid var(--arch-line); border-radius: 8px;
  background: var(--arch-bg); color: var(--arch-text); font-size: 14px; line-height: 1.65;
}
:global(.dark) .architecture-lab { --arch-output: #ffa58d; }
.architecture-lab *, .architecture-lab *::before, .architecture-lab *::after { box-sizing: border-box; }
.architecture-lab { box-sizing: border-box; }
.arch-heading { display: flex; align-items: center; gap: 9px; }
.arch-heading svg { flex: none; color: var(--arch-brand); }
.architecture-lab .arch-heading h3 { margin: 0; font-size: 17px; line-height: 1.5; overflow-wrap: anywhere; }
.architecture-lab .arch-subtitle { margin: 5px 0 16px; color: var(--arch-muted); font-size: 12px; }
.arch-modes { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); border-bottom: 1px solid var(--arch-line); }
.arch-modes button { min-width: 0; min-height: 48px; padding: 8px 4px; border: 0; border-bottom: 3px solid transparent; color: var(--arch-muted); background: transparent; font: inherit; cursor: pointer; }
.arch-modes button span + span { margin-left: 3px; }
.arch-modes button[aria-pressed="true"] { border-bottom-color: var(--arch-brand); color: var(--arch-brand); background: var(--arch-soft); font-weight: 600; }
.arch-controls { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; margin: 16px 0; }
.arch-icon { display: inline-flex; justify-content: center; align-items: center; width: 36px; height: 36px; flex: 0 0 36px; padding: 0; border: 1px solid var(--arch-line); border-radius: 6px; background: var(--arch-bg); color: var(--arch-text); cursor: pointer; }
.arch-icon.primary { background: var(--arch-brand); color: var(--arch-bg); border-color: var(--arch-brand); }
.arch-icon:disabled { opacity: .4; cursor: default; }
.arch-icon:not(:disabled):hover { border-color: var(--arch-brand); }
.arch-counter { margin-left: auto; font-variant-numeric: tabular-nums; white-space: nowrap; }
.arch-play-state { font-size: 12px; color: var(--arch-muted); min-width: 3em; }
.architecture-lab button:focus-visible, .arch-boundary summary:focus-visible { outline: 2px solid var(--arch-brand); outline-offset: 3px; }
.arch-progress { display: grid; grid-template-columns: repeat(4, 1fr); gap: 4px; }
.arch-progress i { height: 3px; background: var(--arch-line); }
.arch-progress i.complete { background: var(--arch-brand); }
.arch-phase { padding: 14px 0 0; }
.arch-phase > strong { font-size: 15px; }
.architecture-lab .arch-phase p { margin: 5px 0 0; min-height: 5em; font-size: 13px; line-height: 1.7; color: var(--arch-muted); overflow-wrap: anywhere; }
.arch-views { display: flex; gap: 4px; border-bottom: 1px solid var(--arch-line); }
.arch-views button { display: inline-flex; align-items: center; justify-content: center; gap: 6px; min-height: 36px; padding: 6px 10px; border: 0; border-bottom: 2px solid transparent; color: var(--arch-muted); font: inherit; font-size: 12px; cursor: pointer; }
.arch-views button[aria-pressed="true"] { color: var(--arch-brand); border-color: var(--arch-brand); }
.arch-visual { height: 450px; min-width: 0; }
.arch-diagram { padding: 18px 0 24px; }
.arch-core { display: flex; align-items: center; justify-content: center; gap: 12px; padding: 12px; border: 2px solid var(--arch-line); min-height: 78px; }
.arch-core strong, .arch-core span { display: block; }
.arch-core span { font-size: 12px; color: var(--arch-muted); }
.arch-core .arch-core-values { display: flex; flex-wrap: wrap; gap: 4px 12px; margin-top: 4px; }
.arch-core-values code { display: inline-block; min-width: 2ch; font-variant-numeric: tabular-nums; }
.arch-lanes { display: grid; grid-template-columns: minmax(0, .9fr) minmax(0, 1.1fr); gap: 24px; padding: 0 12px; }
.arch-lane { min-width: 0; color: var(--arch-muted); }
.arch-lane.selected { color: var(--arch-brand); }
.arch-wire { display: flex; flex-direction: column; align-items: center; justify-content: center; position: relative; min-height: 138px; text-align: center; gap: 2px; }
.arch-wire::before { content: ''; position: absolute; inset: 0 auto 0 50%; width: 2px; background: var(--arch-line); }
.selected .arch-wire::before { background: var(--arch-brand); }
.arch-wire > * { position: relative; background: var(--arch-bg); }
.arch-wire > strong { padding: 1px 5px; font-size: 14px; }
.arch-wire > span { font-size: 12px; padding: 0 5px; }
.arch-transfer { visibility: hidden; min-height: 22px; font-variant-numeric: tabular-nums; }
.arch-transfer.visible { visibility: visible; font-weight: 600; }
.is-playing .arch-transfer.visible { animation: bus-signal 1.1s ease-in-out infinite; }
.is-playing .arch-transfer.returning { animation-direction: reverse; }
.arch-target { position: relative; min-width: 0; min-height: 74px; padding: 10px 5px; border: 1px solid var(--arch-line); color: var(--arch-text); background: var(--arch-bg); text-align: center; overflow-wrap: anywhere; }
.arch-target strong { display: block; font-size: 14px; }
.arch-target > span { display: block; font-size: 12px; color: var(--arch-muted); }
.arch-target.chosen { border: 2px solid var(--arch-brand); padding: 9px 4px; }
.arch-core.updated, .arch-target.updated { border-color: var(--arch-output); }
.arch-branches { position: relative; display: grid; gap: 18px; padding: 10px 0 0 12px; border-left: 2px solid var(--arch-line); }
.arch-branches::before { content: ''; position: absolute; top: 0; left: -2px; width: 50%; height: 2px; background: var(--arch-line); }
.selected .arch-branches { border-left-color: var(--arch-brand); }
.selected .arch-branches::before { background: var(--arch-brand); }
.arch-branches .arch-target::before { content: ''; position: absolute; width: 14px; height: 2px; background: var(--arch-line); top: 25px; left: -14px; }
.arch-branches .arch-target.chosen::before { background: var(--arch-brand); }
.arch-pin { display: inline-flex !important; align-items: center; justify-content: center; gap: 5px; margin-top: 4px; }
.arch-pin i { display: inline-block; width: 9px; height: 9px; border: 1px solid var(--arch-muted); border-radius: 50%; }
.arch-pin i.high { background: var(--arch-output); border-color: var(--arch-output); }
.architecture-lab .arch-state { display: table; table-layout: fixed; width: 100%; margin: 0; border-collapse: collapse; font-size: 13px; overflow: visible; }
.architecture-lab .arch-state tr { background: transparent; border: 0; }
.architecture-lab .arch-state th, .architecture-lab .arch-state td { min-width: 0; height: 48px; padding: 8px; border: 0; border-top: 1px solid var(--arch-line); text-align: left; vertical-align: middle; overflow-wrap: anywhere; white-space: normal; }
.architecture-lab .arch-state th { width: 36%; background: transparent; color: var(--arch-muted); font-weight: 500; }
.arch-state td { font-variant-numeric: tabular-nums; }
.architecture-lab .arch-state code { padding: 0; background: none; color: inherit; font: 13px var(--vp-font-family-mono, monospace); }
.arch-state small { display: block; font-size: 11px; color: var(--arch-muted); }
.architecture-lab .arch-note, .architecture-lab .arch-boundary p { margin: 12px 0 0; font-size: 12px; line-height: 1.75; color: var(--arch-muted); overflow-wrap: anywhere; }
.arch-boundary { border-top: 1px solid var(--arch-line); margin-top: 16px; padding-top: 12px; }
.arch-boundary summary { font-size: 13px; cursor: pointer; }
@keyframes bus-signal { from { transform: translateY(-5px); } to { transform: translateY(5px); } }
@media (max-width: 660px) { .arch-visual { height: 530px; } }
@media (max-width: 520px) {
  .architecture-lab { padding: 14px 12px; }
  .arch-modes button span { display: block; line-height: 1.45; }
  .arch-modes button span + span { margin-left: 0; font-size: 12px; }
  .arch-lanes { gap: 12px; padding: 0; }
  .architecture-lab .arch-phase p { min-height: 6.8em; }
  .arch-play-state { flex-basis: 100%; }
}
@media (prefers-reduced-motion: reduce) { .architecture-lab *, .architecture-lab *::before, .architecture-lab *::after { animation: none !important; transition: none !important; } }
</style>
