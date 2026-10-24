import test from 'node:test';
import assert from 'node:assert/strict';
import { mkdtemp, mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { buildCourseGraph, createCourseGraphLoader } from '../docs/.vitepress/course-graph.mjs';
import { normalizePageId, selectLocalGraph } from '../docs/.vitepress/theme/local-graph.mjs';

const page = (url, src = '', frontmatter = {}) => ({ url, src, frontmatter });
const edge = (source, target) => ({ source, target });
const node = id => ({ id, url: id, title: id, label: id, kind: 'page' });
const targets = (graph, source) => graph.links.filter(link => link.source === source).map(link => link.target).sort();

test('page IDs normalize base, HTML, Markdown, index, encoding and fragments', () => {
  for (const input of ['/', '/index', '/index.md', '/index.html', '/from-c-to-mcu/', '/from-c-to-mcu/index.html?q=1#part']) {
    assert.equal(normalizePageId(input), '/');
  }
  for (const input of ['/optional/', '/optional/index.md', '/from-c-to-mcu/optional/index.html#part', 'https://cafezn.github.io/from-c-to-mcu/optional/?q=1']) {
    assert.equal(normalizePageId(input), '/optional');
  }
  assert.equal(normalizePageId('/from-c-to-mcu/notes/%E4%B8%AD%E6%96%87%20name.html#part'), '/notes/\u4e2d\u6587 name');
  assert.equal(normalizePageId('/from-c-to-mcu-extra/a.html'), '/from-c-to-mcu-extra/a');
  assert.equal(normalizePageId('/other/lessons/1-1.html', '/other/'), '/lessons/1-1');
  assert.equal(normalizePageId('/lessons/../notes/a.md', '/'), '/notes/a');
  assert.equal(normalizePageId('/notes/100%25%23%3F.md'), '/notes/100%25%23%3F');
  assert.equal(normalizePageId('/notes/bad%value.html'), '/notes/bad%value');
});

test('wiki aliases and relative files share canonical directed edges', () => {
  const graph = buildCourseGraph([
    page('/lessons/a', [
      '# A',
      '[[Alias|Visible name]] [[Alias#section|Another name]]',
      '[legacy](../old/Alias.md#part)',
      '[relative](../notes/b.md?mode=1#part)',
      '[clean](../notes/b#part)',
      '[root](/from-c-to-mcu/notes/b.html)',
      '[reference][b]',
      '[b]: /notes/b',
      '[directory](../optional/index.md)',
      '[encoded](../notes/%E4%B8%AD%E6%96%87%20name.md)',
      '[missing](../notes/missing.md)',
      '[[Missing]]'
    ].join('\n\n'), { title: 'Lesson A', lesson: '1.1' }),
    page('/notes/b', '# **B**\n\n[back](/lessons/a)'),
    page('/notes/%E4%B8%AD%E6%96%87%20name', '# Encoded'),
    page('/optional/', '# Optional\n\n[relative from directory](../notes/b.md)'),
    { url: '/no-source' }
  ], { routes: { Alias: { link: '/notes/b' }, Missing: { link: '/notes/does-not-exist' } } });
  assert.deepEqual(targets(graph, '/lessons/a'), ['/notes/b', '/notes/\u4e2d\u6587 name', '/optional'].sort());
  assert.deepEqual(targets(graph, '/notes/b'), ['/lessons/a']);
  assert.deepEqual(targets(graph, '/optional'), ['/notes/b']);
  assert.equal(graph.links.length, 5);
  assert.equal(graph.nodes.length, 4);
  assert.deepEqual(graph.nodes.find(item => item.id === '/lessons/a'), { id: '/lessons/a', url: '/lessons/a', title: 'Lesson A', label: '1.1 Lesson A', kind: 'lesson' });
  assert.equal(graph.nodes.find(item => item.id === '/notes/b').title, 'B');
  assert.equal(graph.nodes.find(item => item.id === '/optional').url, '/optional/');
  assert.equal(graph.nodes.some(item => item.id === '/notes/does-not-exist'), false);
});

test('pending, code, self links and generated navigation do not contribute edges', () => {
  const tick = String.fromCharCode(96);
  const src = [
    '---', 'title: A', 'next:', '  link: /nav-only', 'description: "[metadata](/nav-only)"', '---',
    '# A',
    '[[Pending]] [pending alias](Pending.md) [[External]]',
    '<a href="/topics/#pending" data-pending="true">Pending</a>',
    '<span data-pending><a href="/topics/">Pending ancestor</a></span>',
    '[fragment](#part) [[#part]] [self](/a.html#part) [?](?view=1#part)',
    tick + '[[Unknown]] [code](/code-only)' + tick,
    tick.repeat(3) + 'md', '[[Unknown]] [fenced](/code-only)', tick.repeat(3),
    '<code><a href="/code-only">Code</a></code>',
    '<pre><a href="/code-only">Code</a></pre>',
    '<nav><a href="/nav-only">Navigation</a></nav>',
    '<CourseHome />',
    '[image](/image.svg) ![image link](/code-only)',
    '[external](https://example.com/from-c-to-mcu/b)',
    '[mail](mailto:user@example.com) [phone](tel:123)',
    '[not this project](https://cafezn.github.io/b)',
    '> [!tip]- Folded', '> [body](./b.md)',
    '', '<a href="/b#second">Real HTML body link</a>'
  ].join('\n');
  const graph = buildCourseGraph([page('/a', src), page('/b'), page('/topics/'), page('/code-only'), page('/nav-only')], {
    routes: { Pending: { link: '/topics/#pending', pending: true }, External: { link: 'https://example.com/b' } }
  });
  assert.deepEqual(graph.links, [edge('/a', '/b')]);
  for (const item of graph.nodes) assert.deepEqual(Object.keys(item).sort(), ['id', 'kind', 'label', 'title', 'url']);
  assert.deepEqual(Object.keys(graph).sort(), ['links', 'nodes']);
  assert.equal(JSON.stringify(graph).includes('Folded'), false);
});

test('same-site absolute URLs and base paths agree without accepting other origins', () => {
  const graph = buildCourseGraph([
    page('/a', [
      '[absolute](https://cafezn.github.io/from-c-to-mcu/notes/b.html?x=1#part)',
      '[protocol relative](//cafezn.github.io/from-c-to-mcu/notes/b)',
      '[base](/from-c-to-mcu/notes/b)',
      '[absolute directory](https://cafezn.github.io/from-c-to-mcu/optional/index.html)',
      '[wrong base](https://cafezn.github.io/from-c-to-mcu-other/notes/c)',
      '[wrong scheme](http://cafezn.github.io/from-c-to-mcu/notes/c)',
      '[other origin](https://example.com/from-c-to-mcu/notes/c)',
      '[wrong project](https://cafezn.github.io/notes/c)'
    ].join('\n\n')),
    page('/notes/b'), page('/notes/c'), page('/optional/')
  ]);
  assert.deepEqual(targets(graph, '/a'), ['/notes/b', '/optional']);
  const custom = buildCourseGraph([page('/a', '[b](https://example.test/site/b.html#part)'), page('/b')], { base: '/site/', origin: 'https://example.test' });
  assert.deepEqual(custom.links, [edge('/a', '/b')]);
});

test('directory indexes retain URLs and source keys resolve only to existing pages', () => {
  const graph = buildCourseGraph([
    page('/index.html', '[[Optional]]'),
    page('/optional/index.html', '[home](../index.md)', { sourceKey: 'Optional', title: 'Optional' })
  ]);
  assert.deepEqual(graph.nodes.map(({ id, url }) => ({ id, url })), [{ id: '/', url: '/' }, { id: '/optional', url: '/optional/' }]);
  assert.deepEqual(graph.links, [edge('/', '/optional'), edge('/optional', '/')]);
});

test('local graph distinguishes incoming, outgoing and mutual neighbors without second hops', () => {
  const graph = {
    nodes: ['a', 'out', 'in', 'both', 'second', 'alone'].map(key => node('/' + key)),
    links: [edge('/a', '/out'), edge('/a', '/out'), edge('/in', '/a'), edge('/a', '/both'), edge('/both', '/a'), edge('/out', '/in'), edge('/out', '/second'), edge('/second', '/both'), edge('/a', '/missing'), edge('/a', '/a')]
  };
  const original = structuredClone(graph);
  for (const item of [...graph.nodes, ...graph.links]) Object.freeze(item);
  Object.freeze(graph.nodes); Object.freeze(graph.links); Object.freeze(graph);
  const local = selectLocalGraph(graph, '/from-c-to-mcu/a.html#part');
  assert.equal(local.current.id, '/a');
  assert.deepEqual(local.outgoing, ['/both', '/out']);
  assert.deepEqual(local.incoming, ['/both', '/in']);
  assert.deepEqual(local.nodes.map(item => item.id).sort(), ['/a', '/both', '/in', '/out']);
  assert.equal(local.links.length, 5);
  assert.ok(local.links.some(link => link.source === '/out' && link.target === '/in'));
  assert.ok(local.links.every(link => link.source !== '/second' && link.target !== '/second'));
  local.nodes[0].title = 'Client-owned';
  local.links[0].source = 'Client-owned';
  assert.deepEqual(graph, original);
  assert.deepEqual(selectLocalGraph(graph, '/missing'), { current: null, nodes: [], links: [], outgoing: [], incoming: [] });
  assert.deepEqual(selectLocalGraph(graph, '/alone'), { current: node('/alone'), nodes: [node('/alone')], links: [], outgoing: [], incoming: [] });
});

test('VitePress loader watches routes and recomputes added, edited and deleted public pages', async t => {
  const root = await mkdtemp(path.join(os.tmpdir(), 'course-graph-'));
  const docs = path.join(root, 'docs');
  const routesFile = path.join(docs, '.vitepress', 'routes.json');
  const previousConfig = globalThis.VITEPRESS_CONFIG;
  const previousCwd = process.cwd();
  process.chdir(root);
  t.after(async () => {
    globalThis.VITEPRESS_CONFIG = previousConfig;
    process.chdir(previousCwd);
    await rm(root, { recursive: true, force: true });
  });
  for (const folder of ['.vitepress', 'public', 'notes', 'artifacts', 'authoring']) await mkdir(path.join(docs, folder), { recursive: true });
  await writeFile(routesFile, JSON.stringify({ Alias: { link: '/notes/a' } }));
  await writeFile(path.join(docs, 'index.md'), '---\ntitle: Home\n---\n[[Alias]]');
  await writeFile(path.join(docs, 'notes/a.md'), '# A');
  for (const folder of ['.vitepress', 'public', 'artifacts', 'authoring']) await writeFile(path.join(docs, folder, 'ignored.md'), '[[Unknown private content]]');
  globalThis.VITEPRESS_CONFIG = { srcDir: docs, markdown: {}, site: { base: '/from-c-to-mcu/' }, cleanUrls: true };
  const loader = createCourseGraphLoader({ routesFile });
  assert.ok(loader.watch.includes(routesFile.replace(/\\/g, '/')));
  assert.ok(loader.watch.some(pattern => pattern.endsWith('/**/*.md')));
  const initial = await loader.load();
  assert.deepEqual(initial.nodes.map(item => item.id), ['/', '/notes/a'], JSON.stringify({ root, watch: loader.watch }));
  assert.deepEqual(initial.links, [edge('/', '/notes/a')]);
  await writeFile(path.join(docs, 'notes/b.md'), '# B\n\n[back](/)');
  await writeFile(routesFile, JSON.stringify({ Alias: { link: '/notes/b' } }));
  const changed = await loader.load();
  assert.deepEqual(changed.nodes.map(item => item.id), ['/', '/notes/a', '/notes/b']);
  assert.deepEqual(changed.links, [edge('/', '/notes/b'), edge('/notes/b', '/')]);
  await writeFile(path.join(docs, 'notes/a.md'), '# Edited A\n\n[b](b.md)');
  const edited = await loader.load();
  assert.equal(edited.nodes.find(item => item.id === '/notes/a').title, 'Edited A');
  assert.deepEqual(targets(edited, '/notes/a'), ['/notes/b']);
  await rm(path.join(docs, 'notes/b.md'));
  const removed = await loader.load();
  assert.deepEqual(removed.nodes.map(item => item.id), ['/', '/notes/a']);
  assert.deepEqual(removed.links, []);
  assert.equal(JSON.stringify(removed).includes('src'), false);
});

test('real public docs load as a compact graph without pending-topic or self edges', async t => {
  const docs = fileURLToPath(new URL('../docs/', import.meta.url));
  const previousConfig = globalThis.VITEPRESS_CONFIG;
  t.after(() => { globalThis.VITEPRESS_CONFIG = previousConfig; });
  globalThis.VITEPRESS_CONFIG = { srcDir: docs, markdown: {}, site: { base: '/from-c-to-mcu/' }, cleanUrls: true };
  const graph = await createCourseGraphLoader().load();
  const ids = new Set(graph.nodes.map(item => item.id));
  assert.ok(ids.has('/lessons/1-0'));
  assert.ok(ids.has('/notes/mcu-architecture'));
  assert.equal(graph.nodes.find(item => item.id === '/optional').url, '/optional/');
  assert.ok(graph.links.every(link => ids.has(link.source) && ids.has(link.target) && link.source !== link.target));
  assert.equal(targets(graph, '/lessons/1-1').includes('/topics'), false);
  const source = await readFile(new URL('../docs/.vitepress/theme/local-graph.mjs', import.meta.url), 'utf8');
  assert.doesNotMatch(source, /node:|from ['"](?:vitepress|gray-matter|linkedom|markdown-it)['"]/);
  assert.ok(JSON.stringify(graph).length < 60000);
  t.diagnostic(JSON.stringify({ nodes: graph.nodes.length, links: graph.links.length, bytes: Buffer.byteLength(JSON.stringify(graph)) }));
});

test('VitePress transforms the data entry into a compact client export', async t => {
  const { createServer } = await import('vitepress');
  const docs = fileURLToPath(new URL('../docs/', import.meta.url));
  const previousConfig = globalThis.VITEPRESS_CONFIG;
  const server = await createServer(docs, { middlewareMode: true });
  t.after(async () => {
    await server.close();
    globalThis.VITEPRESS_CONFIG = previousConfig;
  });
  const result = await server.transformRequest('/.vitepress/course-graph.data.ts');
  assert.ok(result?.code.includes('export const data = JSON.parse('));
  assert.doesNotMatch(result.code, /includeSrc|node:fs|createContentLoader|frontmatter|sourceKey/);
  assert.ok(result.code.includes('/lessons/1-0'));
});
