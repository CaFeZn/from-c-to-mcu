import test from 'node:test';
import assert from 'node:assert/strict';
import { demoMarkup, injectCourseDemos } from '../scripts/course-demos.mjs';

const architecture = '【原理】单片机内部结构';
const memory = '【原理】寄存器与内存映射';
const source = '# Example\n\nBefore\n\n<!-- course-demo: mcu-bus -->\n\nBetween\n\n<!-- course-demo: flash-execution -->\n\nAfter\n';
test('principle demos appear at explicit paragraph positions', () => {
  const out = injectCourseDemos(source, architecture);
  assert.match(out, /Before\n\n<div class="demo-slot">/);
  assert.match(out, /<McuArchitectureLab \/>/);
  assert.match(out, /<FlashExecutionLab \/>/);
  assert.ok(out.indexOf('McuArchitectureLab') < out.indexOf('Between'));
  assert.ok(out.indexOf('Between') < out.indexOf('FlashExecutionLab'));
  assert.ok(out.endsWith('After\n'));
});
test('MMIO markers use the matching components', () => {
  const out = injectCourseDemos('<!-- course-demo: address-decode -->\n\n<!-- course-demo: register-write -->\n', memory);
  assert.match(out, /<AddressDecodeLab \/>/);
  assert.match(out, /<RegisterWriteLab \/>/);
});
test('fenced examples stay literal', () => {
  const fence = String.fromCharCode(96).repeat(3);
  const literal = fence + 'html\n<!-- course-demo: mcu-bus -->\n' + fence + '\n\n';
  const out = injectCourseDemos(literal + source, architecture);
  assert.ok(out.startsWith(literal));
  assert.equal((out.match(/<McuArchitectureLab \/>/g) || []).length, 1);
});
test('missing, duplicate and unknown markers fail before publication', () => {
  assert.throws(() => injectCourseDemos('', architecture), /exactly one/);
  assert.throws(() => injectCourseDemos(source + '\n<!-- course-demo: mcu-bus -->\n', architecture), /exactly one/);
  assert.throws(() => injectCourseDemos('<!-- course-demo: surprise -->\n', 'Other'), /Unknown/);
});
test('unrelated prose and existing demo markup remain unchanged', () => {
  assert.equal(injectCourseDemos('# Plain\n\nText\n', 'Other'), '# Plain\n\nText\n');
  assert.match(demoMarkup('BitsLab'), /<ClientOnly>\n<BitsLab \/>\n<\/ClientOnly>/);
});
