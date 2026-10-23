<script setup lang="ts">
import { computed, onMounted, onUnmounted, ref, watch, nextTick } from 'vue'
import { useData } from 'vitepress'
import { Play, Pause, RotateCcw } from '@lucide/vue'
const duty = ref(40), frequency = ref(2), playing = ref(false), elapsed = ref(0)
const canvas = ref<HTMLCanvasElement | null>(null)
const { isDark } = useData()
const period = computed(() => 1000 / frequency.value)
const highTime = computed(() => period.value * duty.value / 100)
const output = computed(() => duty.value > 0 && (duty.value === 100 || (elapsed.value * frequency.value) % 1 < duty.value / 100))
let frame = 0, previous = 0, observer: ResizeObserver | undefined
function draw() {
  const el = canvas.value
  if (!el) return
  const width = el.clientWidth, height = 208, dpr = window.devicePixelRatio || 1
  if (!width) return
  el.width = Math.round(width * dpr); el.height = Math.round(height * dpr)
  const ctx = el.getContext('2d')!
  ctx.scale(dpr, dpr)
  const left = 42, right = width - 18, top = 35, bottom = 153, plot = right - left
  const grid = isDark.value ? '#3c4849' : '#d7e1e0'
  const ink = isDark.value ? '#cad5d4' : '#4c6260'
  ctx.clearRect(0, 0, width, height)
  ctx.strokeStyle = grid; ctx.lineWidth = 1
  ctx.font = '12px system-ui'; ctx.fillStyle = ink
  for (let i = 0; i <= 4; i++) {
    const x = left + plot * i / 4
    ctx.beginPath(); ctx.moveTo(x, 24); ctx.lineTo(x, 164); ctx.stroke()
    ctx.fillText((i / 4).toFixed(2) + 's', x - (i === 4 ? 20 : 8), 186)
  }
  for (const y of [top, bottom]) { ctx.beginPath(); ctx.moveTo(left, y); ctx.lineTo(right, y); ctx.stroke() }
  ctx.fillText('高', 12, top + 4); ctx.fillText('低', 12, bottom + 4)
  ctx.strokeStyle = isDark.value ? '#52c5ae' : '#087f8c'; ctx.lineWidth = 3; ctx.beginPath()
  for (let x = 0; x <= plot; x++) {
    const phase = (x / plot * frequency.value) % 1
    const on = duty.value > 0 && (duty.value === 100 || phase < duty.value / 100)
    const y = on ? top : bottom
    if (!x) ctx.moveTo(left, y); else ctx.lineTo(left + x, y)
  }
  ctx.stroke()
  const cursor = left + (elapsed.value % 1) * plot
  ctx.strokeStyle = '#c66550'; ctx.lineWidth = 2; ctx.beginPath(); ctx.moveTo(cursor, 17); ctx.lineTo(cursor, 165); ctx.stroke()
  ctx.fillStyle = '#c66550'; ctx.beginPath(); ctx.arc(cursor, output.value ? top : bottom, 5, 0, Math.PI * 2); ctx.fill()
}
function tick(now: number) {
  if (!playing.value) return
  if (previous) elapsed.value += Math.min((now - previous) / 1000, 0.05)
  previous = now; draw(); frame = requestAnimationFrame(tick)
}
function toggle() { playing.value = !playing.value; previous = 0; if (playing.value) frame = requestAnimationFrame(tick); else cancelAnimationFrame(frame) }
function reset() { playing.value = false; cancelAnimationFrame(frame); previous = 0; elapsed.value = 0; duty.value = 40; frequency.value = 2; nextTick(draw) }
watch([duty, frequency, isDark], () => nextTick(draw))
onMounted(() => { observer = new ResizeObserver(draw); if (canvas.value) observer.observe(canvas.value); draw() })
onUnmounted(() => { cancelAnimationFrame(frame); observer?.disconnect() })
</script>

<template>
  <section class="pwm-lab teaching-lab" data-testid="pwm-lab" aria-label="PWM 占空比实验">
    <header class="lab-top"><div><span class="lab-kicker">05.3 / PWM</span><h3>PWM：改变高电平的时间</h3></div><span class="model-tag">演示频率</span></header>
    <div class="pwm-readings"><div><span>占空比</span><strong data-testid="pwm-duty">{{ duty }}<small>%</small></strong></div><div><span>周期</span><strong>{{ period.toFixed(0) }}<small>ms</small></strong></div><div><span>高电平时间</span><strong>{{ highTime.toFixed(0) }}<small>ms</small></strong></div><div class="pin-state"><i :class="{ on: output }" aria-hidden="true"></i><span data-testid="pwm-output">{{ output ? '高电平' : '低电平' }}</span></div></div>
    <canvas ref="canvas" role="img" aria-label="PWM 波形与当前时间游标"></canvas>
    <div class="pwm-settings"><label>占空比 <input v-model.number="duty" aria-label="占空比" type="range" min="0" max="100" step="1"></label><label>频率 <input v-model.number="frequency" aria-label="演示频率" type="range" min="1" max="10" step="1"><output>{{ frequency }} Hz</output></label></div>
    <footer class="lab-controls"><button type="button" class="lab-icon primary" :title="playing ? '暂停' : '播放'" :aria-label="playing ? '暂停' : '播放'" @click="toggle"><Pause v-if="playing" :size="18"/><Play v-else :size="18"/></button><button type="button" class="lab-icon" title="重置" aria-label="重置" @click="reset"><RotateCcw :size="18"/></button><span class="lab-status">{{ playing ? '运行中' : '已暂停' }} · 高电平时间 / 周期 = 占空比</span></footer>
  </section>
</template>
<style scoped>
.pwm-readings{display:grid;grid-template-columns:1fr 1fr 1.2fr auto;gap:14px;margin:24px 0 6px;align-items:center}.pwm-readings>div>span{display:block;font-size:12px;color:var(--vp-c-text-2)}.pwm-readings strong{display:block;font-size:27px;line-height:36px;font-variant-numeric:tabular-nums}.pwm-readings small{font-size:12px;font-weight:500;margin-left:4px}canvas{display:block;width:100%;height:208px}.pin-state{display:flex;gap:7px;align-items:center;min-width:70px}.pin-state i{width:13px;height:13px;border-radius:50%;background:var(--vp-c-divider);border:2px solid var(--vp-c-text-3)}.pin-state i.on{background:#ef7053;border-color:#c45135}.pwm-settings{display:grid;grid-template-columns:1fr 1fr;gap:24px;margin:15px 0 24px}.pwm-settings label{display:flex;align-items:center;gap:10px;font-size:13px;white-space:nowrap}.pwm-settings input{min-width:0;width:100%;accent-color:var(--vp-c-brand-1)}.pwm-settings output{min-width:38px;font-variant-numeric:tabular-nums}@media(max-width:600px){.pwm-readings{grid-template-columns:1fr 1fr;gap:12px}.pwm-settings{grid-template-columns:1fr;gap:18px}.pwm-readings strong{font-size:24px}}
</style>
