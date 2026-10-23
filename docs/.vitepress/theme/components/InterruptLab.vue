<script setup lang="ts">
import { computed, onUnmounted, ref } from 'vue'
import { Play, Pause, RotateCcw, SkipForward, Zap } from '@lucide/vue'
const frames = [
  { lane:'main', line:0, flag:0, title:'主循环采集输入', detail:'sample_inputs();', handled:0 },
  { lane:'event', line:-1, flag:0, title:'引脚事件到来', detail:'中断请求挂起，主循环暂时停下。', handled:0 },
  { lane:'isr', line:0, flag:0, title:'进入中断处理', detail:'进入 ISR，先处理当前事件。', handled:0 },
  { lane:'isr', line:1, flag:1, title:'只留下一个标记', detail:'key_pending = 1;', handled:0 },
  { lane:'main', line:1, flag:1, title:'返回被打断的位置', detail:'update_display();', handled:0 },
  { lane:'main', line:2, flag:1, title:'主循环发现标记', detail:'if (key_pending) {', handled:0 },
  { lane:'main', line:3, flag:0, title:'主循环清除标记', detail:'key_pending = 0;', handled:0 },
  { lane:'main', line:4, flag:0, title:'在主循环处理业务', detail:'handle_key();', handled:1 },
  { lane:'main', line:0, flag:0, title:'继续下一轮循环', detail:'sample_inputs();', handled:1 }
]
const mainLines=['sample_inputs();','update_display();','if (key_pending) {','    key_pending = 0;','    handle_key();','}']
const isrLines=['void key_isr(void) {','    key_pending = 1;','}']
const step=ref(0), playing=ref(false)
const current=computed(()=>frames[step.value])
let timer: ReturnType<typeof setInterval> | undefined
function stop(){playing.value=false;if(timer)clearInterval(timer);timer=undefined}
function next(){if(step.value<frames.length-1)step.value++;else stop();if(step.value===frames.length-1)stop()}
function toggle(){if(playing.value){stop();return}if(step.value===frames.length-1)step.value=0;playing.value=true;timer=setInterval(next,850)}
function reset(){stop();step.value=0}
function trigger(){stop();step.value=1}
onUnmounted(stop)
</script>
<template>
  <section class="teaching-lab irq-lab" data-testid="interrupt-lab" aria-label="中断与主循环执行顺序">
    <header class="lab-top"><div><span class="lab-kicker">05.1 / INTERRUPT</span><h3>中断只留标记，主循环做业务</h3></div><span class="model-tag">单次事件示意</span></header>
    <div class="irq-state"><div><span>当前执行</span><strong data-testid="irq-lane">{{ current.lane==='isr'?'中断处理':current.lane==='event'?'外部事件':'主循环' }}</strong></div><div><span>key_pending</span><strong class="flag" :class="{set:current.flag}" data-testid="irq-flag">{{ current.flag }}</strong></div><div><span>已处理</span><strong data-testid="irq-handled">{{ current.handled }}</strong></div></div>
    <div class="code-lanes"><div class="code-lane"><h4>主循环</h4><ol><li v-for="(line,index) in mainLines" :key="index" :class="{active:current.lane==='main'&&current.line===index}"><code>{{ line }}</code></li></ol></div><div class="code-lane isr"><h4>中断处理</h4><ol><li v-for="(line,index) in isrLines" :key="index" :class="{active:current.lane==='isr'&&current.line===index}"><code>{{ line }}</code></li></ol><div class="irq-event" :class="{pending:current.lane==='event'}"><Zap :size="18"/><span>引脚事件</span></div></div></div>
    <div class="irq-caption" aria-live="polite"><strong>{{ String(step+1).padStart(2,'0') }} / {{ frames.length }} · {{ current.title }}</strong><span>{{ current.detail }}</span></div>
    <div class="irq-track" aria-label="执行进度"><span v-for="(_,index) in frames" :key="index" :class="{done:index<=step,isr:index===2||index===3}"></span></div>
    <footer class="lab-controls"><button class="lab-icon primary" type="button" :title="playing?'暂停':'播放'" :aria-label="playing?'暂停':'播放'" @click="toggle"><Pause v-if="playing" :size="18"/><Play v-else :size="18"/></button><button class="lab-icon" type="button" title="下一步" aria-label="下一步" :disabled="step===frames.length-1" @click="stop();next()"><SkipForward :size="18"/></button><button class="lab-icon" type="button" title="重置" aria-label="重置" @click="reset"><RotateCcw :size="18"/></button><button class="lab-command" type="button" @click="trigger"><Zap :size="16"/>触发中断</button></footer>
  </section>
</template>
<style scoped>
.irq-state{display:grid;grid-template-columns:1.5fr 1fr 1fr;gap:20px;margin:22px 0}.irq-state span{display:block;font-size:12px;color:var(--vp-c-text-2)}.irq-state strong{font-size:21px;line-height:34px;font-variant-numeric:tabular-nums}.flag.set{color:#c66550}.code-lanes{display:grid;grid-template-columns:1.2fr 1fr;gap:16px}.code-lane{min-width:0;background:var(--vp-c-bg-soft);padding:12px 0}.code-lane h4{font-size:12px;color:var(--vp-c-text-2);margin:0 12px 10px}.code-lane ol{list-style:none;padding:0;margin:0}.code-lane li{font-size:12px;line-height:24px;min-height:24px;padding:1px 12px;border-left:3px solid transparent;overflow-x:auto;white-space:pre}.code-lane li code{background:none;color:var(--vp-c-text-1);padding:0;font-size:inherit}.code-lane li.active{background:var(--vp-c-brand-soft);border-color:var(--vp-c-brand-1)}.isr li.active{background:rgba(198,101,80,.13);border-color:#c66550}.irq-event{display:flex;align-items:center;gap:8px;margin:20px 12px 0;font-size:12px;color:var(--vp-c-text-3)}.irq-event.pending{color:#c66550}.irq-caption{margin-top:18px;display:flex;flex-direction:column;gap:5px;min-height:57px}.irq-caption strong{font-size:14px}.irq-caption span{font-size:12px;color:var(--vp-c-text-2)}.irq-track{display:grid;grid-template-columns:repeat(9,1fr);gap:4px;margin:16px 0 20px}.irq-track span{height:5px;background:var(--vp-c-divider);border-radius:2px}.irq-track span.done{background:var(--vp-c-brand-1)}.irq-track span.done.isr{background:#c66550}@media(max-width:600px){.code-lanes{grid-template-columns:1fr}.irq-state{gap:10px}.irq-state strong{font-size:18px}.irq-event{margin:12px}.irq-caption{min-height:72px}}
</style>
