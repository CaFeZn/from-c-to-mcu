<script setup lang="ts">
import { computed, nextTick, onUnmounted, ref, useId, watch } from 'vue'
import {
  Check,
  Cpu,
  FileCode2,
  Pause,
  Play,
  RotateCcw,
  StepForward,
  Terminal as TerminalIcon,
} from '@lucide/vue'

type LineKind = 'cmd' | 'info' | 'ok' | 'out'

interface TermLine {
  text: string
  kind: LineKind
  testid?: string
}

const DEFAULT_GREETING = 'Hello, MCU!'
const MAX_GREETING = 40

const stages = [
  { name: '保存源码', meta: 'main.c' },
  { name: 'CMake 配置', meta: 'cmake' },
  { name: 'Ninja 构建', meta: 'ninja' },
  { name: '运行', meta: './build/app' },
] as const

const stageBadges = ['未开始', '已保存', '已配置', '已构建', '已运行'] as const

const configureLines: TermLine[] = [
  { text: '$ cmake -S . -B build -G Ninja', kind: 'cmd' },
  { text: '-- The C compiler identification is GNU', kind: 'info' },
  { text: '-- Detecting C compiler ABI info', kind: 'info' },
  { text: '-- Configuring done', kind: 'ok' },
  { text: '-- Generating done', kind: 'ok' },
  { text: '-- Build files have been written to: build', kind: 'info' },
]

const buildLines: TermLine[] = [
  { text: '$ ninja -C build', kind: 'cmd' },
  { text: '[1/2] Building C object CMakeFiles/app.dir/main.c.o', kind: 'info' },
  { text: '[2/2] Linking C executable app', kind: 'info' },
]

const greetingId = useId()
const stage = ref(0)
const greeting = ref(DEFAULT_GREETING)
const savedGreeting = ref('')
const runGreeting = ref('')
const runSeq = ref(0)
const playing = ref(false)
const notice = ref('')
const termEl = ref<HTMLElement | null>(null)

let timer: ReturnType<typeof setInterval> | null = null

const cLiteral = computed(() => greeting.value.replace(/\\/g, '\\\\').replace(/"/g, '\\"'))
const canAdvance = computed(() => greeting.value.trim().length > 0)
const badge = computed(() => stageBadges[stage.value] ?? '')
const stageCounter = computed(() => (stage.value === 0 ? '尚未执行' : '已完成 ' + stage.value + ' / 4 步'))

const statusText = computed(() => {
  switch (stage.value) {
    case 1:
      return 'main.c 已写入磁盘，问候语成为字符串常量；build/ 还没有生成。'
    case 2:
      return 'CMake 已在 build/ 生成 Ninja 构建文件，编译尚未开始。'
    case 3:
      return 'Ninja 已编译并链接出 build/app，可以运行了。'
    case 4:
      return '第 ' + runSeq.value + ' 次运行，退出码 0；终端输出来自本次构建的 main.c。'
    default:
      return 'main.c 尚未保存；问候语会写进 C 字符串，再由 printf 原样打印。'
  }
})

const hint = computed(() => {
  if (notice.value) return notice.value
  if (!canAdvance.value) return '问候语为空时没有可打印的字符串，先输入一句英文。'
  return '输出只属于本次运行；修改问候语会清空旧构建结果和终端。'
})

const emptyText = computed(() => {
  switch (stage.value) {
    case 1:
      return 'main.c 已保存，等待 CMake 配置。'
    case 2:
      return 'CMake 配置完成，等待 Ninja 编译链接。'
    case 3:
      return 'build/app 已生成，等待运行。'
    default:
      return '尚未执行。保存源码后，CMake、Ninja 与程序的输出会依次显示在这里。'
  }
})

const terminalLines = computed<TermLine[]>(() => {
  const lines: TermLine[] = []
  if (stage.value >= 2) lines.push(...configureLines)
  if (stage.value >= 3) lines.push(...buildLines)
  if (stage.value >= 4) {
    lines.push(
      { text: '$ ./build/app', kind: 'cmd' },
      { text: runGreeting.value, kind: 'out', testid: 'run-greeting' },
    )
  }
  return lines
})

function stopPlayback() {
  if (timer !== null) {
    clearInterval(timer)
    timer = null
  }
  playing.value = false
}

async function scrollTerminal(toBottom: boolean) {
  await nextTick()
  const el = termEl.value
  if (!el) return
  el.scrollTop = toBottom ? el.scrollHeight : 0
}

function advance() {
  if (stage.value >= 4) {
    stopPlayback()
    return
  }
  notice.value = ''
  if (stage.value === 0) {
    if (!canAdvance.value) {
      stopPlayback()
      return
    }
    savedGreeting.value = greeting.value
    stage.value = 1
    return
  }
  if (stage.value === 1) {
    stage.value = 2
    return
  }
  if (stage.value === 2) {
    stage.value = 3
    return
  }
  runGreeting.value = savedGreeting.value || greeting.value
  runSeq.value += 1
  stage.value = 4
  stopPlayback()
}

function nextStep() {
  stopPlayback()
  advance()
}

function togglePlay() {
  if (playing.value) {
    stopPlayback()
    return
  }
  if (stage.value >= 4 || !canAdvance.value) return
  playing.value = true
  timer = setInterval(() => {
    advance()
    if (stage.value >= 4) stopPlayback()
  }, 900)
}

function reset() {
  stopPlayback()
  stage.value = 0
  savedGreeting.value = ''
  runGreeting.value = ''
  runSeq.value = 0
  notice.value = '流程已重置，终端已清空；问候语保留在输入框中。'
  void scrollTerminal(false)
}

function invalidate() {
  const hadProgress = stage.value > 0
  stopPlayback()
  stage.value = 0
  savedGreeting.value = ''
  runGreeting.value = ''
  runSeq.value = 0
  if (hadProgress) notice.value = '问候语已修改，旧构建结果和终端输出已清除。'
}

function onGreetingInput(event: Event) {
  const input = event.target as HTMLInputElement
  const next = input.value.replace(/[\r\n]/g, '').slice(0, MAX_GREETING)
  if (input.value !== next) input.value = next
  if (next === greeting.value) return
  greeting.value = next
  invalidate()
}

watch(
  () => terminalLines.value.length,
  () => {
    void scrollTerminal(true)
  },
)

watch(stage, (value, previous) => {
  if (value === 0 && previous !== 0) void scrollTerminal(false)
})

onUnmounted(stopPlayback)
</script>

<template>
  <section class="build-lab" data-testid="build-lab" aria-label="C 源码构建与运行演示">
    <header class="lab-head">
      <div class="lab-title-wrap">
        <Cpu class="lab-ico" :size="17" aria-hidden="true" />
        <h3 class="lab-title">从 C 源码到运行结果</h3>
      </div>
      <span class="lab-chip">CMake + Ninja</span>
    </header>

    <p class="lab-intro">
      保存 <code>main.c</code> 后，CMake 在 <code>build/</code> 生成 Ninja 构建文件，Ninja 编译链接出
      <code>build/app</code>，最后运行它。CMake 负责生成构建系统，真正的编译由 Ninja 调度。
    </p>

    <ol class="pipeline" aria-label="构建阶段">
      <li
        v-for="(item, i) in stages"
        :key="item.name"
        class="step"
        :class="{ 'is-done': stage > i, 'is-active': stage === i }"
        :aria-current="stage === i ? 'step' : undefined"
      >
        <span class="step-dot" aria-hidden="true">
          <Check v-if="stage > i" :size="12" />
          <span v-else>{{ i + 1 }}</span>
        </span>
        <span class="step-name">{{ item.name }}</span>
        <span class="step-meta" :title="item.meta">{{ item.meta }}</span>
      </li>
    </ol>

    <div class="status">
      <div class="status-line">
        <span class="badge" :data-stage="stage" data-testid="build-status-badge">{{ badge }}</span>
        <span class="status-count">{{ stageCounter }}</span>
      </div>
      <p class="status-text" data-testid="build-status">{{ statusText }}</p>
    </div>

    <p class="hint" :class="{ 'is-warn': notice || !canAdvance }" role="status">{{ hint }}</p>

    <div class="controls">
      <button
        class="lab-btn is-primary"
        type="button"
        :aria-label="playing ? '暂停' : '播放'"
        :title="playing ? '暂停（保留当前进度）' : '播放（自动执行后续步骤）'"
        :disabled="!playing && (stage === 4 || !canAdvance)"
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
        title="下一步（执行当前阶段）"
        :disabled="stage === 4 || !canAdvance"
        @click="nextStep"
      >
        <StepForward class="lab-ico" :size="15" aria-hidden="true" />
        <span>下一步</span>
      </button>
      <button
        class="lab-btn"
        type="button"
        aria-label="重置"
        title="重置（清空终端并回到起点）"
        @click="reset"
      >
        <RotateCcw class="lab-ico" :size="15" aria-hidden="true" />
        <span>重置</span>
      </button>
    </div>

    <div class="field">
      <label class="field-label" :for="greetingId">问候语（英文）</label>
      <input
        :id="greetingId"
        class="field-input"
        type="text"
        :value="greeting"
        :maxlength="MAX_GREETING"
        autocomplete="off"
        spellcheck="false"
        placeholder="Hello, MCU!"
        @input="onGreetingInput"
      />
      <p class="field-hint"><code>main.c</code> → <code>build/app</code></p>
    </div>

    <div class="pane">
      <div class="pane-head">
        <FileCode2 class="lab-ico" :size="14" aria-hidden="true" />
        <span class="pane-name">main.c</span>
        <span class="pane-tag">{{ stage > 0 ? '已保存' : '未保存' }}</span>
      </div>
      <pre class="code" tabindex="0" aria-label="main.c 源码"><code><span class="tok-comment">/* 固定示例；问候语来自上方输入 */</span>
<span class="tok-pre">#include</span> &lt;stdio.h&gt;

<span class="tok-key">int</span> <span class="tok-fn">main</span>(<span class="tok-key">void</span>) {
    printf(<span class="tok-str">"%s\n"</span>, <span class="tok-str">"{{ cLiteral }}"</span>);
    <span class="tok-key">return</span> <span class="tok-num">0</span>;
}</code></pre>
    </div>

    <div class="pane">
      <div class="pane-head">
        <span class="dots" aria-hidden="true"><i class="dot is-coral"></i><i class="dot is-amber"></i><i class="dot is-ok"></i></span>
        <TerminalIcon class="lab-ico" :size="14" aria-hidden="true" />
        <span class="pane-name">终端</span>
        <span class="pane-tags">
          <span class="pane-tag">模拟 · build/</span>
          <span v-if="stage === 4" class="pane-tag is-exit">退出码 0</span>
        </span>
      </div>
      <div
        ref="termEl"
        class="term-body"
        role="log"
        tabindex="0"
        aria-label="CMake、Ninja 与运行输出"
        data-testid="build-terminal"
      >
        <p v-if="terminalLines.length === 0" class="term-empty">{{ emptyText }}</p>
        <div
          v-for="(line, i) in terminalLines"
          :key="i"
          class="term-line"
          :class="'is-' + line.kind"
          :data-testid="line.testid"
        >
          {{ line.text }}
        </div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.build-lab {
  --lab-border: var(--vp-c-divider, rgba(60, 60, 67, 0.18));
  --lab-border-strong: var(--vp-c-border, rgba(60, 60, 67, 0.32));
  --lab-pane: var(--vp-c-bg-elv, var(--vp-c-bg, #fff));
  --lab-soft: var(--vp-c-bg-soft, #f6f6f7);
  --lab-text: var(--vp-c-text-1, #2c2c32);
  --lab-muted: var(--vp-c-text-2, #5b5b66);
  --lab-brand: var(--vp-c-brand-1, #3451b2);
  --lab-coral: #c04832;
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

:global(html.dark) .build-lab {
  --lab-coral: #ff9f8a;
  --lab-amber: #e5bd5c;
  --lab-ok: #6fd6a3;
}

.build-lab *,
.build-lab *::before,
.build-lab *::after {
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

.lab-intro code,
.field-hint code {
  padding: 0 3px;
  border-radius: 3px;
  background: var(--lab-pane);
  color: var(--lab-text);
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', monospace;
  font-size: 0.92em;
}

.pipeline {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  gap: 6px;
  margin: 0 0 12px;
  padding: 0;
  list-style: none;
}

.step {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: flex-start;
  gap: 2px;
  min-width: 0;
  padding: 8px 3px;
  border: 1px solid var(--lab-border);
  border-radius: 6px;
  background: var(--lab-pane);
}

.step-dot {
  display: grid;
  place-items: center;
  width: 16px;
  height: 16px;
  border: 1px solid var(--lab-border-strong);
  border-radius: 8px;
  color: var(--lab-muted);
  font-size: 10px;
  line-height: 1;
}

.step-name {
  max-width: 100%;
  color: var(--lab-muted);
  font-size: 11px;
  line-height: 1.4;
  overflow-wrap: normal;
  text-align: center;
  word-break: keep-all;
}

.step-meta {
  max-width: 100%;
  margin-top: auto;
  overflow: hidden;
  color: var(--lab-muted);
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', monospace;
  font-size: 9px;
  line-height: 1.5;
  opacity: 0.85;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.step.is-active {
  border-color: var(--lab-brand);
}

.step.is-active .step-dot {
  border-color: var(--lab-brand);
  background: var(--lab-brand);
  color: var(--vp-c-bg, #fff);
}

.step.is-active .step-name {
  color: var(--lab-brand);
  font-weight: 600;
}

.step.is-done .step-dot {
  border-color: var(--lab-ok);
  background: var(--lab-ok);
  color: var(--vp-c-bg, #fff);
}

.step.is-done .step-name {
  color: var(--lab-text);
}

.status {
  display: flex;
  flex-direction: column;
  gap: 3px;
  min-height: 62px;
}

.status-line {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 7px;
  min-height: 20px;
}

.badge {
  flex: none;
  padding: 1px 6px;
  border: 1px solid var(--lab-border-strong);
  border-radius: 4px;
  color: var(--lab-muted);
  font-size: 10.5px;
  line-height: 1.7;
}

.badge[data-stage='1'] {
  border-color: var(--lab-amber);
  color: var(--lab-amber);
}

.badge[data-stage='2'],
.badge[data-stage='3'] {
  border-color: var(--lab-brand);
  color: var(--lab-brand);
}

.badge[data-stage='4'] {
  border-color: var(--lab-ok);
  color: var(--lab-ok);
}

.status-count {
  color: var(--lab-muted);
  font-size: 11px;
}

.status-text {
  margin: 0;
  color: var(--lab-muted);
  font-size: 12px;
  line-height: 1.55;
  overflow-wrap: anywhere;
}

.hint {
  min-height: 36px;
  margin: 2px 0 10px;
  color: var(--lab-muted);
  font-size: 11.5px;
  line-height: 1.55;
  overflow-wrap: anywhere;
}

.hint.is-warn {
  color: var(--lab-amber);
}

.controls {
  display: flex;
  flex-wrap: wrap;
  gap: 7px;
  margin-bottom: 10px;
}

.lab-btn {
  display: inline-flex;
  align-items: center;
  gap: 5px;
  height: 33px;
  margin: 0;
  padding: 0 10px;
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
.field-input:focus-visible,
.code:focus-visible,
.term-body:focus-visible {
  outline: 2px solid var(--lab-brand);
  outline-offset: 2px;
}

.lab-btn:disabled {
  color: var(--lab-muted);
  cursor: not-allowed;
  opacity: 0.45;
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

.field {
  display: block;
  margin-bottom: 12px;
}

.field-label {
  display: block;
  margin-bottom: 4px;
  color: var(--lab-muted);
  font-size: 11.5px;
  line-height: 1.5;
}

.field-input {
  display: block;
  width: 100%;
  height: 34px;
  margin: 0;
  padding: 0 10px;
  border: 1px solid var(--lab-border-strong);
  border-radius: 6px;
  background: var(--lab-pane);
  color: var(--lab-text);
  font-family: inherit;
  font-size: 13px;
  line-height: 1.4;
  transition: border-color 0.15s ease;
}

.field-input:hover {
  border-color: var(--lab-brand);
}

.field-input::placeholder {
  color: var(--lab-muted);
  opacity: 0.7;
}

.field-hint {
  margin: 4px 0 0;
  color: var(--lab-muted);
  font-size: 10.5px;
  line-height: 1.5;
  overflow-wrap: anywhere;
}

.pane {
  min-width: 0;
  margin-bottom: 10px;
  overflow: hidden;
  border: 1px solid var(--lab-border);
  border-radius: 8px;
  background: var(--lab-pane);
}

.pane:last-child {
  margin-bottom: 0;
}

.pane-head {
  display: flex;
  align-items: center;
  gap: 6px;
  min-height: 32px;
  padding: 4px 9px;
  border-bottom: 1px solid var(--lab-border);
  color: var(--lab-muted);
  font-size: 11px;
}

.pane-head .lab-ico {
  color: var(--lab-muted);
}

.pane-name {
  color: var(--lab-text);
  font-size: 11.5px;
  font-weight: 600;
}

.pane-tags {
  display: inline-flex;
  flex-wrap: wrap;
  gap: 4px;
  margin-left: auto;
  min-width: 0;
}

.pane-tag {
  flex: none;
  padding: 0 5px;
  border: 1px solid var(--lab-border);
  border-radius: 4px;
  color: var(--lab-muted);
  font-size: 10px;
  line-height: 1.7;
  white-space: nowrap;
}

.pane-head > .pane-tag {
  margin-left: auto;
}

.pane-tag.is-exit {
  border-color: var(--lab-coral);
  color: var(--lab-coral);
}

.dots {
  display: inline-flex;
  flex: none;
  gap: 3px;
  margin-right: 2px;
}

.dot {
  width: 6px;
  height: 6px;
  border-radius: 3px;
}

.dot.is-coral {
  background: var(--lab-coral);
}

.dot.is-amber {
  background: var(--lab-amber);
}

.dot.is-ok {
  background: var(--lab-ok);
}

.code {
  max-width: 100%;
  margin: 0;
  padding: 10px 12px;
  overflow-x: hidden;
  background: transparent;
  color: var(--lab-text);
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', monospace;
  font-size: 11.5px;
  line-height: 1.7;
  tab-size: 4;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}

.code code {
  display: block;
  min-width: 0;
  padding: 0;
  border: 0;
  background: transparent;
  color: inherit;
  font-family: inherit;
  font-size: inherit;
  line-height: inherit;
}

.tok-comment {
  color: var(--lab-muted);
}

.tok-pre {
  color: var(--lab-coral);
}

.tok-key {
  color: var(--lab-brand);
}

.tok-fn {
  color: var(--lab-text);
}

.tok-str {
  color: var(--lab-amber);
}

.tok-num {
  color: var(--lab-coral);
}

.term-body {
  height: 196px;
  padding: 9px 12px;
  overflow-x: hidden;
  overflow-y: auto;
  font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, 'Liberation Mono', monospace;
  font-size: 11.5px;
  line-height: 1.65;
  overscroll-behavior: contain;
}

.term-line {
  min-height: 19px;
  color: var(--lab-text);
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}

.term-line.is-cmd {
  color: var(--lab-brand);
}

.term-line.is-info {
  color: var(--lab-muted);
}

.term-line.is-ok {
  color: var(--lab-ok);
}

.term-line.is-out {
  color: var(--lab-text);
}

.term-empty {
  margin: 0;
  color: var(--lab-muted);
  font-size: 11.5px;
  line-height: 1.65;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}

@media (max-width: 420px) {
  .build-lab {
    padding: 12px;
  }

  .step-meta {
    font-size: 8.5px;
  }

  .lab-btn {
    padding: 0 8px;
  }
}

@media (prefers-reduced-motion: reduce) {
  .build-lab *,
  .build-lab *::before,
  .build-lab *::after {
    transition: none !important;
  }
}
</style>
