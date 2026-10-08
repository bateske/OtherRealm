// Deterministic UI captures and transitions, in an isolated browser profile.
import {createRequire} from 'node:module';
import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
const require=createRequire(import.meta.url);
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||path.resolve('../CH32EMU/node_modules/playwright'));
const browser=await chromium.launch({channel:'msedge',headless:true});
const page=await browser.newPage();const errors=[];
page.on('pageerror',e=>errors.push(e.message));
fs.mkdirSync('build/menu-fade',{recursive:true});
const pixels=()=>page.evaluate(()=>Array.from(mod.HEAPU8.slice(mod._or_screen(),mod._or_screen()+32768)));
const shot=async name=>fs.writeFileSync(`build/menu-fade/${name}.png`,Buffer.from((await page.locator('canvas').evaluate(c=>c.toDataURL())).split(',')[1],'base64'));
const at=async t=>page.evaluate(t=>{mod._or_animate(t);draw()},t);
const tap=async b=>page.evaluate(b=>{mod._or_buttons(b);mod._or_buttons(0)},b);
const load=async()=>{
 await page.goto('http://127.0.0.1:7788/web/');await page.waitForFunction(()=>typeof mod!=='undefined'&&mod);
 await page.locator('#file').setInputFiles(path.resolve('private/handheld/OTHERWRL.PAK'));
 await page.waitForFunction(()=>mod._or_title());await page.evaluate(()=>running=false);
};
try{
 await load();await at(15360);await shot('title-default');const normal=await pixels();
 await tap(1);await tap(64);await at(16000);await tap(8);await tap(1);await at(17000);
 assert.equal(await page.evaluate(()=>mod._or_part()),16002);
 await page.evaluate(()=>{for(let i=0;i<105;i++)mod._or_buttons(5);mod._or_buttons(0);draw()});
 const playing=await pixels();await at(17200);await tap(64);await at(17248);await shot('opening-fade');
 await at(18000);await shot('idle-0');const footer=(await pixels()).slice(109*256,117*256);
 for(const t of [20000,22000,24000]){await at(t);await shot('idle-'+t);assert.deepEqual((await pixels()).slice(109*256,117*256),footer,'checkpoint text is static')}
 await tap(8);await at(24120);await shot('selection-dust');
 for(let i=0;i<2;i++)await tap(8);
 await at(24800);
 await tap(1);await at(24920);await shot('sound-burst');
 assert.equal(await page.evaluate(()=>mod._or_sound()),0);
 await at(26000);assert.equal(await page.evaluate(()=>mod._or_menu()),1,'sound stays in menu');
 await tap(2);await at(26120);await shot('confirm-burst');
 await at(26432);await shot('confirm-selected');
 assert.equal(await page.evaluate(()=>mod._or_menu()),1,'selection lingers before closing');
 await at(26720);await shot('confirm-fade');await at(26960);
 assert.equal(await page.evaluate(()=>mod._or_menu()),0);
 assert.deepEqual(await pixels(),playing,'all paused pixels restored without a black intermediate clear');
 await page.evaluate(()=>mod._or_buttons(64));await at(27000);await at(28500);await shot('hold-exit');
 assert.equal(await page.evaluate(()=>mod._or_hold()),48);assert.equal(await page.evaluate(()=>mod._or_exit()),0);
 await page.evaluate(()=>mod._or_buttons(0));await at(28600);assert.equal(await page.evaluate(()=>mod._or_hold()),0,'release cancels hold');
 const key=await page.evaluate(()=>'otherrealm.save.v1.'+(mod._or_identity()>>>0));
 const saved=await page.evaluate(k=>localStorage.getItem(k),key);
 await load();await at(15360);assert.deepEqual(await pixels(),normal,'ordinary save still uses shoreline');
 await tap(1);await at(16000);await tap(2);assert.equal(await page.evaluate(()=>mod._or_title()),1,'B backs out to title');
 assert.equal(await page.evaluate(k=>localStorage.getItem(k),key),saved,'B never changes saved progress');
 // Only this isolated browser profile gets a synthetic completed record.
 await page.evaluate(k=>{const s=JSON.parse(localStorage.getItem(k));s[10]=1;localStorage.setItem(k,JSON.stringify(s))},key);
 await load();await at(15360);await shot('title-unlocked');
 assert.equal(await page.evaluate(()=>mod._or_completed()),1);assert.notDeepEqual(await pixels(),normal);
 assert.deepEqual(errors,[]);
 console.log('PASS menu visuals: two titles, B-to-title, fixed footer, fade/idle/dust/poof captures, hold indicator, sound stays open, delayed selection, exact resume, no browser errors.');
}finally{await browser.close()}
