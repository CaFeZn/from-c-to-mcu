import MarkdownIt from 'markdown-it';

const parser = new MarkdownIt({ html: true });
const demos = new Map([
  ['【原理】单片机内部结构', { 'mcu-bus': 'McuArchitectureLab', 'flash-execution': 'FlashExecutionLab' }],
  ['【原理】寄存器与内存映射', { 'address-decode': 'AddressDecodeLab', 'register-write': 'RegisterWriteLab' }],
]);

export function demoMarkup(component) {
  return '\n<div class="demo-slot">\n<ClientOnly>\n<' + component + ' />\n</ClientOnly>\n</div>\n';
}

export function injectCourseDemos(body, stem) {
  const expected = demos.get(stem) || {};
  if (!Object.keys(expected).length && !body.includes('course-demo:')) return body;
  const markers = [];
  for (const token of parser.parse(body, {})) {
    if (token.type !== 'html_block') continue;
    const match = token.content.trim().match(/^<!--\s*course-demo:\s*([a-z0-9-]+)\s*-->$/);
    if (match) markers.push({ id: match[1], start: token.map[0], end: token.map[1] });
  }
  for (const marker of markers) {
    if (!Object.hasOwn(expected, marker.id)) throw new Error('Unknown course demo ' + marker.id + ' in ' + stem);
  }
  for (const id of Object.keys(expected)) {
    if (markers.filter(marker => marker.id === id).length !== 1) {
      throw new Error('Expected exactly one course demo ' + id + ' in ' + stem);
    }
  }
  // Token line ranges prevent examples inside fenced code from becoming live components.
  const lines = body.split('\n');
  for (const marker of markers.sort((a, b) => b.start - a.start)) {
    lines.splice(marker.start, marker.end - marker.start, demoMarkup(expected[marker.id]).trim());
  }
  return lines.join('\n');
}
