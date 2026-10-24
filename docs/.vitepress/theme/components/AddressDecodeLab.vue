<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, useId, watch } from 'vue'
import { Check, CircleHelp, MapPin, Pause, Play, RotateCcw, StepForward } from '@lucide/vue'
import { ADDRESS_EXAMPLES, MEMORY_REGIONS, REGISTER_TARGETS, decodeAddress, formatHex32, parseUint32Hex } from '../mcu-registers.mjs'

const id = useId()
const addressText = ref('0x40020018')
const stage = ref(0)
const playing = ref(false)
const reducedMotion = ref(false)
const stages = ['区域', '外设基址', '寄存器偏移', '目标']
let timer: ReturnType<typeof setInterval> | null = null
let motionQuery: MediaQueryList | null = null

const parsed = computed(() => parseUint32Hex(addressText.value))
const decoded = computed(() => parsed.value.ok ? decodeAddress(parsed.value.value) : null)
const selectedExample = computed(() => parsed.value.ok && ADDRESS_EXAMPLES.some(item => item.address === parsed.value.value) ? parsed.value.hex : '')
const blocked = computed(() => !parsed.value.ok || decoded.value?.status === 'unaligned')
const outcome = computed(() => blocked.value ? 'blocked' : stage.value < 4 ? 'pending' : decoded.value?.modeled ? 'decoded' : 'unmodeled')
const matches = computed(() => [Boolean(decoded.value?.region), decoded.value?.base != null, decoded.value?.offset != null, decoded.value?.modeled === true])
const regionText = computed(() => stage.value < 1 ? '等待区域译码' : decoded.value?.region?.name ?? '未建模区域')
const baseText = computed(() => {
  if (stage.value < 2) return '等待基址识别'
  if (decoded.value?.base == null) return '未建模'
  return (decoded.value.peripheral ?? '非外设；区域起点') + ' · ' + formatHex32(decoded.value.base)
})
const offsetText = computed(() => stage.value < 3 ? '等待偏移计算' : decoded.value?.offset == null ? '未建模' : formatHex32(decoded.value.offset))
const targetText = computed(() => stage.value < 4 ? '尚未确认目标' : decoded.value?.target ?? '未知 / 保留 / 未建模')
const result = computed(() => {
  if (!parsed.value.ok) return parsed.value.message
  if (decoded.value?.status === 'unaligned') return '未执行：本模型的 32 位寄存器访问要求 4 字节对齐，地址低两位须为 0。此处不推断真实芯片的异常行为。'
  if (stage.value === 0) return '尚未译码。地址属于同一个 32 位地址空间，不同地址可以指向不同硬件目标。'
  if (stage.value === 1) return decoded.value?.region ? '已识别选定区域；区域命中不等于每个地址都可访问。' : '未命中本模型选定区域；不据此判断真实芯片的可访问性。'
  if (stage.value === 2) return decoded.value?.peripheral ? '已识别 ' + decoded.value.peripheral + ' 基址；继续确定寄存器偏移。' : '存储区域不需要外设基址；未建模区域仍没有可确认的目标。'
  if (stage.value === 3) return decoded.value?.offset != null ? '偏移 = 地址 - 基址。存储区显示区内偏移，外设显示寄存器偏移。' : '未建模基址，不能推算寄存器偏移。'
  if (!decoded.value?.modeled) return '未确认目标：本模型没有该地址的访问语义；不执行访问、不伪造读取值，也不承诺地址可访问。'
  return '译码目标：' + decoded.value.target + '。这里只确定路由，没有读取存储内容或执行寄存器读写。'
})
const codeText = computed(() => {
  if (!parsed.value.ok) return '/* 地址无效：未提交 */'
  const first = 'uint32_t address = ' + parsed.value.hex + 'u;'
  if (stage.value < 3 || decoded.value?.offset == null) return first + '\n/* 地址译码，不读取内容 */'
  return first + '\n/* ' + parsed.value.hex + ' =\n   ' + formatHex32(decoded.value.base) + ' + ' + formatHex32(decoded.value.offset) + ' */'
})
const playLabel = computed(() => playing.value ? '暂停译码' : reducedMotion.value ? '单步播放译码（减少动态效果）' : '播放译码')

function stop() {
  if (timer !== null) clearInterval(timer)
  timer = null
  playing.value = false
}
function reset() { stop(); stage.value = 0 }
function advance() {
  if (blocked.value || stage.value >= 4) { stop(); return }
  stage.value += 1
  if (stage.value === 4) stop()
}
function stepOnce() { stop(); advance() }
function togglePlay() {
  if (playing.value) { stop(); return }
  if (blocked.value || stage.value === 4) return
  if (reducedMotion.value) { advance(); return }
  playing.value = true
  timer = setInterval(advance, 850)
}
function chooseExample(event: Event) {
  const value = (event.target as HTMLSelectElement).value
  if (value) addressText.value = value
}
function updateMotion() {
  reducedMotion.value = motionQuery?.matches ?? false
  if (reducedMotion.value) stop()
}
watch(addressText, reset, { flush: 'sync' })
onMounted(() => {
  if (typeof window.matchMedia !== 'function') return
  motionQuery = window.matchMedia('(prefers-reduced-motion: reduce)')
  updateMotion()
  motionQuery.addEventListener('change', updateMotion)
})
onUnmounted(() => { stop(); motionQuery?.removeEventListener('change', updateMotion) })
</script>

<template>
  <section class="address-lab" data-testid="address-decode-lab" :data-stage="stage" :data-playing="playing" aria-label="地址译码实验">
    <header class="lab-heading">
      <MapPin :size="20" aria-hidden="true" />
      <h3>地址先译码，再找到目标</h3>
      <span class="model-label">STM32F407xG</span>
    </header>
    <p class="intro">Flash、SRAM 和外设寄存器共享地址空间；地址并不全部对应 RAM。</p>

    <div class="request-fields">
      <label :for="id + '-example'">示例地址
        <select :id="id + '-example'" :value="selectedExample" data-testid="address-example" @change="chooseExample">
          <option value="" disabled>自定义地址</option>
          <option v-for="item in ADDRESS_EXAMPLES" :key="item.name" :value="formatHex32(item.address)">{{ item.name }}</option>
        </select>
      </label>
      <label :for="id + '-input'">32 位十六进制地址
        <input :id="id + '-input'" v-model="addressText" type="text" autocomplete="off" autocapitalize="off" :spellcheck="false" :aria-invalid="blocked" :aria-describedby="id + '-result'" data-testid="address-input" />
      </label>
    </div>

    <div class="toolbar" role="group" aria-label="地址译码播放控制">
      <button class="icon-button primary" type="button" :title="playLabel" :aria-label="playLabel" :disabled="!playing && (blocked || stage === 4)" data-testid="address-play" @click="togglePlay"><Pause v-if="playing" :size="19" aria-hidden="true" /><Play v-else :size="19" aria-hidden="true" /></button>
      <button class="icon-button" type="button" title="下一步译码" aria-label="下一步译码" :disabled="blocked || stage === 4" data-testid="address-step" @click="stepOnce"><StepForward :size="19" aria-hidden="true" /></button>
      <button class="icon-button" type="button" title="重置译码进度" aria-label="重置译码进度" data-testid="address-reset" @click="reset"><RotateCcw :size="19" aria-hidden="true" /></button>
      <span class="progress" data-testid="address-stage">{{ stage }} / 4 · {{ playing ? '播放中' : reducedMotion ? '单步模式' : stage === 4 ? '已结束' : '已暂停' }}</span>
    </div>

    <p :id="id + '-result'" class="result" :class="{ warning: blocked || outcome === 'unmodeled' }" role="status" aria-live="polite" data-testid="address-result" :data-outcome="outcome">{{ result }}</p>

    <div class="decode-view" :class="{ decoding: playing && !blocked }">
      <div class="address-map" aria-label="选定地址区域示意">
        <h4>同一个 32 位地址空间</h4>
        <div class="space-boundary">0x00000000</div>
        <div v-for="region in MEMORY_REGIONS" :key="region.id" class="region" :class="{ selected: stage >= 1 && decoded?.region?.id === region.id, comparing: stage === 1 && decoded?.region?.id === region.id }" :data-testid="'address-region-' + region.id">
          <strong>{{ region.name }}</strong>
          <span>{{ formatHex32(region.start) }}</span>
          <span>至 {{ formatHex32(region.end) }}</span>
        </div>
        <div class="space-boundary">0xFFFFFFFF</div>
        <p class="map-note">仅列选定区域，省略 CCM SRAM 等其他映射；不按容量比例。地址空间不等于实际物理存储容量。</p>
      </div>
      <div class="decode-route">
        <h4>本次地址 <output data-testid="address-value">{{ parsed.ok ? parsed.hex : '无效' }}</output></h4>
        <ol class="route" aria-label="地址译码路径">
          <li v-for="(name, index) in stages" :key="name" :class="{ reached: stage > index, active: stage === index + 1, matched: stage > index && matches[index], missed: stage > index && !matches[index] }" :data-match="stage <= index ? 'pending' : matches[index] ? 'matched' : 'unmodeled'" :aria-current="stage === index + 1 ? 'step' : undefined">
            <span class="step-number">{{ index + 1 }}</span>
            <div><span class="route-label">{{ name }}{{ index === 2 && decoded?.status === 'memory' ? ' / 区内偏移' : '' }}<span class="match-mark" :class="{ visible: stage > index }" :title="matches[index] ? '已匹配' : '未建模'"><Check v-if="matches[index]" :size="14" aria-label="已匹配" /><CircleHelp v-else :size="14" aria-label="未建模" /></span></span>
              <output v-if="index === 0" data-testid="address-region">{{ regionText }}</output>
              <output v-else-if="index === 1" data-testid="address-base">{{ baseText }}</output>
              <output v-else-if="index === 2" data-testid="address-offset">{{ offsetText }}</output>
              <output v-else data-testid="address-target">{{ targetText }}</output>
            </div>
          </li>
        </ol>
      </div>
    </div>

    <pre class="address-code" data-testid="address-code" aria-label="当前地址与偏移"><code>{{ codeText }}</code></pre>

    <table class="register-table" aria-label="已建模寄存器地址表">
      <thead><tr><th scope="col">寄存器</th><th scope="col">地址</th><th scope="col">偏移</th></tr></thead>
      <tbody><tr v-for="item in REGISTER_TARGETS" :key="item.name" :class="{ matched: stage === 4 && decoded?.target === item.name }">
        <th scope="row">{{ item.name }}</th><td>{{ formatHex32(item.address) }}</td><td>{{ '0x' + item.offset.toString(16).toUpperCase().padStart(2, '0') }}</td>
      </tr></tbody>
    </table>
    <p class="footnote">教学译码模型，不访问主机真实硬件。未列出的寄存器与未知、保留地址没有访问结论。</p>
  </section>
</template>

<style scoped>
.address-lab { box-sizing: border-box; min-width: 0; margin: 24px 0; padding: 18px; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 8px; background: var(--vp-c-bg, #fff); color: var(--vp-c-text-1, #263332); font-size: 14px; line-height: 1.65; }
.address-lab *, .address-lab *::before, .address-lab *::after { box-sizing: border-box; }
.lab-heading { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
.lab-heading > svg { flex: none; color: var(--vp-c-brand-1, #087f8c); }
.address-lab h3 { margin: 0; font-size: 17px; line-height: 1.5; }
.address-lab h4 { margin: 0 0 12px; font-size: 14px; line-height: 1.6; }
.model-label { margin-left: auto; color: var(--vp-c-text-2, #5c6a69); font-size: 13px; }
.address-lab .intro { margin: 12px 0 18px; font-size: 14px; line-height: 1.7; }
.request-fields { display: grid; grid-template-columns: 1fr 1fr; gap: 14px; }
.request-fields label { display: grid; gap: 6px; min-width: 0; font-weight: 600; }
.request-fields input, .request-fields select { width: 100%; min-width: 0; height: 42px; padding: 6px 10px; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 4px; background: var(--vp-c-bg, #fff); color: inherit; font: inherit; }
.request-fields input { font-family: var(--vp-font-family-mono, monospace); font-weight: 400; font-size: 16px; }
.request-fields input[aria-invalid='true'] { border-color: #b6533e; }
.address-lab :is(button, input, select):focus-visible { outline: 2px solid var(--vp-c-brand-1, #087f8c); outline-offset: 3px; }
.toolbar { display: flex; flex-wrap: wrap; gap: 8px; align-items: center; margin: 16px 0 20px; padding-bottom: 16px; border-bottom: 1px solid var(--vp-c-divider, #dfe6e4); }
.icon-button { display: inline-flex; align-items: center; justify-content: center; flex: none; width: 42px; height: 42px; padding: 0; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 4px; background: var(--vp-c-bg, #fff); color: inherit; cursor: pointer; }
.icon-button.primary { background: var(--vp-c-brand-1, #087f8c); color: var(--vp-c-bg, #fff); border-color: transparent; }
.icon-button:disabled { opacity: .4; cursor: not-allowed; }
.icon-button:not(:disabled):hover { border-color: var(--vp-c-brand-1, #087f8c); }
.progress { margin-left: auto; font-variant-numeric: tabular-nums; color: var(--vp-c-text-2, #5c6a69); white-space: nowrap; }
.decode-view { display: grid; grid-template-columns: minmax(0, .9fr) minmax(0, 1.1fr); gap: 24px; }
.address-map, .decode-route { min-width: 0; }
.space-boundary { color: var(--vp-c-text-2, #5c6a69); font: 13px/1.8 var(--vp-font-family-mono, monospace); }
.region { display: grid; grid-template-columns: 1fr; gap: 0; margin: 8px 0; padding: 8px 12px; border-left: 3px solid var(--vp-c-divider, #dfe6e4); background: var(--vp-c-bg-soft, #f5f7f6); min-height: 82px; }
.region strong { font-size: 14px; }
.region span { font: 13px/1.8 var(--vp-font-family-mono, monospace); }
.region.selected { border-color: var(--vp-c-brand-1, #087f8c); background: var(--vp-c-brand-soft, #e8f4f4); }
.address-lab .map-note, .address-lab .footnote { margin: 10px 0 0; color: var(--vp-c-text-2, #5c6a69); font-size: 13px; line-height: 1.75; overflow-wrap: anywhere; }
.decode-route h4 { display: flex; flex-wrap: wrap; gap: 6px; }
.decode-route h4 output { font: 14px/1.6 var(--vp-font-family-mono, monospace); }
.address-lab .route { list-style: none; margin: 0; padding: 0; }
.route li { position: relative; display: flex; align-items: flex-start; gap: 10px; margin: 0; min-height: 92px; padding: 10px 0; border-top: 1px solid var(--vp-c-divider, #dfe6e4); }
.step-number { display: inline-flex; align-items: center; justify-content: center; width: 26px; height: 26px; flex: none; border: 1px solid var(--vp-c-divider, #dfe6e4); border-radius: 50%; color: var(--vp-c-text-2, #5c6a69); font-variant-numeric: tabular-nums; }
.route .reached .step-number { border-color: var(--vp-c-brand-1, #087f8c); color: var(--vp-c-brand-1, #087f8c); }
.route .active .step-number { background: var(--vp-c-brand-1, #087f8c); color: var(--vp-c-bg, #fff); }
.route li > div { min-width: 0; }
.route-label { display: flex; align-items: center; gap: 6px; color: var(--vp-c-text-2, #5c6a69); font-size: 13px; }
.match-mark { display: inline-flex; flex: none; visibility: hidden; }
.match-mark.visible { visibility: visible; color: var(--vp-c-brand-1, #087f8c); }
.missed .match-mark.visible { color: #b6533e; }
.route li.matched::after { content: ''; position: absolute; top: 0; left: 0; height: 2px; width: 100%; background: var(--vp-c-brand-1, #087f8c); transform-origin: left; }
.decoding .route li.active.matched::after { animation: decode-match .85s ease-in-out infinite; }
.decoding .region.comparing { animation: decode-region .85s ease-in-out infinite; }
@keyframes decode-match { from { transform: scaleX(.1); } to { transform: scaleX(1); } }
@keyframes decode-region { 50% { box-shadow: inset 5px 0 var(--vp-c-brand-1, #087f8c); } }
.route output { display: block; overflow-wrap: anywhere; font-size: 14px; font-weight: 600; }
.address-lab .result { min-height: 76px; margin: 18px 0; padding: 12px 0 12px 12px; border-left: 3px solid var(--vp-c-brand-1, #087f8c); font-size: 14px; line-height: 1.75; overflow-wrap: anywhere; }
.address-lab .result.warning { border-color: #b6533e; }
.address-lab .address-code { white-space: pre-wrap; overflow-wrap: anywhere; min-height: 100px; margin: 16px 0; padding: 12px; background: var(--vp-c-bg-soft, #f5f7f6); font: 13px/1.8 var(--vp-font-family-mono, monospace); }
.address-code code { padding: 0; background: transparent; color: inherit; font: inherit; }
.address-lab .register-table { display: table; width: 100%; table-layout: fixed; border-collapse: collapse; margin: 16px 0; font-size: 13px; }
.register-table th, .register-table td { min-width: 0; padding: 9px 8px; text-align: left; border: 0; border-bottom: 1px solid var(--vp-c-divider, #dfe6e4); overflow-wrap: anywhere; white-space: normal; background: transparent; }
.register-table th:first-child { width: 44%; }
.register-table th:last-child { width: 16%; }
.register-table td { font-family: var(--vp-font-family-mono, monospace); }
.register-table tr { border: 0; background: transparent; }
.register-table tr.matched { background: var(--vp-c-brand-soft, #e8f4f4); }
@media (max-width: 600px) {
  .address-lab { padding: 14px; }
  .request-fields, .decode-view { grid-template-columns: minmax(0, 1fr); }
  .model-label { margin-left: 28px; }
  .decode-view { gap: 18px; }
  .address-map { padding-bottom: 16px; border-bottom: 1px solid var(--vp-c-divider, #dfe6e4); }
  .route li { min-height: 84px; }
  .address-lab .result { min-height: 150px; }
  .address-lab .address-code { min-height: 150px; }
  .register-table th, .register-table td { padding: 8px 3px; }
  .register-table th:first-child { width: 42%; }
  .register-table th:last-child { width: 18%; }
  .register-table td { word-break: break-all; }
}
@media (prefers-reduced-motion: reduce) { .address-lab *, .address-lab *::before, .address-lab *::after { animation: none !important; transition: none !important; } }
</style>
