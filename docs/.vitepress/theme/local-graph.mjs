const fallbackOrigin = 'https://course.invalid';

export function normalizePageId(path, base = '/from-c-to-mcu/') {
  if (typeof path !== 'string' || !path.trim()) return '/';
  let pathname;
  try {
    pathname = new URL(path.trim(), fallbackOrigin).pathname;
  } catch {
    return '/';
  }
  // Decode filenames, but keep URL delimiters escaped so IDs remain reusable.
  pathname = pathname.split('/').map(segment => {
    try {
      return decodeURIComponent(segment).replace(/[%/?#]/g, char => encodeURIComponent(char));
    } catch {
      return segment;
    }
  }).join('/');
  const prefix = '/' + base.split('/').filter(Boolean).join('/');
  if (prefix !== '/' && (pathname === prefix || pathname.startsWith(prefix + '/'))) {
    pathname = pathname.slice(prefix.length) || '/';
  }
  pathname = pathname.replace(/\/{2,}/g, '/').replace(/\.(?:html|md)$/i, '').replace(/\/+$/, '');
  pathname = pathname.replace(/(?:^|\/)index$/i, '');
  return pathname || '/';
}

export function selectLocalGraph(graph, currentId) {
  const id = normalizePageId(currentId);
  const allNodes = new Map((graph?.nodes || []).map(node => [node.id, node]));
  if (!allNodes.has(id)) return { current: null, nodes: [], links: [], outgoing: [], incoming: [] };

  const outgoing = new Set();
  const incoming = new Set();
  const uniqueLinks = new Map();
  for (const link of graph.links || []) {
    const { source, target } = link;
    if (source === target || !allNodes.has(source) || !allNodes.has(target)) continue;
    uniqueLinks.set(JSON.stringify([source, target]), { source, target });
    if (source === id) outgoing.add(target);
    if (target === id) incoming.add(source);
  }
  const visible = new Set([id, ...outgoing, ...incoming]);
  const nodes = [...visible].map(key => ({ ...allNodes.get(key) }));
  const links = [...uniqueLinks.values()].filter(link => visible.has(link.source) && visible.has(link.target));
  return { current: nodes[0], nodes, links, outgoing: [...outgoing].sort(), incoming: [...incoming].sort() };
}
