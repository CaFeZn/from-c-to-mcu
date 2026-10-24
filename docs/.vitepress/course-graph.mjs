import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
import MarkdownIt from 'markdown-it';
import matter from 'gray-matter';
import { parseHTML } from 'linkedom';
import { createContentLoader } from 'vitepress';
import { courseMarkdown } from './course-markdown.mjs';
import { normalizePageId } from './theme/local-graph.mjs';

const defaultBase = '/from-c-to-mcu/';
const defaultOrigin = 'https://cafezn.github.io';
const kinds = { lessons: 'lesson', chapters: 'chapter', notes: 'note', optional: 'optional', labs: 'lab', topics: 'topic', resources: 'resource' };

function pageUrl(url, base) {
  const pathname = new URL(url, defaultOrigin).pathname;
  const id = normalizePageId(pathname, base);
  return id === '/' ? '/' : id + (/\/$|\/index(?:\.(?:html|md))?$/i.test(pathname) ? '/' : '');
}

function documentFor(html) {
  return parseHTML('<!doctype html><html><body>' + html + '</body></html>').document;
}

export function buildCourseGraph(pages, { routes = {}, base = defaultBase, origin = defaultOrigin } = {}) {
  const site = new URL(origin);
  const basePath = '/' + base.split('/').filter(Boolean).join('/') + '/';
  const prefix = basePath === '//' ? '/' : basePath;
  const records = new Map();
  for (const page of pages) {
    if (typeof page.src !== 'string') continue;
    const parsed = matter(page.src);
    const frontmatter = { ...parsed.data, ...page.frontmatter };
    const url = pageUrl(page.url, base);
    const id = normalizePageId(url, base);
    if (records.has(id)) continue;
    const title = typeof frontmatter.title === 'string' ? frontmatter.title : '';
    records.set(id, { content: parsed.content, frontmatter, node: { id, url, title, label: '', kind: kinds[id.split('/')[1]] || 'page' } });
  }

  const aliases = Object.assign(Object.create(null), routes);
  for (const { frontmatter, node } of records.values()) {
    if (frontmatter.sourceKey && !Object.hasOwn(aliases, frontmatter.sourceKey)) {
      aliases[frontmatter.sourceKey] = { link: node.url };
    }
  }

  function relativeMarkdownLink(raw, source) {
    if (/^(?:[a-z][a-z\d+.-]*:|\/|#|\?)/i.test(raw)) return { href: raw };
    let url;
    try {
      url = new URL(raw, new URL(prefix + source.url.slice(1), site));
    } catch {
      return { href: '' };
    }
    if (!/\.md$/i.test(url.pathname)) return { href: raw };
    if (!records.has(normalizePageId(url.pathname, base))) {
      let name;
      try { name = decodeURIComponent(url.pathname.split('/').at(-1)).replace(/\.md$/i, ''); } catch { return { href: '' }; }
      const alias = aliases[name];
      if (alias) return { href: alias.link + (url.hash && !alias.link.includes('#') ? url.hash : ''), pending: alias.pending };
    }
    // Public relative .md links must not be remapped by a basename-only alias.
    return { href: url.pathname + url.search + url.hash };
  }

  function targetId(raw, source) {
    const { href, pending } = relativeMarkdownLink(raw, source);
    if (!href || pending || href.startsWith('#') || href.startsWith('?')) return null;
    let url;
    try { url = new URL(href, new URL(prefix + source.url.slice(1), site)); } catch { return null; }
    if (!/^https?:$/.test(url.protocol) || url.origin !== site.origin) return null;
    const absolute = /^(?:[a-z][a-z\d+.-]*:|\/\/)/i.test(href);
    if (absolute && prefix !== '/' && url.pathname !== prefix.slice(0, -1) && !url.pathname.startsWith(prefix)) return null;
    const id = normalizePageId(url.pathname, base);
    return id !== source.id && records.has(id) ? id : null;
  }

  const links = new Map();
  for (const { content, frontmatter, node } of records.values()) {
    const md = new MarkdownIt({ html: true }).use(courseMarkdown, { routes: aliases });
    md.core.ruler.before('course_local_links', 'course_graph_relative_links', state => {
      for (const token of state.tokens) {
        for (const child of token.children || []) {
          if (child.type !== 'link_open') continue;
          const result = relativeMarkdownLink(child.attrGet('href') || '', node);
          child.attrSet('href', result.href);
          if (result.pending) child.attrSet('data-pending', 'true');
        }
      }
    });
    const document = documentFor(md.render(content));
    node.title ||= document.querySelector('h1')?.textContent.trim() || node.id.split('/').at(-1) || 'Home';
    const lesson = String(frontmatter.lesson || '');
    node.label = lesson && !node.title.startsWith(lesson + ' ') ? lesson + ' ' + node.title : node.title;
    for (const anchor of document.querySelectorAll('a[href]')) {
      if (anchor.closest('[data-pending], pre, code, nav, script, style, template')) continue;
      const target = targetId(anchor.getAttribute('href').trim(), node);
      if (target) links.set(JSON.stringify([node.id, target]), { source: node.id, target });
    }
  }
  return {
    nodes: [...records.values()].map(record => record.node).sort((a, b) => a.id.localeCompare(b.id)),
    links: [...links.values()].sort((a, b) => a.source.localeCompare(b.source) || a.target.localeCompare(b.target))
  };
}

export function createCourseGraphLoader({ routesFile = fileURLToPath(new URL('./routes.json', import.meta.url)), base = defaultBase, origin = defaultOrigin } = {}) {
  const content = createContentLoader('**/*.md', {
    includeSrc: true,
    globOptions: {
      ignore: ['**/node_modules/**', '**/dist/**', '**/.vitepress/**', '**/public/**', '**/authoring/**', '**/artifacts/**'],
      followSymbolicLinks: false
    },
    async transform(pages) {
      const routes = JSON.parse(await readFile(routesFile, 'utf8'));
      return buildCourseGraph(pages, { routes, base, origin });
    }
  });
  return {
    watch: [...(Array.isArray(content.watch) ? content.watch : [content.watch]), routesFile.replace(/\\/g, '/')],
    // Re-glob public pages on every run; the watcher list also contains routes.json.
    load: () => content.load()
  };
}
