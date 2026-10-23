import path from 'node:path';
import { slugify } from '@mdit-vue/shared';

export function courseMarkdown(md, { routes = {} } = {}) {
  const escape = md.utils.escapeHtml;
  function resolve(raw) {
    const cleaned = decodeURIComponent(raw).replace(/\\\|/g, '|');
    const hashAt = cleaned.indexOf('#');
    const target = (hashAt < 0 ? cleaned : cleaned.slice(0, hashAt)).trim();
    const hash = hashAt < 0 ? '' : cleaned.slice(hashAt + 1);
    if (!target) return { href: '#' + slugify(hash) };
    const name = path.posix.basename(target.replace(/\\/g, '/')).replace(/\.md$/, '');
    const found = routes[name];
    if (!found) throw new Error('Unmapped course link: ' + raw);
    const href = found.link + (hash && !found.link.includes('#') ? '#' + slugify(hash) : '');
    return { ...found, href };
  }

  md.inline.ruler.before('link', 'course_wikilink', (state, silent) => {
    if (state.src.slice(state.pos, state.pos + 2) !== '[[') return false;
    const end = state.src.indexOf(']]', state.pos + 2);
    if (end < 0) return false;
    const inside = state.src.slice(state.pos + 2, end).replace(/\\\|/g, '|');
    if (inside.includes('\n')) return false;
    const pipe = inside.indexOf('|');
    const target = pipe < 0 ? inside : inside.slice(0, pipe);
    const label = pipe < 0 ? target.replace(/^.*#/, '') || target : inside.slice(pipe + 1);
    if (!silent) {
      const result = resolve(target);
      const open = state.push('link_open', 'a', 1);
      open.attrSet('href', result.href);
      if (result.pending) open.attrSet('data-pending', 'true');
      if (/^https?:/.test(result.href)) { open.attrSet('target', '_blank'); open.attrSet('rel', 'noreferrer noopener'); }
      const text = state.push('text', '', 0); text.content = label;
      state.push('link_close', 'a', -1);
    }
    state.pos = end + 2;
    return true;
  });

  md.core.ruler.after('inline', 'course_local_links', state => {
    for (const token of state.tokens) {
      for (const child of token.children || []) {
        if (child.type !== 'link_open') continue;
        const href = child.attrGet('href') || '';
        if (/^(https?:|mailto:|tel:|\/)/.test(href)) continue;
        if (!href.includes('.md') && !href.startsWith('#')) continue;
        const result = resolve(href);
        child.attrSet('href', result.href);
        if (result.pending) child.attrSet('data-pending', 'true');
      }
    }
  });

  md.core.ruler.after('course_local_links', 'course_callouts', state => {
    const stack = [];
    const pairs = [];
    state.tokens.forEach((token, i) => {
      if (token.type === 'blockquote_open') stack.push(i);
      if (token.type === 'blockquote_close') pairs.push([stack.pop(), i]);
    });
    for (const [start, end] of pairs) {
      const open = state.tokens[start];
      const paragraph = state.tokens[start + 1];
      const inline = state.tokens[start + 2];
      if (paragraph?.type !== 'paragraph_open' || inline?.type !== 'inline') continue;
      const firstNewline = inline.content.indexOf('\n');
      const header = firstNewline < 0 ? inline.content : inline.content.slice(0, firstNewline);
      const match = header.match(/^\[!([a-z]+)\]([+-])?\s*(.*)$/i);
      if (!match) continue;
      const allowed = ['tip', 'info', 'abstract', 'example', 'question', 'success', 'warning', 'failure', 'note'];
      const kind = allowed.includes(match[1].toLowerCase()) ? match[1].toLowerCase() : 'note';
      const title = match[3] || { tip: '提示', info: '补充', question: '先想一想', success: '对照结果', warning: '留意', failure: '检查', note: '笔记' }[kind] || '补充';
      const folded = Boolean(match[2]) || title.includes('跳读自测');
      open.type = 'html_block'; open.tag = ''; open.nesting = 0;
      open.content = folded ? '<details class="course-callout callout-' + kind + '"' + (match[2] === '+' ? ' open' : '') + '><summary>' + escape(title) + '</summary><div class="callout-body">\n' : '<aside class="course-callout callout-' + kind + '"><p class="callout-title">' + escape(title) + '</p><div class="callout-body">\n';
      const close = state.tokens[end]; close.type = 'html_block'; close.tag = ''; close.nesting = 0;
      close.content = folded ? '</div></details>\n' : '</div></aside>\n';
      inline.content = firstNewline < 0 ? '' : inline.content.slice(firstNewline + 1);
      inline.children = []; md.inline.parse(inline.content, md, state.env, inline.children);
      if (!inline.content) {
        paragraph.hidden = true; inline.hidden = true; state.tokens[start + 3].hidden = true;
      }
    }
  });
}
