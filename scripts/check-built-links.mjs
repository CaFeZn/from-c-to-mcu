import fs from 'node:fs/promises';
import path from 'node:path';
import { parseHTML } from 'linkedom';
const root=path.resolve('docs/.vitepress/dist'),base='/from-c-to-mcu/';
async function walk(dir){const out=[];for(const e of await fs.readdir(dir,{withFileTypes:true})){const p=path.join(dir,e.name);if(e.isDirectory())out.push(...await walk(p));else if(p.endsWith('.html'))out.push(p)}return out}
const pages=await walk(root),cache=new Map(),issues=[];
for(const file of pages)cache.set(file,parseHTML(await fs.readFile(file,'utf8')).document);
async function exists(p){try{return (await fs.stat(p)).isFile()}catch{return false}}
for(const [file,doc]of cache){
  const relative=path.relative(root,file).replaceAll('\\','/');
  const current=new URL(base+relative.replace(/index\.html$/,'').replace(/\.html$/,''),'https://cafezn.github.io');
  for(const el of doc.querySelectorAll('a[href],img[src],script[src],link[href]')){
    const attr=el.tagName==='IMG'||el.tagName==='SCRIPT'?'src':'href';const raw=el.getAttribute(attr);
    if(!raw||/^(data:|mailto:|tel:|javascript:)/.test(raw))continue;
    let url;try{url=new URL(raw,current)}catch{issues.push({page:relative,raw,reason:'invalid URL'});continue}
    if(url.origin!==current.origin)continue;
    if(!url.pathname.startsWith(base)){issues.push({page:relative,raw,reason:'outside project base'});continue}
    const target=decodeURIComponent(url.pathname.slice(base.length));
    let targetFile=path.join(root,target||'index.html');
    if(!await exists(targetFile))targetFile=target.endsWith('/')?path.join(root,target,'index.html'):path.join(root,target+'.html');
    if(!await exists(targetFile)){issues.push({page:relative,raw,reason:'missing target'});continue}
    if(url.hash&&targetFile.endsWith('.html')){
      const targetDoc=cache.get(targetFile)||parseHTML(await fs.readFile(targetFile,'utf8')).document;
      const id=decodeURIComponent(url.hash.slice(1));
      if(id&&!targetDoc.getElementById(id))issues.push({page:relative,raw,reason:'missing anchor '+id});
    }
  }
}
console.log(JSON.stringify({pages:pages.length,issues},null,2));
if(issues.length)process.exitCode=1;
