import { defineConfig } from 'vitepress';
import course from './course.json';
import routes from './routes.json';
import { courseMarkdown } from './course-markdown.mjs';

export default defineConfig({
  lang: 'zh-CN', title: '从 C 到单片机', description: '从第一句输出，到能运行的嵌入式项目。',
  base: '/from-c-to-mcu/', cleanUrls: true, lastUpdated: true,
  head: [['meta', { name: 'theme-color', content: '#087f8c' }]],
  themeConfig: {
    siteTitle: '从 C 到单片机',
    nav: [{ text: '课程', link: '/' }, { text: '动画实验', link: '/labs/' }, { text: '原理与拓展', link: '/topics/' }, { text: '补充资料', link: '/resources/' }],
    sidebar: [{ text: '开始学习', items: [{ text: '课程导航', link: '/' }, { text: '动画实验', link: '/labs/' }] }, ...course.chapters.map(ch => ({ text: ch.title, collapsed: ch.id !== 1, items: ch.lessons.map(l => ({ text: l.id + ' ' + l.title, link: l.link })) })), { text: '按需深入', collapsed: true, items: [{ text: '第 0 章 · 工具原理选读', link: '/optional/' }, { text: '原理与拓展', link: '/topics/' }, { text: '补充视频', link: '/resources/' }] }],
    socialLinks: [{ icon: 'github', link: 'https://github.com/CaFeZn/from-c-to-mcu' }],
    search: { provider: 'local', options: { locales: { root: { translations: { button: { buttonText: '搜索课程', buttonAriaLabel: '搜索课程' }, modal: { noResultsText: '没有找到相关内容', resetButtonTitle: '清空', footer: { selectText: '选择', navigateText: '切换', closeText: '关闭' } } } } } } },
    outline: { level: [2, 3], label: '本页内容' },
    docFooter: { prev: '上一节', next: '下一节' },
    returnToTopLabel: '回到顶部', sidebarMenuLabel: '课程目录', darkModeSwitchLabel: '外观', lightModeSwitchTitle: '浅色', darkModeSwitchTitle: '深色',
    lastUpdated: { text: '更新于', formatOptions: { dateStyle: 'medium' } },
    editLink: { pattern: 'https://github.com/CaFeZn/from-c-to-mcu/edit/main/docs/:path', text: '在 GitHub 编辑此页' }
  },
  markdown: { lineNumbers: true, config(md) { md.use(courseMarkdown, { routes }); } },
  vite: { server: { host: '127.0.0.1' } }
});
