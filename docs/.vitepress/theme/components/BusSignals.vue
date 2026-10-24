<script setup>
import { computed, ref, useId, watch } from 'vue'
import { Activity } from '@lucide/vue'
import { parallelWord } from '../bus-signals.mjs'

const props = defineProps({
  address: { type: Number, default: null },
  addressActive: Boolean,
  value: { type: Number, default: null },
  active: Boolean,
  playing: Boolean,
  blocked: Boolean,
  direction: { type: String, default: 'write' },
  unmodeled: { type: String, default: '数据位未建模' },
  focusBit: { type: Number, default: 5 },
})
const id = useId()
const channel = ref('data')
const selected = ref(props.focusBit)
watch(() => props.focusBit, value => { selected.value = value })
const isAddress = computed(() => channel.value === 'address')
const name = computed(() => isAddress.value ? 'A' : 'D')
const enabled = computed(() => !props.blocked && (isAddress.value ? props.addressActive : props.active))
const word = computed(() => parallelWord(isAddress.value ? props.address : props.value, enabled.value))
const level = computed(() => word.value.bits[31 - selected.value]?.level ?? null)
const status = computed(() => props.blocked ? '本模型未执行' : word.value.status === 'valid' ? '有效窗口 · 32 位同时有效' : word.value.status === 'unmodeled' ? props.unmodeled : '窗口外 · 电平未指定')
const wordText = computed(() => word.value.value === null ? '????????' : word.value.value.toString(16).toUpperCase().padStart(8, '0'))
const direction = computed(() => isAddress.value ? 'CPU → 目标' : props.direction === 'read' ? '目标 → CPU' : 'CPU → 目标')
</script>

<template>
  <div class="bus-signals" :class="{ running: playing && enabled, valid: word.status === 'valid' }" data-testid="bus-signals" :data-status="word.status" :data-channel="channel" :data-active="enabled" aria-label="并行总线逻辑电平">
    <div class="signal-toolbar">
      <Activity :size="18" aria-hidden="true" />
      <label :for="id + '-channel'" class="signal-sr-only">观察总线</label>
      <select :id="id + '-channel'" v-model="channel" data-testid="signal-channel">
        <option value="data">数据 D[31:0]</option>
        <option value="address">地址 A[31:0]</option>
      </select>
      <span class="signal-direction">{{ direction }}</span>
    </div>
    <div class="signal-summary">
      <output data-testid="signal-word">{{ word.value === null ? wordText : '0x' + wordText }}</output>
      <span data-testid="signal-status">{{ status }}</span>
    </div>
    <div class="signal-bits" :aria-label="name + '[31:0] 并行位值'">
      <button v-for="bit in word.bits" :key="bit.index" type="button" class="signal-bit"
        :class="{ high: bit.level === 1, low: bit.level === 0, selected: bit.index === selected }"
        :aria-pressed="bit.index === selected" :aria-label="name + '[' + bit.index + ']，' + (bit.level === null ? '未指定或未建模' : bit.level === 1 ? '高电平 1' : '低电平 0')"
        :title="name + '[' + bit.index + ']'" :data-bit="bit.index" :data-level="bit.level === null ? 'unknown' : bit.level"
        @click="selected = bit.index"><span>{{ bit.index }}</span><strong>{{ bit.level === null ? '?' : bit.level }}</strong><i aria-hidden="true"></i></button>
    </div>
    <div class="signal-detail">
      <strong>{{ name }}[{{ selected }}]</strong><span>{{ level === null ? '无可确认电平' : level === 1 ? '高电平 · 1' : '低电平 · 0' }}</span>
    </div>
    <div class="signal-window" :data-level="level === null ? 'unknown' : level" data-testid="signal-window" role="img" :aria-label="name + '[' + selected + ']，' + status">
      <div class="outside-window">未指定</div>
      <div class="valid-window">
        <span class="window-label">{{ enabled ? '逻辑有效窗口' : '无有效窗口' }}</span>
        <span class="level-reference high-reference">1</span><span class="level-reference low-reference">0</span>
        <i v-if="level !== null" class="level-line" :class="level === 1 ? 'at-high' : 'at-low'"></i>
        <span v-else class="unknown-level">?</span>
        <i v-if="level !== null" class="window-sweep" aria-hidden="true"></i>
      </div>
      <div class="outside-window">未指定</div>
    </div>
    <p class="signal-boundary">并行逻辑慢放，非逐位串行。教学步骤不是 HCLK 周期；未建模 HREADY 采样，不代表真机测量。</p>
  </div>
</template>

<style scoped>
.bus-signals { --signal-high: var(--vp-c-brand-1, #087f8c); --signal-low: var(--vp-c-text-2, #5c6a69); width: 100%; min-width: 0; height: 430px; padding: 12px 0; display: grid; grid-template-rows: 42px 58px 86px 32px 100px 1fr; color: var(--vp-c-text-1, #263332); font-size: 12px; letter-spacing: 0; }
.bus-signals *, .bus-signals *::before, .bus-signals *::after { box-sizing: border-box; }
.signal-toolbar { display: flex; align-items: center; gap: 8px; min-width: 0; }
.signal-toolbar > svg { flex: none; color: var(--signal-high); }
.signal-toolbar select { min-width: 0; max-width: 100%; height: 34px; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 4px; background: var(--vp-c-bg, #fff); color: inherit; font: inherit; padding: 4px; }
.signal-direction { margin-left: auto; white-space: nowrap; }
.signal-summary { display: flex; flex-direction: column; justify-content: center; gap: 1px; min-width: 0; }
.signal-summary output { font: 600 16px/1.5 var(--vp-font-family-mono, monospace); }
.signal-summary > span { color: var(--signal-low); font-size: 12px; line-height: 1.5; }
.signal-bits { display: grid; grid-template-columns: repeat(16, minmax(0, 1fr)); grid-template-rows: repeat(2, 40px); gap: 4px; }
.signal-bit { position: relative; display: flex; flex-direction: column; justify-content: center; align-items: center; min-width: 0; width: 100%; height: 40px; padding: 1px 0 4px; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 2px; background: var(--vp-c-bg-soft, #f5f7f6); color: var(--signal-low); cursor: pointer; }
.signal-bit > span { font: 10px/13px var(--vp-font-family-mono, monospace); }
.signal-bit > strong { font: 600 14px/17px var(--vp-font-family-mono, monospace); }
.signal-bit > i { position: absolute; bottom: 3px; left: 20%; width: 60%; height: 2px; }
.signal-bit.high { color: var(--signal-high); background: var(--vp-c-brand-soft, #e8f4f4); }
.signal-bit.high > i { bottom: 20px; background: currentColor; opacity: .35; }
.signal-bit.low > i { background: currentColor; }
.signal-bit.selected { outline: 2px solid var(--signal-high); outline-offset: -2px; }
.signal-bit:focus-visible, .signal-toolbar select:focus-visible { outline: 2px solid var(--signal-high); outline-offset: 2px; }
.signal-detail { display: flex; align-items: center; justify-content: space-between; gap: 8px; min-width: 0; }
.signal-detail strong { font-family: var(--vp-font-family-mono, monospace); }
.signal-detail > span { color: var(--signal-low); }
.signal-window { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 2fr) minmax(0, 1fr); min-width: 0; height: 100px; border-block: 1px solid var(--vp-c-divider, #dfe6e4); }
.outside-window { display: flex; align-items: center; justify-content: center; color: var(--signal-low); font-size: 11px; }
.valid-window { position: relative; min-width: 0; border-inline: 1px dashed var(--vp-c-divider, #dfe6e4); background: var(--vp-c-bg-soft, #f5f7f6); overflow: hidden; }
.valid .valid-window { background: var(--vp-c-brand-soft, #e8f4f4); }
.window-label { position: absolute; top: 4px; width: 100%; text-align: center; font-size: 10px; white-space: nowrap; }
.level-reference { position: absolute; left: 4px; color: var(--signal-low); font: 10px/12px var(--vp-font-family-mono, monospace); }
.high-reference { top: 28px; }.low-reference { top: 72px; }
.level-line { position: absolute; left: 20px; right: 8px; height: 3px; background: var(--signal-high); }
.at-high { top: 33px; }.at-low { top: 77px; background: var(--signal-low); }
.unknown-level { position: absolute; inset: 32px 0 auto; text-align: center; color: var(--signal-low); font: 24px/36px var(--vp-font-family-mono, monospace); }
.window-sweep { position: absolute; inset: 24px auto 8px 0; width: 2px; background: var(--signal-high); opacity: 0; }
.running .window-sweep { animation: parallel-window 1.1s linear infinite; }
.bus-signals .signal-boundary { margin: 8px 0 0; color: var(--signal-low); font-size: 11px; line-height: 1.6; overflow-wrap: anywhere; }
.signal-sr-only { position: absolute; width: 1px; height: 1px; padding: 0; margin: -1px; overflow: hidden; clip-path: inset(50%); white-space: nowrap; }
@keyframes parallel-window { 0% { left: 0; opacity: .25; } 50% { opacity: .7; } 100% { left: 100%; opacity: .25; } }
@media (max-width: 660px) {
  .bus-signals { height: 510px; grid-template-rows: 42px 58px 174px 32px 100px 1fr; }
  .signal-bits { grid-template-columns: repeat(8, minmax(0, 1fr)); grid-template-rows: repeat(4, 40px); }
  .signal-toolbar { gap: 5px; }.signal-direction { font-size: 10px; }
}
@media (prefers-reduced-motion: reduce) { .bus-signals *, .bus-signals *::before, .bus-signals *::after { animation: none !important; transition: none !important; } }
</style>
