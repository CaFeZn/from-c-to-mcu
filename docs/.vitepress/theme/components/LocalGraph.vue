<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, useId, watch } from 'vue'
import { useData, useRoute, useRouter, withBase } from 'vitepress'
import { Expand, Minus, Network, Plus, RotateCcw, X } from '@lucide/vue'
import { data as graph } from '../../course-graph.data'
import { normalizePageId, selectLocalGraph } from '../local-graph.mjs'
import GraphView from './GraphView.vue'
import { createFocusTrap } from 'focus-trap'

const props = defineProps({ mobile: Boolean })
const route = useRoute(), router = useRouter(), { site } = useData()
const currentId = computed(() => normalizePageId(route.path, site.value.base))
const local = computed(() => selectLocalGraph(graph, currentId.value))
const outgoing = computed(() => new Set(local.value.outgoing)), incoming = computed(() => new Set(local.value.incoming))
const relation = id => id === currentId.value ? 'current' : outgoing.value.has(id) && incoming.value.has(id) ? 'both' : outgoing.value.has(id) ? 'outgoing' : 'incoming'
const relationLabel = id => ({ current: '当前文件', outgoing: '此页引用', incoming: '引用此页', both: '互相引用' })[relation(id)]
const viewNodes = computed(() => local.value.nodes.map(node => ({ ...node, relation: relation(node.id), relationLabel: relationLabel(node.id) })))
const neighbors = computed(() => viewNodes.value.filter(node => node.id !== currentId.value))
const inlineView = ref(null), modalView = ref(null), dialog = ref(null), expanded = ref(false)
const inlineScale = ref(1), modalScale = ref(1), inlineReady = ref(false), modalReady = ref(false)
const mounted = ref(false)
onMounted(() => { mounted.value = true })
const id = 'local-graph-' + useId()
let previousOverflow = null
let focusTrap = null
const activeView = () => expanded.value ? modalView.value : inlineView.value
function zoomBy(factor) { activeView()?.zoomBy(factor) }
function reset() { activeView()?.resetView() }
async function open() {
  if (!local.value.current || expanded.value) return
  previousOverflow = document.body.style.overflow
  document.body.style.overflow = 'hidden'
  expanded.value = true
  await nextTick()
  if (!expanded.value || !dialog.value) { restoreScroll(); return }
  dialog.value.showModal()
  focusTrap = createFocusTrap(dialog.value, {
    initialFocus: () => dialog.value.querySelector('.graph-close'),
    fallbackFocus: () => dialog.value.querySelector('.graph-close'),
    escapeDeactivates: false,
    returnFocusOnDeactivate: false
  })
  focusTrap.activate()
}
function restoreScroll() { if (previousOverflow !== null) { document.body.style.overflow = previousOverflow; previousOverflow = null } }
function closed() {
  // A queued close event must not dismiss a dialog that has already reopened.
  if (dialog.value?.open) return
  focusTrap?.deactivate(); expanded.value = false; modalReady.value = false; restoreScroll()
}
// Release the trap before native dialog closure restores the previous focus.
function close() { focusTrap?.deactivate(); if (dialog.value?.open) dialog.value.close(); closed() }
function navigate(url) { close(); router.go(withBase(url)) }
function backdrop(event) { if (event.target === dialog.value) { const r = dialog.value.getBoundingClientRect(); if (event.clientX < r.left || event.clientX > r.right || event.clientY < r.top || event.clientY > r.bottom) close() } }
watch(currentId, () => { close(); inlineReady.value = false })
onBeforeUnmount(() => { focusTrap?.deactivate(); if (dialog.value?.open) dialog.value.close(); restoreScroll() })
</script>

<template>
  <div v-if="local.current" :class="mobile ? 'local-graph-mobile' : 'local-graph'" :data-testid="mobile ? 'local-graph-mobile' : 'local-graph'" :data-current="currentId">
    <button v-if="mobile" type="button" class="graph-launcher" :disabled="!mounted" data-testid="local-graph-open" @click="open"><Network :size="16" aria-hidden="true" />关系图谱</button>
    <template v-else>
      <header class="graph-heading"><h3>关系图谱</h3><div class="graph-tools">
        <button type="button" title="缩小" aria-label="缩小关系图" :disabled="!inlineReady || inlineScale <= 0.61" data-testid="graph-zoom-out" @click="zoomBy(0.8)"><Minus :size="15" /></button>
        <button type="button" title="放大" aria-label="放大关系图" :disabled="!inlineReady || inlineScale >= 3.99" data-testid="graph-zoom-in" @click="zoomBy(1.25)"><Plus :size="15" /></button>
        <button type="button" title="重置视图" aria-label="重置关系图视图" :disabled="!inlineReady" data-testid="graph-reset" @click="reset"><RotateCcw :size="14" /></button>
        <button type="button" title="展开当前文件关系" aria-label="展开当前文件关系" :disabled="!mounted" data-testid="local-graph-open" @click="open"><Expand :size="14" /></button>
      </div></header>
      <GraphView ref="inlineView" :nodes="viewNodes" :links="local.links" :current-id="currentId" @navigate="navigate" @scale="inlineScale = $event" @ready="inlineReady = $event" />
      <p v-if="!neighbors.length" class="graph-empty">暂无关联页面</p>
      <details v-else class="graph-related"><summary>关联页面 <span>{{ neighbors.length }}</span></summary><ul><li v-for="node in neighbors" :key="node.id"><a :href="withBase(node.url)">{{ node.label }}</a><small>{{ node.relationLabel }}</small></li></ul></details>
    </template>
    <Teleport v-if="mounted" to="body">
      <dialog ref="dialog" class="graph-dialog" :aria-labelledby="id + '-title'" data-testid="graph-dialog" @close="closed" @cancel.prevent="close" @click="backdrop">
        <header class="graph-dialog-header"><div><h2 :id="id + '-title'">当前文件关系</h2><p>{{ local.current.title }}</p></div><button type="button" class="graph-close" title="关闭关系图" aria-label="关闭关系图" autofocus @click="close"><X :size="20" /></button></header>
        <div class="graph-dialog-toolbar"><div class="graph-key"><span class="key-current">当前</span><span class="key-outgoing">此页引用</span><span class="key-incoming">引用此页</span><span class="key-both">双向</span></div><div class="graph-tools">
          <button type="button" title="缩小" aria-label="缩小关系图" :disabled="!modalReady || modalScale <= 0.61" @click="zoomBy(0.8)"><Minus :size="16" /></button>
          <button type="button" title="放大" aria-label="放大关系图" :disabled="!modalReady || modalScale >= 3.99" @click="zoomBy(1.25)"><Plus :size="16" /></button>
          <button type="button" title="重置视图" aria-label="重置关系图视图" :disabled="!modalReady" @click="reset"><RotateCcw :size="16" /></button>
        </div></div>
        <GraphView v-if="expanded" ref="modalView" large :nodes="viewNodes" :links="local.links" :current-id="currentId" @navigate="navigate" @scale="modalScale = $event" @ready="modalReady = $event" />
        <ul class="graph-dialog-links"><li v-for="node in neighbors" :key="node.id"><a :href="withBase(node.url)" @click="close">{{ node.label }}</a><small>{{ node.relationLabel }}</small></li><li v-if="!neighbors.length" class="graph-empty">暂无关联页面</li></ul>
      </dialog>
    </Teleport>
  </div>
</template>

<style scoped>
.local-graph { margin-top: 22px; padding-top: 16px; border-top: 1px solid var(--vp-c-divider); min-width: 0; letter-spacing: 0; }
.graph-heading { display: flex; align-items: center; justify-content: space-between; gap: 4px; }
.graph-heading h3 { margin: 0; font-size: 13px; font-weight: 600; line-height: 28px; }
.graph-tools { display: flex; flex: 0 0 auto; gap: 2px; }
.graph-tools button, .graph-close { display: inline-flex; align-items: center; justify-content: center; width: 26px; height: 28px; flex: 0 0 auto; padding: 0; color: var(--vp-c-text-2); border: 1px solid transparent; border-radius: 4px; cursor: pointer; }
.graph-tools button:hover, .graph-close:hover { background: var(--vp-c-default-soft); color: var(--vp-c-brand-1); }
.graph-tools button:disabled { opacity: .35; cursor: default; }
.graph-tools button:focus-visible, .graph-close:focus-visible, .graph-launcher:focus-visible, .graph-related summary:focus-visible { outline: 2px solid var(--vp-c-brand-1); outline-offset: 2px; }
.graph-related { border-top: 1px solid var(--vp-c-divider); padding-top: 8px; font-size: 12px; }
.graph-related summary { cursor: pointer; color: var(--vp-c-text-2); }
.graph-related summary span { float: right; font-variant-numeric: tabular-nums; }
.graph-related ul { margin: 8px 0 0; padding: 0; list-style: none; max-height: 220px; overflow-y: auto; }
.graph-related li, .graph-dialog-links li { padding: 7px 0; line-height: 1.5; border-bottom: 1px solid var(--vp-c-divider); }
.graph-related a, .graph-dialog-links a { color: var(--vp-c-text-1); overflow-wrap: anywhere; }
.graph-related a:hover, .graph-dialog-links a:hover { color: var(--vp-c-brand-1); }
.graph-related small, .graph-dialog-links small { display: block; color: var(--vp-c-text-3); font-size: 11px; }
.graph-empty { margin: 8px 0; font-size: 12px; color: var(--vp-c-text-3); }
.local-graph-mobile { display: none; }
.graph-launcher:disabled { opacity: .4; cursor: wait; }
.graph-launcher { display: inline-flex; align-items: center; gap: 6px; padding: 5px 0; color: var(--vp-c-text-2); font-size: 13px; line-height: 24px; cursor: pointer; }
.graph-dialog { box-sizing: border-box; width: min(900px, calc(100vw - 32px)); max-height: calc(100dvh - 32px); margin: auto; padding: 20px; border: 1px solid var(--vp-c-divider); border-radius: 8px; background: var(--vp-c-bg); color: var(--vp-c-text-1); overflow: auto; letter-spacing: 0; }
.graph-dialog::backdrop { background: rgb(0 0 0 / .42); }
.graph-dialog-header { display: flex; align-items: flex-start; justify-content: space-between; gap: 16px; }
.graph-dialog-header > div { min-width: 0; }
.graph-dialog-header h2 { margin: 0; font-size: 18px; font-weight: 600; line-height: 26px; }
.graph-dialog-header p { margin: 4px 0 12px; font-size: 13px; line-height: 21px; color: var(--vp-c-text-2); overflow-wrap: anywhere; }
.graph-dialog-toolbar { display: flex; align-items: center; justify-content: space-between; flex-wrap: wrap; gap: 8px; border-bottom: 1px solid var(--vp-c-divider); padding-bottom: 8px; }
.graph-key { display: flex; flex-wrap: wrap; gap: 6px 12px; font-size: 11px; color: var(--vp-c-text-2); }
.graph-key span::before { content: ''; display: inline-block; width: 7px; height: 7px; border-radius: 50%; margin-right: 5px; background: var(--vp-c-brand-1); }
.graph-key .key-outgoing::before { background: #397fa1; }.graph-key .key-incoming::before { background: #ac7a28; }.graph-key .key-both::before { background: #627b68; }
.graph-dialog-links { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 0 20px; list-style: none; margin: 0; padding: 8px 0 0; max-height: min(20dvh, 160px); overflow-y: auto; border-top: 1px solid var(--vp-c-divider); font-size: 13px; }
@media (max-width: 1279px) { .local-graph-mobile { display: flex; justify-content: flex-end; margin: 0 0 8px; } }
@media (max-width: 560px) { .graph-dialog { padding: 14px; width: calc(100vw - 20px); max-height: calc(100dvh - 20px); }.graph-dialog-links { grid-template-columns: minmax(0, 1fr); }.graph-dialog-header h2 { font-size: 16px; } }
</style>
