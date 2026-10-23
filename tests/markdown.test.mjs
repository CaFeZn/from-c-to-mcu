import test from 'node:test';
import assert from 'node:assert/strict';
import MarkdownIt from 'markdown-it';
import { courseMarkdown } from '../docs/.vitepress/course-markdown.mjs';
const routes = { '1.0_搭建环境': { link: '/lessons/1-0' }, '原理': { link: '/topics/#pending', pending: true } };
const render = text => new MarkdownIt({ html: true }).use(courseMarkdown, { routes }).render(text);
test('wiki aliases resolve without altering code', () => {
  const html = render('[[1.0_搭建环境|开始]] and `[[unknown]]`');
  assert.match(html, /href="\/lessons\/1-0">开始/);
  assert.match(html, /<code>\[\[unknown\]\]<\/code>/);
});
test('relative markdown links and anchors resolve', () => {
  assert.match(render('[开始](../chapter/1.0_搭建环境.md#第一步)'), /href="\/lessons\/1-0#第一步"/);
});
test('callouts retain body, nested answers and code', () => {
  const html = render('> [!tip]- 自测\n> 正文\n>\n> > [!success]- 答案\n> > 答案正文\n> >\n> > ```c\n> > printf("hi");\n> > ```');
  assert.equal((html.match(/<details /g) || []).length, 2);
  assert.match(html, /正文/); assert.match(html, /答案正文/); assert.match(html, /language-c/);
  assert.doesNotMatch(html, /\[!success\]/);
});
test('pending links are explicitly marked', () => assert.match(render('[[原理]]'), /data-pending="true"/));
test('unknown links fail visibly', () => assert.throws(() => render('[[missing]]'), /Unmapped/));
