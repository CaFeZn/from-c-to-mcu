<script setup lang="ts">
import { onMounted, ref, watch } from 'vue'
import { useData } from 'vitepress'
const { frontmatter }=useData()
const done=ref(false)
function read(){try{const ids=JSON.parse(localStorage.getItem('c-to-mcu-progress')||'[]');done.value=ids.includes(String(frontmatter.value.lesson))}catch{done.value=false}}
function save(){try{const ids=new Set<string>(JSON.parse(localStorage.getItem('c-to-mcu-progress')||'[]'));const id=String(frontmatter.value.lesson);if(done.value)ids.add(id);else ids.delete(id);localStorage.setItem('c-to-mcu-progress',JSON.stringify([...ids]))}catch{}}
onMounted(read);watch(()=>frontmatter.value.lesson,read)
</script>
<template><div v-if="frontmatter.lesson" class="lesson-finish"><label><input v-model="done" type="checkbox" @change="save">已完成本节</label><span>进度保存在当前浏览器</span></div></template>
<style scoped>.lesson-finish{display:flex;align-items:center;justify-content:space-between;gap:16px;border-top:1px solid var(--vp-c-divider);padding-top:22px;margin-top:32px}.lesson-finish label{display:flex;align-items:center;gap:9px;font-size:14px;cursor:pointer}.lesson-finish input{width:16px;height:16px;accent-color:var(--vp-c-brand-1)}.lesson-finish span{font-size:12px;color:var(--vp-c-text-2)}@media(max-width:480px){.lesson-finish{align-items:flex-start;flex-direction:column;gap:8px}}</style>
