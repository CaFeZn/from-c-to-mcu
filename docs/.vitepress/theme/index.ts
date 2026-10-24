import DefaultTheme from 'vitepress/theme'
import { defineAsyncComponent, h } from 'vue'
import CourseHome from './components/CourseHome.vue'
import LessonFooter from './components/LessonFooter.vue'
import BuildRunLab from './components/BuildRunLab.vue'
import BitsLab from './components/BitsLab.vue'
import PwmLab from './components/PwmLab.vue'
import InterruptLab from './components/InterruptLab.vue'
import './style.css'
const LocalGraph = defineAsyncComponent(() => import('./components/LocalGraph.vue'))
const McuArchitectureLab = defineAsyncComponent(() => import('./components/McuArchitectureLab.vue'))
const FlashExecutionLab = defineAsyncComponent(() => import('./components/FlashExecutionLab.vue'))
const AddressDecodeLab = defineAsyncComponent(() => import('./components/AddressDecodeLab.vue'))
const RegisterWriteLab = defineAsyncComponent(() => import('./components/RegisterWriteLab.vue'))
export default {
  extends: DefaultTheme,
  Layout: () => h(DefaultTheme.Layout, null, {
    'doc-after': () => h(LessonFooter),
    'aside-outline-after': () => h(LocalGraph),
    'doc-before': () => h(LocalGraph, { mobile: true })
  }),
  enhanceApp({ app }) {
    for (const [name, component] of Object.entries({ CourseHome, BuildRunLab, BitsLab, PwmLab, InterruptLab, McuArchitectureLab, FlashExecutionLab, AddressDecodeLab, RegisterWriteLab })) app.component(name, component)
  }
}
