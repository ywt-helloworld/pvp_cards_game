import test from 'node:test';
import assert from 'node:assert/strict';
import { resolve } from 'node:path';
import { setTimeout as sleep } from 'node:timers/promises';
import { io } from 'socket.io-client';
import { createGateway } from '../server/server.mjs';
import { command } from '../server/commands.mjs';

test('gateway accepts only typed operations, never actor or system events', () => {
  assert.equal(command({kind:'play',turn:5,cards:[1,53],actor:3}), 'PLAY 5 1 53');
  for (const data of [{kind:'SCORE'}, {kind:'play',turn:1,cards:[1,1]},
    {kind:'done',shop:'1\nNEW'}, {kind:'upgrade',shop:1,pattern:5}]) assert.throws(()=>command(data));
});
async function until(fn, description, ms=10000) {
  const end=Date.now()+ms;
  while(Date.now()<end) {if(fn())return;await sleep(20);}
  throw Error(`Timed out: ${description}`);
}
test('four browsers: rooms, private shop, buy, deal, play, reconnect and isolation', {timeout:40000}, async () => {
  const app=await createGateway({engine:process.env.CARDGAME_ENGINE||resolve('../build/cardgame_server.exe'),port:0,shopTime:3600,turnTime:3600,seed:42,disconnectGrace:20});
  const clients=[];
  const connect=async()=>{
    const socket=io(`http://127.0.0.1:${app.address.port}`,{transports:['websocket'],forceNew:true,reconnection:false});
    const c={socket,state:null};socket.on('state',s=>c.state=s);clients.push(c);
    await until(()=>socket.connected,'connect');return c;
  };
  const send=(c,event,data)=>c.socket.timeout(10000).emitWithAck(event,data);
  try {
    const health=await fetch(`http://127.0.0.1:${app.address.port}/healthz`);assert.equal((await health.json()).ok,true);
    const first=await connect();let r=await send(first,'join',{name:'Alice',create:true});assert.equal(r.ok,true);first.session=r;
    const room=r.room;
    for (const name of ['Bob','Carol','Dave']) {const c=await connect();c.session=await send(c,'join',{name,room});assert.equal(c.session.ok,true);}
    await until(()=>clients.slice(0,4).every(c=>c.state?.phase==='shopping'),'all shops');
    const s=first.state;
    assert.equal(s.players.filter(p=>p.coins===null).length,3);assert.equal(s.self.functionSlots,4);assert.ok(s.remainingMs>0);
    const offer=s.shop.offers.find(o=>o.price<=6);
    assert.equal((await send(first,'action',{kind:'buy',shop:s.shop.id,slot:offer.slot,product:offer.product})).ok,true);
    await until(()=>first.state.players[first.state.seat].cards.length===1,'bought card');
    const me=first.state.players[first.state.seat];assert.equal(me.coins,6-offer.price);
    for(const c of clients.slice(0,4)) assert.equal((await send(c,'action',{kind:'done',shop:c.state.shop.id})).ok,true);
    await until(()=>clients.slice(0,4).every(c=>c.state?.phase==='playing'),'initial deal');
    for(const c of clients.slice(0,4)){assert.equal(c.state.hand.length,7);assert.equal(c.state.players.filter(p=>p.hand!==undefined).length,0);}
    const acting=clients.find(c=>c.state.seat===first.state.acting);
    const card=acting.state.hand.find(c=>!c.joker);
    const oldTurn=acting.state.turn;
    assert.equal((await send(acting,'action',{kind:'play',turn:oldTurn,cards:[card.id]})).ok,true);
    await until(()=>first.state.turn!==oldTurn,'play committed');assert.ok(first.state.table.includes(card.id));
    const notActing=clients.find(c=>c.state.seat!==first.state.acting && c!==acting);
    assert.equal((await send(notActing,'action',{kind:'pass',turn:first.state.turn})).ok,false);
    const current=clients.find(c=>c.state.seat===first.state.acting);
    assert.equal((await send(current,'action',{kind:'pass',turn:current.state.turn})).ok,true);
    const privateClient=clients.find(c=>c.state.seat!==first.state.acting);
    const savedHand=privateClient.state.hand.map(c=>c.id), token=privateClient.session.token, seat=privateClient.state.seat;
    privateClient.socket.disconnect();await sleep(200);
    const resumed=await connect();r=await send(resumed,'resume',{token});assert.equal(r.ok,true);assert.equal(r.seat,seat);
    await until(()=>resumed.state?.players[seat].connected,'reconnect');assert.deepEqual(resumed.state.hand.map(c=>c.id),savedHand);
    const fifth=await connect();assert.equal((await send(fifth,'join',{name:'Intruder',room})).ok,false);
    assert.equal((await send(fifth,'action',{kind:'pass',turn:first.state.turn})).ok,false);
    const secondRoom=await send(fifth,'join',{name:'Other room',create:true});assert.equal(secondRoom.ok,true);assert.notEqual(secondRoom.room,room);
    assert.equal(app.rooms.size,2);
    const peer=await connect();assert.equal((await send(peer,'resume',{token})).ok,false,'no live-seat hijack');
  } finally {for(const c of clients)c.socket.disconnect();await app.close();}
});
