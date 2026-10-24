import fs from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';
import matter from 'gray-matter';
import { parseArgs } from 'node:util';
import { demoMarkup, injectCourseDemos } from './course-demos.mjs';
const repo = path.dirname(path.dirname(fileURLToPath(import.meta.url)));
const args = parseArgs({ options: { only: { type: 'string', multiple: true } }, allowPositionals: true });
if (args.positionals.length !== 1) throw new Error('Legacy import needs an explicit source directory. Maintain the course in docs/.');
const source = path.resolve(args.positionals[0]);
const selected = new Set(args.values.only || []);
const docs = path.join(repo, 'docs');
const folders = ['00_第0章_自学选读','01_第1章_C语言起步','02_第2章_认识单片机','03_第3章_串口与调试','04_第4章_GPIO','05_第5章_中断与定时器','06_第6章_UART','07_第7章_CAN','08_第8章_综合与排障','分支/原理','分支/拓展'];
await fs.access(path.join(source, '01_第1章_C语言起步', '1.0_搭建环境.md'));
const records = [];
for (const folder of folders) {
  for (const item of await fs.readdir(path.join(source, folder), { withFileTypes: true })) {
    if (!item.isFile() || !item.name.endsWith('.md')) continue;
    const original = folder + '/' + item.name;
    const raw = await fs.readFile(path.join(source, original), 'utf8');
    const parsed = matter(raw);
    const stem = item.name.slice(0, -3);
    const title = (parsed.content.match(/^#\s+(.+)$/m)?.[1] || stem).replace(/^\d+\.\d+\s+/, '');
    const lessonMatch = stem.match(/^(\d+)\.(\d+)_/);
    const chapterMatch = stem.match(/^第(\d+)章/);
    const pending = parsed.data.status === '待写';
    let dest;
    if (lessonMatch) dest = lessonMatch[1] === '0' ? 'optional/build-process.md' : 'lessons/' + lessonMatch[1] + '-' + lessonMatch[2] + '.md';
    else if (chapterMatch) dest = chapterMatch[1] === '0' ? 'optional/index.md' : 'chapters/ch-' + chapterMatch[1] + '.md';
    else if (!pending) {
      const names = { '【原理】从源码到可执行文件': 'source-to-program', '【原理】交叉编译与工具链': 'cross-compilation', '【拓展】让VSCode看懂CMake工程': 'editor-setup', '【原理】单片机内部结构': 'mcu-architecture', '【原理】寄存器与内存映射': 'memory-mapped-io' };
      dest = 'notes/' + (names[stem] || createHash('sha256').update(stem).digest('hex').slice(0, 12)) + '.md';
    }
    const id = lessonMatch ? lessonMatch[1] + '.' + lessonMatch[2] : undefined;
    const topicId = 'topic-' + createHash('sha256').update(stem).digest('hex').slice(0, 10);
    const link = pending ? '/topics/#' + topicId : '/' + dest.replace(/index\.md$/, '').replace(/\.md$/, '');
    records.push({ original, stem, title, id, pending, topicId, link, dest, body: parsed.content, meta: parsed.data, level: folder.endsWith('原理') ? '原理' : folder.endsWith('拓展') ? '拓展' : '' });
  }
}
const resource = matter(await fs.readFile(path.join(source, '99_附录/D_参考视频映射.md'), 'utf8'));
records.push({ original:'99_附录/D_参考视频映射.md', stem:'D_参考视频映射', title:'补充视频与资料', dest:'resources/index.md', link:'/resources/', body:resource.content, meta:{} });
const externalVideo = 'https://www.bilibili.com/video/BV1z14y1G77V';
const routes = { '00_教程导览': {link:'/'}, '分支总表': {link:'/topics/'}, '00_合集索引': {link:externalVideo}, '01_大纲': {link:externalVideo}, '02_关键词索引': {link:externalVideo} };
for (const record of records) {
  if (routes[record.stem]) throw new Error('Duplicate route: ' + record.stem);
  routes[record.stem] = { link:record.link, pending:Boolean(record.pending) };
}
const lessons = records.filter(r=>r.id && !r.id.startsWith('0.')).sort((a,b)=>Number(a.id.split('.')[0])-Number(b.id.split('.')[0]) || Number(a.id.split('.')[1])-Number(b.id.split('.')[1]));
const chapters = Array.from({length:8},(_,i)=>({id:i+1,title:['第 1 章 · C 语言起步','第 2 章 · 认识单片机','第 3 章 · 串口与调试','第 4 章 · GPIO','第 5 章 · 中断与定时器','第 6 章 · UART','第 7 章 · CAN','第 8 章 · 综合与排障'][i],lessons:lessons.filter(r=>r.id.startsWith((i+1)+'.')).map(({id,title,link})=>({id,title,link}))}));
const demos = { '1.0':'BuildRunLab', '1.3':'BitsLab', '5.1':'InterruptLab', '5.3':'PwmLab' };
for (const stem of selected) {
  if (!records.some(record => record.stem === stem && !record.pending)) throw new Error('Requested content is missing or pending: ' + stem);
}
for (const record of records.filter(r=>!r.pending && (!selected.size || selected.has(r.stem)))) {
  let body = injectCourseDemos(record.body, record.stem);
  if (record.stem === 'D_参考视频映射') body = body.replace(/^> 本地导航：.*$/m, '> 原视频由原作者发布；本站仅提供主题映射，不转载第三方字幕。');
  if (demos[record.id]) {
    const demo = demoMarkup(demos[record.id]) + '\n';
    const anchor = record.id === '1.0' ? /^## 第三步：运行它\s*$/m : /^## 动手做\s*$/m;
    if (anchor.test(body)) body = body.replace(anchor, match=>match + '\n' + demo);
    else body = body.replace(/^(#\s+.+\n)/m, '$1\n' + demo);
  }
  // Local verification paths are not part of the public course contract.
  body = body.replace(/[A-Za-z]:[\\/][^\x00-\x7F\\/\n`][^\n`]+/g, 'C:/dev/course-check').replace(/[A-Za-z]:[\\/]Users[\\/][^\\/\s`]+/g, 'C:/Users/your-name');
  if (/(?:gh[pousr]_[A-Za-z0-9]{30,}|-----BEGIN (?:RSA |OPENSSH )?PRIVATE KEY-----|Obsidian笔记|\.dsh[\\/])/i.test(body)) throw new Error('Private material detected in ' + record.original);
  const meta = { title:record.title, description: record.title + ' · 从 C 到单片机', sourceKey:record.stem };
  if (record.pending) meta.search = false;
  if (record.id && !record.id.startsWith('0.')) {
    const index=lessons.indexOf(record);
    meta.lesson = record.id;
    meta.prev = index>0 ? {text:lessons[index-1].id+' '+lessons[index-1].title,link:lessons[index-1].link} : {text:'课程导航',link:'/'};
    meta.next = index<lessons.length-1 ? {text:lessons[index+1].id+' '+lessons[index+1].title,link:lessons[index+1].link} : {text:'动画实验',link:'/labs/'};
  }
  const target=path.join(docs,record.dest);
  await fs.mkdir(path.dirname(target),{recursive:true});
  await fs.writeFile(target,matter.stringify(body,meta),'utf8');
}
const ready=records.filter(r=>r.level&&!r.pending);
let topics='# 原理与拓展\n\n主线先解决眼前的问题；需要更多解释时，再从这里继续。标为待写的主题还没有完整正文。\n\n## 已有内容\n\n'+ready.map(r=>'- <span id="'+r.topicId+'"></span>['+r.title+']('+r.link+')').join('\n')+'\n\n';
for (const level of ['原理','拓展']) {
  topics+='## '+level+' · 待写\n\n| 主题 | 状态 |\n|---|---|\n';
  for(const r of records.filter(r=>r.pending&&r.level===level)) topics+='| <span id="'+r.topicId+'"></span>'+r.stem.replace(/^【[^】]+】/,'')+' | 待写 |\n';
  topics+='\n';
}
await fs.mkdir(path.join(docs,'topics'),{recursive:true});
await fs.writeFile(path.join(docs,'topics/index.md'),matter.stringify(topics,{title:'原理与拓展',outline:false,search:false}).trimEnd()+'\n');
await fs.mkdir(path.join(docs,'.vitepress'),{recursive:true});
await fs.writeFile(path.join(docs,'.vitepress/routes.json'),JSON.stringify(routes,null,2)+'\n');
await fs.writeFile(path.join(docs,'.vitepress/course.json'),JSON.stringify({chapters,lessonCount:lessons.length,readyNotes:ready.map(({title,link})=>({title,link})),pendingCount:records.filter(r=>r.pending).length},null,2)+'\n');
console.log(JSON.stringify({lessons:lessons.length,chapters:chapters.length,readyNotes:ready.length,pending:records.filter(r=>r.pending).length,publishedPages:records.filter(r=>!r.pending).length+1}));
