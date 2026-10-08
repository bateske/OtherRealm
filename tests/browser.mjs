// Local browser integration checks. Runs silently in an isolated browser profile.
import {createRequire} from 'node:module';
import assert from 'node:assert/strict';
import path from 'node:path';
const require=createRequire(import.meta.url);
const {chromium}=require(process.env.PLAYWRIGHT_MODULE||path.resolve('../CH32EMU/node_modules/playwright'));
const browser=await chromium.launch({channel:'msedge',headless:true});
const page=await browser.newPage({viewport:{width:1200,height:930}}),errors=[];
page.on('pageerror',e=>errors.push(e.message));
const press=async key=>{await page.keyboard.press(key);await page.waitForTimeout(950);};
try{
 await page.goto('http://127.0.0.1:7788/web/');
 await page.getByRole('button',{name:'Play Signal Grove'}).click();
 await page.waitForFunction(()=>mod._or_title());
 await press('Space');await page.waitForFunction(()=>mod._or_saved());
 const before=await page.locator('canvas').evaluate(c=>c.toDataURL());
 await page.keyboard.down('ArrowRight');await page.waitForTimeout(600);await page.keyboard.up('ArrowRight');
 const after=await page.locator('canvas').evaluate(c=>c.toDataURL());assert.notEqual(before,after,'keyboard movement changes visible frame');
 await press('Enter');assert.equal(await page.evaluate(()=>mod._or_menu()),1);
 const frames=await page.evaluate(()=>mod._or_frames());await page.waitForTimeout(150);
 assert.equal(await page.evaluate(()=>mod._or_frames()),frames,'menu pauses VM');
 await page.screenshot({path:'build/browser-menu.png'});
 await press('Escape');assert.equal(await page.evaluate(()=>mod._or_menu()),0);
 const saved=await page.evaluate(()=>localStorage.getItem('otherrealm.save.v1.'+(mod._or_identity()>>>0)));assert.ok(saved);
 await page.reload();await page.getByRole('button',{name:'Play Signal Grove'}).click();
 await page.waitForFunction(()=>mod._or_title());await press('Space');await press('Space');
 assert.equal(await page.evaluate(()=>mod._or_menu()),0,'continue after browser reload');
 await page.locator('#file').setInputFiles(path.resolve('private/handheld/OTHERWRL.PAK'));
 await page.waitForFunction(()=>!isDemo&&mod._or_title());
 await page.screenshot({path:'build/browser-title.png'});await press('Space');
 await press('Enter');await press('ArrowDown');await press('Space'); // Skip Intro after starting a fresh game from its title.
 await page.waitForFunction(()=>mod._or_part()===16002&&mod._or_saved());
 await page.keyboard.down('ArrowUp');await page.keyboard.down(' ');await page.waitForTimeout(1100);await page.keyboard.up(' ');await page.keyboard.up('ArrowUp');
 await press('Enter');await page.screenshot({path:'build/browser-private-menu.png'});
 // New Game requires explicit confirmation and cancellation keeps the saved point.
 const saveKey=await page.evaluate(()=>'otherrealm.save.v1.'+(mod._or_identity()>>>0));
 const retained=await page.evaluate(k=>localStorage.getItem(k),saveKey);
 await press('ArrowDown');await press('ArrowDown');await press('Space');await press('Escape');
 assert.equal(await page.evaluate(k=>localStorage.getItem(k),saveKey),retained);
 await page.setViewportSize({width:430,height:800});assert.ok(await page.evaluate(()=>document.documentElement.scrollWidth<=innerWidth));
 const denied=await browser.newPage();
 await denied.addInitScript(()=>{Storage.prototype.setItem=function(){throw new DOMException('Test storage failure','QuotaExceededError')};});
 await denied.goto('http://127.0.0.1:7788/web/');await denied.getByRole('button',{name:'Play Signal Grove'}).click();
 await denied.waitForFunction(()=>mod._or_title());await denied.keyboard.press('Space');
 await denied.waitForFunction(()=>document.querySelector('#status').textContent.includes('RAM only'));
 assert.equal(await denied.evaluate(()=>mod._or_save_failed()),1);await denied.close();
 assert.deepEqual(errors,[]);console.log('PASS browser: movement, Start menu, paused VM, persistent reload/continue, title on every boot, intro/skip, private adapted pack, new-game confirmation, storage failure notice, responsive layout, no errors.');
}catch(e){console.error('UI status:',await page.locator('#status').textContent());console.error('Browser errors:',errors);await page.screenshot({path:'build/browser-failure.png'});throw e;}finally{await browser.close()}
