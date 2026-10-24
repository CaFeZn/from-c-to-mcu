<script setup>
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import { withBase } from 'vitepress'
import { select } from 'd3-selection'
import { forceSimulation, forceLink, forceManyBody, forceCollide, forceX, forceY } from 'd3-force'
import { zoom, zoomIdentity } from 'd3-zoom'
import { drag, dragEnable } from 'd3-drag'

const props = defineProps({ nodes: { type: Array, required: true }, links: { type: Array, required: true }, currentId: { type: String, required: true }, large: Boolean })
const emit = defineEmits(['navigate', 'scale', 'ready'])
const frame = ref(null), svg = ref(null), nodes = ref([]), edges = ref([]), active = ref(null)
const size = ref({ width: 224, height: 224 }), transform = ref({ x: 0, y: 0, k: 1 }), ready = ref(false)
const transformText = computed(() => 'translate(' + transform.value.x + ',' + transform.value.y + ') scale(' + transform.value.k + ')')
let simulation, zoomer, observer, media, canvasContext, generation = 0, disposed = false, dragging = false, panning = false, lastDrag = 0
let reduced = false
const clamp = (value, max, endPadding = 18) => Math.max(18, Math.min(max - endPadding, value))
function constrain() {
  for (const node of nodes.value) {
    node.x = clamp(node.x, size.value.width)
    node.y = clamp(node.y, size.value.height, 34)
  }
}
// Keep labels clear of both other labels and neighboring nodes.
const labelPositions = computed(() => {
  const result = new Map(), occupied = []
  const w = size.value.width, h = size.value.height
  const measure = text => canvasContext ? canvasContext.measureText(text).width : Array.from(text).length * 11
  const intersects = (a, b, padding = 3) => a.left < b.right + padding && a.right + padding > b.left && a.top < b.bottom + padding && a.bottom + padding > b.top
  const priority = node => node.id === active.value ? 0 : node.id === props.currentId ? 1 : 2
  const ordered = [...nodes.value].sort((a, b) => priority(a) - priority(b) || a.id.localeCompare(b.id))
  for (const node of ordered) {
    const full = (node.label || node.title).replace(/^【(?:原理|拓展)】\s*/, '')
    const maxWidth = node.id === active.value ? w - 24 : node.id === props.currentId ? Math.min(118, w - 24) : props.large && w > 500 ? 160 : 88
    const chars = Array.from(full)
    while (chars.length > 1 && measure(chars.join('') + (chars.length < Array.from(full).length ? '...' : '')) > maxWidth) chars.pop()
    const text = chars.join('') + (chars.length < Array.from(full).length ? '...' : '')
    const width = measure(text) + 4
    const centered = Math.max(8, Math.min(w - width - 8, node.x - width / 2))
    const candidates = node.id === props.currentId
      ? [[centered, node.y + 22], [centered, node.y - 13]]
      : [[centered, node.y - 13], [centered, node.y + 22]]
    candidates.push([node.x + 11, node.y + 4], [node.x - width - 11, node.y + 4], [centered, node.y - 29], [centered, node.y + 38])
    if (priority(node) < 2) candidates.push([Math.max(8, (w - width) / 2), h - 9])
    for (const [x, y] of candidates) {
      const rect = { left: x - 2, right: x + width, top: y - 11, bottom: y + 2 }
      if (rect.left < 4 || rect.right > w - 4 || rect.top < 4 || rect.bottom > h - 4) continue
      if (occupied.some(other => intersects(rect, other))) continue
      if (nodes.value.some(other => other.id !== node.id && intersects(rect, { left: other.x - 7, right: other.x + 7, top: other.y - 7, bottom: other.y + 7 }, 1))) continue
      result.set(node.id, { x, y, text })
      occupied.push(rect)
      break
    }
  }
  return result
})
function labelVisible(node) { return labelPositions.value.has(node.id) }
function labelAnchor() { return 'start' }
function labelX(node) { return labelPositions.value.get(node.id)?.x }
function labelY(node) { return labelPositions.value.get(node.id)?.y }
function labelText(node) { return labelPositions.value.get(node.id)?.text || '' }
function linked(edge) { return active.value && (edge.source.id === active.value || edge.target.id === active.value) }
function linkPath(node) { return new URL(withBase(node.url), 'https://course.invalid').pathname }
function navigate(event, node) {
  if (event.ctrlKey || event.metaKey || event.shiftKey || event.altKey || event.button > 0) return
  event.preventDefault()
  if (performance.now() < lastDrag) return
  emit('navigate', node.url)
}
function setReduced(event) { reduced = event.matches; if (reduced) simulation?.stop() }
function stopGestures() {
  if (dragging || panning) {
    select(window).on('mousemove.drag mouseup.drag mousemove.zoom mouseup.zoom', null)
    dragEnable(window)
    dragging = false; panning = false
  }
}
async function rebuild() {
  if (!svg.value || disposed) return
  const bounds = frame.value.getBoundingClientRect()
  if (bounds.width < 2 || bounds.height < 2) { simulation?.stop(); return }
  const version = ++generation
  stopGestures()
  lastDrag = 0
  ready.value = false
  simulation?.stop()
  select(svg.value).selectAll('.graph-node').on('.drag', null)
  size.value = { width: bounds.width, height: bounds.height }
  const radius = Math.max(34, Math.min(bounds.width, bounds.height) * 0.34)
  const ordered = [...props.nodes].sort((a, b) => Number(b.id === props.currentId) - Number(a.id === props.currentId) || a.id.localeCompare(b.id))
  nodes.value = ordered.map((node, index) => {
    const current = node.id === props.currentId, angle = (index - 1) * Math.PI * 2 / Math.max(1, ordered.length - 1)
    return { ...node, x: bounds.width / 2 + (current ? 0 : Math.cos(angle) * radius), y: bounds.height / 2 + (current ? 0 : Math.sin(angle) * radius), ...(current ? { fx: bounds.width / 2, fy: bounds.height / 2 } : {}) }
  })
  const ids = new Set(nodes.value.map(node => node.id))
  const links = props.links.filter(edge => ids.has(edge.source) && ids.has(edge.target)).map(edge => ({ ...edge }))
  edges.value = links
  simulation = forceSimulation(nodes.value).stop()
    .force('links', forceLink(links).id(node => node.id).distance(radius * 0.95).strength(0.25))
    .force('charge', forceManyBody().strength(nodes.value.length > 15 ? -32 : -70))
    .force('collision', forceCollide(nodes.value.length > 15 ? 10 : 16))
    .force('x', forceX(bounds.width / 2).strength(0.035))
    .force('y', forceY(bounds.height / 2).strength(0.035)).alphaDecay(0.075).on('tick', constrain)
  for (let i = 0; i < 180; i++) { simulation.tick(); constrain() }
  active.value = null
  select(svg.value).call(zoomer.transform, zoomIdentity)
  await nextTick()
  if (disposed || version !== generation) return
  let startX = 0, startY = 0, moved = false
  select(svg.value).selectAll('.graph-node').data(nodes.value).call(drag()
    .filter(event => !event.button && !event.ctrlKey && !event.metaKey)
    .clickDistance(5)
    .on('start', (event, node) => {
      dragging = true; moved = false; startX = event.x; startY = event.y
      node.fx = node.x; node.fy = node.y
      active.value = node.id
      if (!reduced) simulation.alpha(0.25).alphaTarget(0.1).restart()
    })
    .on('drag', (event, node) => {
      moved ||= Math.hypot(event.x - startX, event.y - startY) > 4
      node.fx = node.x = clamp(event.x, size.value.width)
      node.fy = node.y = clamp(event.y, size.value.height, 34)
      if (reduced) { simulation.tick(12); constrain() }
    })
    .on('end', (_event, node) => {
      dragging = false
      if (moved) lastDrag = performance.now() + 400
      if (node.id !== props.currentId) { node.fx = null; node.fy = null }
      simulation.alphaTarget(0)
      if (reduced) simulation.stop()
    }))
  ready.value = true
  emit('ready', true)
}
function zoomBy(factor) { if (ready.value) select(svg.value).call(zoomer.scaleBy, factor) }
function resetView() { rebuild() }
defineExpose({ zoomBy, resetView })
onMounted(() => {
  media = window.matchMedia('(prefers-reduced-motion: reduce)')
  reduced = media.matches
  media.addEventListener('change', setReduced)
  canvasContext = document.createElement('canvas').getContext('2d')
  if (canvasContext) canvasContext.font = '11px ' + getComputedStyle(svg.value).fontFamily
  zoomer = zoom().extent(() => [[0, 0], [size.value.width, size.value.height]]).scaleExtent([0.6, 4])
    .filter(event => event.type === 'wheel' ? event.ctrlKey || event.metaKey : !event.button && !event.ctrlKey && !event.target.closest('.graph-node'))
    .on('start', event => { panning = event.sourceEvent?.type === 'mousedown' })
    .on('end', () => { panning = false })
    .on('zoom', event => { transform.value = { x: event.transform.x, y: event.transform.y, k: event.transform.k }; emit('scale', event.transform.k) })
  select(svg.value).call(zoomer).on('dblclick.zoom', null)
  observer = new ResizeObserver(() => rebuild())
  observer.observe(frame.value)
  rebuild()
})
watch([() => props.nodes, () => props.links, () => props.currentId], () => rebuild(), { flush: 'post' })
onBeforeUnmount(() => {
  disposed = true; generation++
  simulation?.stop(); observer?.disconnect(); media?.removeEventListener('change', setReduced)
  if (svg.value) { select(svg.value).on('.zoom', null); select(svg.value).selectAll('.graph-node').on('.drag', null) }
  stopGestures()
})
</script>

<template>
  <div ref="frame" class="graph-view" :class="{ large }" data-testid="graph-view" :data-ready="ready" :data-current="currentId" :data-node-count="nodes.length" :data-scale="transform.k.toFixed(2)">
    <svg ref="svg" :viewBox="'0 0 ' + size.width + ' ' + size.height" aria-label="当前文件的关联页面" role="group">
      <g :transform="transformText">
        <g class="graph-edges" aria-hidden="true"><line v-for="edge in edges" :key="edge.source.id + '>' + edge.target.id" :x1="edge.source.x" :y1="edge.source.y" :x2="edge.target.x" :y2="edge.target.y" :class="{ highlighted: linked(edge) }" /></g>
        <!-- VitePress 1.6 prefetch expects HTMLAnchor.pathname, absent on SVG links. -->
        <a v-for="node in nodes" :key="node.id" class="graph-node" tabindex="0" :class="[node.relation, { current: node.id === currentId, active: active === node.id }]" :href="withBase(node.url)" :pathname.prop="linkPath(node)" :aria-label="node.title + (node.relationLabel ? '，' + node.relationLabel : '')" :aria-current="node.id === currentId ? 'page' : undefined" :data-page-id="node.id" @click="navigate($event, node)" @mouseenter="active = node.id" @mouseleave="active = null" @focus="active = node.id" @blur="active = null">
          <title>{{ node.title }}{{ node.relationLabel ? ' · ' + node.relationLabel : '' }}</title>
          <circle class="graph-hit" :cx="node.x" :cy="node.y" r="12" />
          <circle class="graph-dot" :cx="node.x" :cy="node.y" :r="node.id === currentId ? 7 : 4.5" />
          <text v-if="labelVisible(node)" :x="labelX(node)" :y="labelY(node)" :text-anchor="labelAnchor(node)">{{ labelText(node) }}</text>
        </a>
      </g>
    </svg>
  </div>
</template>

<style scoped>
.graph-view { width: 100%; aspect-ratio: 1; min-width: 0; overflow: hidden; touch-action: none; cursor: grab; color: var(--vp-c-text-1); }
.graph-view.large { aspect-ratio: auto; height: min(50dvh, 440px); min-height: 240px; }
.graph-view:active { cursor: grabbing; }
svg { display: block; width: 100%; height: 100%; font-family: var(--vp-font-family-base); overflow: hidden; }
.graph-edges line { stroke: var(--vp-c-text-3); stroke-width: 1; opacity: 0.34; }
.graph-edges line.highlighted { stroke: var(--vp-c-brand-1); opacity: 0.9; stroke-width: 1.5; }
.graph-node { cursor: pointer; outline: none; }
.graph-node .graph-hit { fill: transparent; stroke: none; pointer-events: all; }
.graph-node .graph-dot { fill: var(--vp-c-text-3); stroke: var(--vp-c-bg); stroke-width: 2; }
.graph-node.outgoing .graph-dot { fill: #397fa1; }
.graph-node.incoming .graph-dot { fill: #ac7a28; }
.graph-node.both .graph-dot { fill: #627b68; }
.graph-node.current .graph-dot { fill: var(--vp-c-brand-1); stroke-width: 2.5; }
.graph-node.active .graph-dot, .graph-node:focus-visible .graph-dot { stroke: var(--vp-c-text-1); stroke-width: 2.5; }
.graph-node text { fill: var(--vp-c-text-2); font-size: 11px; letter-spacing: 0; stroke: var(--vp-c-bg); stroke-width: 3.5px; paint-order: stroke; pointer-events: auto; }
.graph-node.current text, .graph-node.active text { fill: var(--vp-c-text-1); font-weight: 600; }
:global(.dark) .graph-node.outgoing .graph-dot { fill: #71b0ce; }
:global(.dark) .graph-node.incoming .graph-dot { fill: #d5ac68; }
:global(.dark) .graph-node.both .graph-dot { fill: #a2bba8; }
</style>
