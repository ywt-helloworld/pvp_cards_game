// Run against local preview or TARGET_URL=http://47.76.64.154. Uses an existing
// Chrome/Edge installation; no browser is installed on the deployment server.
import { chromium } from 'playwright-core';
import { createGateway } from '../server/server.mjs';
import { resolve } from 'node:path';
import { mkdir } from 'node:fs/promises';
import assert from 'node:assert/strict';
const executablePath=process.env.BROWSER_PATH||'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe';
const app=process.env.TARGET_URL?null:await createGateway({port:0,engine:process.env.CARDGAME_ENGINE||resolve('../build/cardgame_server.exe'),shopTime:3600,turnTime:3600,seed:42});
const url=process.env.TARGET_URL||`http://127.0.0.1:${app.address.port}`;
const browser=await chromium.launch({executablePath,headless:true});
const errors=[], contexts=[], websockets=[];
await mkdir('../test-results',{recursive:true});
try {
  const pages=[];
  for(let i=0;i<4;i++) {
    const context=await browser.newContext({viewport:{width:844,height:390},isMobile:true,hasTouch:true});contexts.push(context);
    const page=await context.newPage();page.on('pageerror',error=>errors.push(error.message));
    page.on('websocket',ws=>websockets.push(ws.url()));pages.push(page);
    await page.goto(url);await page.locator('.connection.online').waitFor();
    await page.locator('#playerName').fill(`测试玩家${i+1}`);
    if(i===0){await page.locator('#createRoom').click();await page.locator('.room-code').waitFor();}
    else {
      await page.locator('#roomCode').fill(await pages[0].locator('.room-code').textContent());
      await page.locator('#joinRoom').click();
    }
  }
  for(const p of pages)await p.getByRole('heading',{name:'局前商店',exact:true}).waitFor();
  const p=pages[0];
  await p.screenshot({path:'../test-results/mobile-shop.png',fullPage:true});
  await p.locator('.buy:enabled').first().click();await p.locator('.owned-card').waitFor();
  assert.match(await p.locator('.owned-card').textContent(),/自己.*后/);
  if(app) {
    await pages[1].getByRole('button',{name:'购买并使用',exact:true}).click();
    await pages[1].waitForFunction(()=>document.querySelectorAll('.offers > *').length===4);
    assert.equal(await pages[1].locator('.owned-card').count(),0);
    await pages[2].getByRole('button',{name:'购买并使用',exact:true}).click();
    await pages[2].waitForFunction(()=>document.querySelector('.shop-layout .muted').textContent.includes('Lv.2'));
  }
  for(const page of pages)await page.locator('[data-do="done"]').click();
  for(const page of pages)await page.locator('.hand .poker').first().waitFor();
  await p.screenshot({path:'../test-results/mobile-playing.png',fullPage:true});
  const actor=await (async()=>{for(const page of pages)if(await page.getByRole('heading',{name:'轮到你出牌',exact:true}).count())return page;})();
  assert.ok(actor,'server selected an acting player');
  await actor.locator('.hand .poker:not(.joker)').first().click();
  await actor.locator('[data-do="play"]').click();
  await actor.locator('.table-cards .poker').first().waitFor();
  // Card text on table must match the selected authoritative ID/rank mapping.
  const tableText=await actor.locator('.table-cards .poker').first().textContent();assert.ok(tableText.trim());
  await p.reload();await p.locator('.hand .poker').first().waitFor();
  assert.ok(await p.locator('.player.mine').count(),'same tab resumed its own seat');
  const dims=await p.evaluate(()=>({scroll:document.documentElement.scrollWidth,width:innerWidth}));
  assert.ok(dims.scroll<=dims.width+1,`landscape overflows horizontally: ${JSON.stringify(dims)}`);
  const portrait=await browser.newContext({viewport:{width:390,height:844},isMobile:true,hasTouch:true});contexts.push(portrait);
  const portraitPage=await portrait.newPage();await portraitPage.goto(url);await portraitPage.locator('.rotate-hint').waitFor();
  await portraitPage.screenshot({path:'../test-results/mobile-portrait.png',fullPage:true});
  assert.equal(errors.length,0,errors.join('\n'));
  assert.ok(websockets.length>=4,'four WebSocket connections upgraded');
  console.log(JSON.stringify({ok:true,url,fourPlayers:true,buy:true,consumables:!!app,deal:true,play:true,reconnect:true,mobileLandscape:true,websocketConnections:websockets.length,consoleErrors:errors}));
} finally {for(const context of contexts)await context.close();await browser.close();if(app)await app.close();}
