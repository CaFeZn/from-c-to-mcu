<script setup lang="ts">
import { computed, onUnmounted, ref, useId } from 'vue'
import { Binary, Pause, Play, RotateCcw, StepForward } from '@lucide/vue'

type ApplyMode = 'set' | 'clear' | 'toggle'

const MAX = 0xff
const bitOptions = [7, 6, 5, 4, 3, 2, 1, 0]
const targetId = useId()

const value = ref(0)
const target = ref(0)
const playing = ref(false)
const actionNote = ref('')

let timer: ReturnType<typeof setInterval> | null = null

const bits = computed(() =>
  Array.from({ length: 8 }, (_, position) => {
    const index = 7 - position
    const weight = 1 << index
    return {
      index,
      weight,
      on: (value.value & weight) !== 0,
    }
  }),
)

const hex = computed(() => '0x' + value.value.toString(16).toUpperCase().padStart(2, '0'))
const binary = computed(() => value.value.toString(2).padStart(8, '0'))
const binaryHigh = computed(() => binary.value.slice(0, 4))
const binaryLow = computed(() => binary.value.slice(4))

const hint = computed(() => {
  if (playing.value) return '自动播放中：每次加 1，255 之后回到 0。'
  return actionNote.value || '无符号 8 位整数按模 256 回绕：255 再加 1 得到 0。'
})

function stopPlayback() {
  if (timer !== null) {
    clearInterval(timer)
    timer = null
  }
  playing.value = false
}

function togglePlay() {
  if (playing.value) {
    stopPlayback()
    actionNote.value = '已暂停，当前值 ' + value.value + '（' + hex.value + '）。'
    return
  }
  actionNote.value = ''
  playing.value = true
  timer = setInterval(() => {
    value.value = (value.value + 1) & MAX
  }, 600)
}

function stepOnce() {
  stopPlayback()
  const before = value.value
  value.value = (before + 1) & MAX
  actionNote.value =
    before === MAX ? '255 再加 1 回绕为 0。' : '加 1：' + before + ' → ' + value.value + '。'
}

function toggleBit(index: number) {
  stopPlayback()
  target.value = index
  value.value = (value.value ^ (1 << index)) & MAX
  const now = (value.value >> index) & 1
  actionNote.value = 'bit ' + index + ' 翻转为 ' + now + '，目标位同步到 bit ' + index + '。'
}

function applyBit(mode: ApplyMode) {
  stopPlayback()
  const bit = target.value
  const mask = 1 << bit
  if (mode === 'set') value.value = (value.value | mask) & MAX
  else if (mode === 'clear') value.value = value.value & ~mask & MAX
  else value.value = (value.value ^ mask) & MAX
  const label = mode === 'set' ? '置位' : mode === 'clear' ? '清位' : '翻转'
  actionNote.value = 'bit ' + bit + ' 已' + label + '，当前值 ' + value.value + '。'
}

function reset() {
  stopPlayback()
  value.value = 0
  target.value = 0
  actionNote.value = '已重置为 0，8 个位全部清零。'
}

onUnmounted(stopPlayback)
</script>

<template>
  <section class="bits-lab" data-testid="bits-lab" aria-label="uint8_t 位操作演示">
    <header class="lab-head">
      <div class="lab-title-wrap">
        <Binary class="lab-ico" :size="17" aria-hidden="true" />
        <h3 class="lab-title">uint8_t 的 8 个位</h3>
      </div>
      <span class="lab-chip">0 – 255</span>
    </header>

    <p class="lab-intro">
      <code>uint8_t</code> 来自 <code>&lt;stdint.h&gt;</code>，是无符号 8 位整数，取值 0–255。bit 0
      是最低位（权重 1），bit 7 是最高位（权重 128）；无符号运算溢出按模 256 回绕。
    </p>

    <div class="bit-row" role="group" aria-label="8 个位开关">
      <button
        v-for="bit in bits"
        :key="bit.index"
        type="button"
        class="bit"
        :class="{ 'is-on': bit.on, 'is-target': bit.index === target }"
        :aria-label="'bit ' + bit.index"
        :aria-pressed="bit.on"
        :title="'bit ' + bit.index + '（权重 ' + bit.weight + '），点击翻转'"
        :data-testid="'bit-' + bit.index"
        @click="toggleBit(bit.index)"
      >
        <span class="bit-name">bit {{ bit.index }}</span>
        <span class="bit-val">{{ bit.on ? 1 : 0 }}</span>
      </button>
    </div>

    <div class="values">
      <div class="value">
        <span class="value-label">十进制</span>
        <output class="value-num" data-testid="bits-decimal">{{ value }}</output>
      </div>
      <div class="value">
        <span class="value-label">十六进制</span>
        <output class="value-num" data-testid="bits-hex">{{ hex }}</output>
      </div>
      <div class="value">
        <span class="value-label">二进制</span>
        <output
          class="value-num is-bin"
          data-testid="bits-binary"
          :aria-label="'二进制 ' + binary"
        ><span>{{ binaryHigh }}</span><span class="bin-gap">{{ binaryLow }}</span></output>
      </div>
    </div>

    <div class="controls">
      <button
        class="lab-btn is-primary"
        type="button"
        :aria-label="playing ? '暂停' : '播放'"
        :title="playing ? '暂停（保留当前值）' : '播放（自动每次加 1）'"
        @click="togglePlay"
      >
        <Pause v-if="playing" class="lab-ico" :size="15" aria-hidden="true" />
        <Play v-else class="lab-ico" :size="15" aria-hidden="true" />
        <span>{{ playing ? '暂停' : '播放' }}</span>
      </button>
      <button
        class="lab-btn"
        type="button"
        aria-label="下一步"
        title="下一步（值加 1）"
        @click="stepOnce"
      >
        <StepForward class="lab-ico" :size="15" aria-hidden="true" />
        <span>下一步</span>
      </button>
      <button
        class="lab-btn"
        type="button"
        aria-label="重置"
        title="重置（回到 0）"
        @click="reset"
      >
        <RotateCcw class="lab-ico" :size="15" aria-hidden="true" />
        <span>重置</span>
      </button>
    </div>

    <div class="ops">
      <div class="ops-target">
        <label class="ops-label" :for="targetId">操作目标位</label>
        <select :id="targetId" v-model.number="target" class="ops-select">
          <option v-for="index in bitOptions" :key="index" :value="index">
            bit {{ index }} · 权重 {{ 1 << index }}
          </option>
        </select>
      </div>
      <div class="ops-actions" role="group" aria-label="对目标位执行操作">
        <button class="lab-btn" type="button" title="置位（把目标位设为 1）" @click="applyBit('set')">
          置位
        </button>
        <button class="lab-btn" type="button" title="清位（把目标位设为 0）" @click="applyBit('clear')">
          清位
        </button>
        <button class="lab-btn" type="button" title="翻转（目标位取反）" @click="applyBit('toggle')">
          翻转
        </button>
      </div>
    </div>

    <p class="ops-note">置位、清位、翻转只作用于选中的目标位；点击任意位会同时把它设为目标位。</p>

    <p class="hint" :class="{ 'is-playing': playing }" role="status">{{ hint }}</p>
  </section>
</template>

<style scoped>
.bits-lab {
  --lab-border: var(--vp-c-divider, rgba(60, 60, 67, 0.18));
  --lab-border-strong: var(--vp-c-border, rgba(60, 60, 67, 0.32));
  --lab-pane: var(--vp-c-bg-elv, var(--vp-c-bg, #fff));
  --lab-soft: var(--vp-c-bg-soft, #f6f6f7);
  --lab-text: var(--vp-c-text-1, #2c2c32);
  --lab-muted: var(--vp-c-text-2, #5b5b66);
  --lab-brand: var(--vp-c-brand-1, #3451b2);
  --lab-coral: #c04832;
  --lab-coral-soft: #fbeee9;
  --lab-amber: #8a5c00;
  --lab-ok: var(--vp-c-tip-1, #18794e);

  box-sizing: border-box;
  width: 100%;
  min-width: 0;
  margin: 24px 0;
  padding: 16px;
  border: 1px solid var(--lab-border);
  border-radius: 8px;
  background: var(--lab-soft);
  color: var(--lab-text);
  font-size: 13px;
  line-height: 1.55;
}

:global(html.dark) .bits-lab {
  --lab-coral: #ff9f8a;
  --lab-coral-soft: #33201c;
  --lab-amber: #e5bd5c;
  --lab-ok: #6fd6a3;
}

.bits-lab *,
.bits-lab *::before,
.bits-lab *::after {
  box-sizing: border-box;
}

.lab-head {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}

.lab-title-wrap {
  display: flex;
  align-items: center;
  gap: 7px;
  min-width: 0;
}

.lab-title {
  margin: 0;
  font-size: 16px;
  font-weight: 600;
  line-height: 1.35;
  color: var(--lab-text);
}

.lab-ico {
  flex: none;
  color: var(--lab-brand);
}

.lab-chip {
  flex: none;
  padding: 1px 7px;
  border: 1px solid var(--lab-border-strong);
  border-radius: 4px;
  color: var(--lab-muted);
  font-size: 10.5px;
  line-height: 1.7;
  white-space: nowrap;
}

.lab-intro {
  margin: 8px 0 12px;
  color: var(--lab-muted);
  font-size: 12.5px;
  line-height: 1.65;
  overflow-wrap: anywhere;
}

.lab-intro code {
  padding: 0 3px;
  border-radius: 3px;
  background: var(--lab-pane);
  color: var(--lab-text);
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', monospace;
  font-size: 0.92em;
}

.bit-row {
  display: grid;
  grid-template-columns: repeat(8, minmax(0, 1fr));
  gap: 4px;
  margin-top: 10px;
}

.bit {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 1px;
  min-width: 0;
  height: 56px;
  margin: 0;
  padding: 2px 1px;
  border: 1px solid var(--lab-border-strong);
  border-radius: 6px;
  background: var(--lab-pane);
  color: var(--lab-muted);
  font-family: inherit;
  cursor: pointer;
  transition: border-color 0.15s ease, background-color 0.15s ease, color 0.15s ease;
}

.bit:hover {
  border-color: var(--lab-brand);
}

.bit:focus-visible {
  outline: 2px solid var(--lab-brand);
  outline-offset: 2px;
}

.bit.is-on {
  border-color: var(--lab-coral);
  background: var(--lab-coral-soft);
}

.bit.is-target {
  outline: 1px solid var(--lab-amber);
  outline-offset: 1px;
}

.bit-name {
  max-width: 100%;
  overflow: hidden;
  color: var(--lab-muted);
  font-size: 9.5px;
  line-height: 1.4;
  letter-spacing: -0.2px;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.bit-val {
  color: var(--lab-muted);
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', monospace;
  font-size: 17px;
  font-weight: 600;
  line-height: 1.15;
  font-variant-numeric: tabular-nums;
}

.bit.is-on .bit-val {
  color: var(--lab-coral);
}

.values {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 6px;
  margin-top: 10px;
}

.value {
  min-width: 0;
  min-height: 60px;
  padding: 7px 8px;
  border: 1px solid var(--lab-border);
  border-radius: 6px;
  background: var(--lab-pane);
}

.value-label {
  display: block;
  color: var(--lab-muted);
  font-size: 10px;
  line-height: 1.5;
}

.value-num {
  display: block;
  color: var(--lab-text);
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', monospace;
  font-size: 18px;
  line-height: 1.35;
  font-variant-numeric: tabular-nums;
  overflow-wrap: anywhere;
}

.value-num.is-bin {
  padding-top: 3px;
  font-size: 12px;
  letter-spacing: 0.2px;
  white-space: nowrap;
}

.bin-gap {
  margin-left: 4px;
}

.controls {
  display: flex;
  flex-wrap: wrap;
  gap: 7px;
  margin-top: 12px;
}

.lab-btn {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  height: 33px;
  margin: 0;
  padding: 0 9px;
  border: 1px solid var(--lab-border-strong);
  border-radius: 6px;
  background: var(--lab-pane);
  color: var(--lab-text);
  font-family: inherit;
  font-size: 12px;
  line-height: 1;
  white-space: nowrap;
  cursor: pointer;
  transition: border-color 0.15s ease, background-color 0.15s ease, color 0.15s ease;
}

.lab-btn:hover:not(:disabled) {
  border-color: var(--lab-brand);
  color: var(--lab-brand);
}

.lab-btn:focus-visible,
.ops-select:focus-visible {
  outline: 2px solid var(--lab-brand);
  outline-offset: 2px;
}

.lab-btn.is-primary {
  border-color: var(--lab-brand);
  background: var(--lab-brand);
  color: var(--vp-c-bg, #fff);
}

.lab-btn.is-primary:hover:not(:disabled) {
  background: var(--vp-c-brand-2, var(--lab-brand));
  color: var(--vp-c-bg, #fff);
}

.lab-btn.is-primary .lab-ico {
  color: currentColor;
}

.ops {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-end;
  gap: 8px;
  margin-top: 12px;
}

.ops-target {
  flex: 1 1 150px;
  min-width: 0;
}

.ops-label {
  display: block;
  margin-bottom: 4px;
  color: var(--lab-muted);
  font-size: 11.5px;
  line-height: 1.5;
}

.ops-select {
  display: block;
  width: 100%;
  height: 33px;
  margin: 0;
  padding: 0 6px;
  border: 1px solid var(--lab-border-strong);
  border-radius: 6px;
  background: var(--lab-pane);
  color: var(--lab-text);
  font-family: inherit;
  font-size: 12px;
  line-height: 1.4;
  cursor: pointer;
}

.ops-actions {
  display: flex;
  flex: 0 0 auto;
  gap: 6px;
}

.ops-note {
  margin: 6px 0 0;
  color: var(--lab-muted);
  font-size: 10.5px;
  line-height: 1.55;
  overflow-wrap: anywhere;
}

.hint {
  min-height: 36px;
  margin: 4px 0 0;
  color: var(--lab-muted);
  font-size: 11.5px;
  line-height: 1.55;
  overflow-wrap: anywhere;
}

.hint.is-playing {
  color: var(--lab-brand);
}

@media (max-width: 420px) {
  .bits-lab {
    padding: 12px;
  }

  .lab-btn {
    padding: 0 8px;
  }

  .bit-name {
    font-size: 9px;
  }

  .bit-val {
    font-size: 16px;
  }

  .ops-target {
    flex: 1 1 100%;
  }
}

@media (prefers-reduced-motion: reduce) {
  .bits-lab *,
  .bits-lab *::before,
  .bits-lab *::after {
    transition: none !important;
  }
}
</style>
