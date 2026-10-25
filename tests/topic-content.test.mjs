import test from 'node:test';
import assert from 'node:assert/strict';
import { readFile, access } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import matter from 'gray-matter';
import MarkdownIt from 'markdown-it';
import { parseHTML } from 'linkedom';
import { courseMarkdown } from '../docs/.vitepress/course-markdown.mjs';

const root = fileURLToPath(new URL('../', import.meta.url));
const json = async name => JSON.parse(await readFile(path.join(root, name), 'utf8'));
const manifest = await json('docs/.vitepress/topic-manifest.json');
const routes = await json('docs/.vitepress/routes.json');
const course = await json('docs/.vitepress/course.json');
const md = new MarkdownIt({ html: true }).use(courseMarkdown, { routes });
const index = await readFile(path.join(root, 'docs/topics/index.md'), 'utf8');

test('the frozen completion scope covers 95 old topics and two requested practices exactly once', () => {
  assert.equal(manifest.topics.length, 97);
  assert.equal(manifest.topics.filter(topic => topic.wasPending).length, 95);
  for (const field of ['key', 'path', 'link']) assert.equal(new Set(manifest.topics.map(topic => topic[field])).size, manifest.topics.length, field);
  assert.deepEqual(manifest.topics.filter(topic => !topic.wasPending).map(topic => topic.link).sort(), ['/notes/git-workflow', '/notes/markdown-notes']);
});

test('every completed topic has a public page, resolved route and discoverable index entry', async () => {
  assert.equal(Object.values(routes).filter(route => route.pending).length, 0);
  assert.equal(course.pendingCount, 0);
  assert.equal(course.readyNotes.length, 102);
  for (const topic of manifest.topics) {
    await access(path.join(root, topic.path));
    assert.equal(routes[topic.key]?.link, topic.link, topic.key);
    assert.equal(routes[topic.key]?.pending, false, topic.key);
    assert.ok(course.readyNotes.some(note => note.link === topic.link), topic.key);
    assert.ok(index.includes('](' + topic.link + ')'), topic.key + ' is not indexed');
    if (topic.anchor) assert.ok(index.includes('id="' + topic.anchor + '"'), topic.key + ' lost its old anchor');
  }
});

for (const topic of manifest.topics) {
  test('complete teaching content: ' + topic.key, async t => {
    const source = await readFile(path.join(root, topic.path), 'utf8');
    const { data, content } = matter(source);
    assert.equal(data.sourceKey, topic.key);
    assert.equal(data.title, topic.key);
    assert.ok(typeof data.description === 'string' && data.description.length > 12, 'specific description');
    assert.doesNotMatch(source, /^(?:status|draft):\s*(?:pending|todo|true)/m);
    const inlineCode = md.parse(content, {}).filter(token => token.type === 'inline').flatMap(token => token.children || []).filter(token => token.type === 'code_inline');
    assert.ok(inlineCode.every(token => !token.content.includes('{{')), 'literal inline C initializers must not open Vue interpolation');
    const html = md.render(content);
    const { document } = parseHTML('<html><body><article>' + html + '</article></body></html>');
    const article = document.querySelector('article');
    assert.equal(article.querySelectorAll('h1').length, 1, 'one clear article title');
    assert.ok(article.querySelectorAll('h2,h3').length >= 5, 'full explanation, not a stub');
    const prose = article.cloneNode(true);
    for (const element of prose.querySelectorAll('pre,code,table,script,style')) element.remove();
    const han = (prose.textContent.match(/[\u3400-\u9fff]/g) || []).length;
    // This is a stub alarm, not a substitute for the recorded human/agent technical review.
    assert.ok(han >= 1000, 'too little explanatory prose: ' + han + ' Han characters');
    assert.ok(article.querySelector('pre,table'), 'a worked code example or concrete observation/calculation table');
    const answers = [...article.querySelectorAll('details')].filter(details => /答案|解析|分析|推演|结果|揭晓|检查|对照|参考|验证/.test(details.querySelector('summary')?.textContent || ''));
    assert.ok(answers.some(details => details.textContent.length > 80), 'exercise with a substantive folded answer');
    const links = [...article.querySelectorAll('a[href]')].map(link => link.getAttribute('href'));
    assert.ok(links.some(href => /^https:\/\//.test(href)), 'traceable technical reference');
    assert.ok(links.some(href => /^\/lessons\//.test(href)), 'return to an appropriate main lesson');
    assert.ok(links.some(href => /^\/notes\//.test(href) && href !== topic.link), 'related reading');
    assert.equal(article.querySelectorAll('[data-pending="true"]').length, 0, 'no unresolved topic links');
    t.diagnostic(JSON.stringify({ path: topic.path, proseHan: han, bytes: Buffer.byteLength(source) }));
  });
}

test('Git and Markdown practice is connected back into the main learning path', async () => {
  const intro = await readFile(path.join(root, 'docs/lessons/1-0.md'), 'utf8');
  const split = await readFile(path.join(root, 'docs/lessons/1-8.md'), 'utf8');
  const project = await readFile(path.join(root, 'docs/lessons/8-3.md'), 'utf8');
  assert.match(intro, /\/notes\/git-workflow/);
  assert.match(intro, /\/notes\/markdown-notes/);
  assert.match(split, /\/notes\/git-workflow/);
  assert.match(project, /\/notes\/markdown-notes/);
  assert.ok(intro.indexOf('/notes/git-workflow') > intro.indexOf('## 第一步'), 'do not displace the install-and-run entry');
});
