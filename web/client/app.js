import { io } from 'socket.io-client';
const $ = id => document.getElementById(id);
const esc = value => String(value ?? '').replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const patterns = ['单张','对子','顺子','炸弹','同花顺'];
const suits = ['♠','♥','♣','♦'];
const rank = n => ({1:'A',11:'J',12:'Q',13:'K',14:'A',15:2}[n] || n);
const socket = io({ autoConnect:false, reconnection:true });
let state = null, joined = null, roster = null, selected = new Set(), busy = false, deadline = 0, choicePage = 0, lastChoice = null;
let resumeToken = sessionStorage.getItem('cardgame.session');
const entry = $('app').innerHTML;
const enhancementTargets = new Map();
let toastTimer;
function toast(message, bad = false) {
  $('toast').textContent = message; $('toast').classList.toggle('bad',bad); $('toast').hidden = false;
  clearTimeout(toastTimer); toastTimer = setTimeout(() => $('toast').hidden = true, 4000);
}
function errorText(message) {
  return ({'Insufficient coins':'金币不足','Not your turn':'还没轮到你','Invalid turn':'回合已变化，请重新选择',
    'Cannot pass while leading':'你是领出者，请出牌','No legal play':'所选手牌不能合法出牌',
    'Refresh limit':'本次商店已达到刷新上限','Upgrade maximum reached':'该牌型已加强到上限',
    'Invalid lock':'只能锁定一个有效货位','Function slots full':'功能牌槽位已满，请先出售',
    'Waiting for four players':'等待四位玩家加入'}[message] || message);
}
function cardEffect(d) {
  if (!(d >= 1 && d <= 10)) return '效果由服务器结算';
  const score = d%3 === 1 ? 1+Math.floor(d/4) : d%3 === 0 ? 1 : 0;
  const coins = d%3 === 2 || d%3 === 0 ? 1 : 0;
  return `${d===4||d===5?'自己不出后':'自己出牌后'}：${[score?`+${score} 分`:'',coins?`+${coins} 金币`:''].filter(Boolean).join('，')}`;
}
const cardName = d => `功能牌 ${d}`;
function resetEntry(message) {
  state = null; joined = null; roster = null; selected.clear(); resumeToken = null;
  sessionStorage.removeItem('cardgame.session'); $('app').innerHTML = entry;
  if ($('choiceDialog').open) $('choiceDialog').close();
  wireEntry(); if (message) $('joinError').textContent = message;
}
function remember(response) {
  joined = response; resumeToken = response.token; sessionStorage.setItem('cardgame.session',resumeToken);
  localStorage.setItem('cardgame.name',response.name); if (!state) renderLobby();
}
function wireEntry() {
  $('playerName').value = localStorage.getItem('cardgame.name') || '';
  const queryRoom = new URLSearchParams(location.search).get('room');
  if (queryRoom) $('roomCode').value = queryRoom;
  const join = async create => {
    const name = $('playerName').value.trim(), room = $('roomCode').value.trim();
    if (!name) { $('playerName').focus(); $('joinError').textContent = '先给自己起个名字'; return; }
    if (!socket.connected) { $('joinError').textContent = '正在连接服务器，请稍后再试'; return; }
    $('joinRoom').disabled = $('createRoom').disabled = true; $('joinError').textContent = '';
    try {
      const r = await socket.timeout(20000).emitWithAck('join',{ name, room, create });
      if (!r.ok) throw Error(r.message);
      remember(r);
    } catch (e) { if ($('joinError')) { $('joinError').textContent = errorText(e.message); $('joinRoom').disabled = $('createRoom').disabled = false; } }
  };
  $('joinForm').addEventListener('submit',e => { e.preventDefault(); join(false); });
  $('createRoom').addEventListener('click',() => join(true));
}
socket.on('connect',async () => {
  $('connection').textContent = '已连接'; $('connection').className = 'connection online';
  if (resumeToken) {
    try { const r = await socket.timeout(15000).emitWithAck('resume',{token:resumeToken});
      if (!r.ok) { resetEntry(r.message); return; } remember(r);
    } catch { toast('恢复连接失败，请刷新重试',true); }
  }
});
socket.on('disconnect',() => {
  $('connection').textContent = '重连中…'; $('connection').className = 'connection offline';
  busy = false; if (state) renderGame();
});
socket.on('connect_error',() => { $('connection').textContent = '连接失败，正在重试'; $('connection').className = 'connection offline'; });
socket.on('lobby',data => { roster = data; if (!state) renderLobby(); });
socket.on('state',data => {
  state = data; deadline = Date.now()+data.remainingMs;
  selected = new Set([...selected].filter(id => data.hand.some(c => c.id === id)));
  if (data.choice?.id !== lastChoice) { lastChoice = data.choice?.id; choicePage = 0; }
  renderGame(); renderChoice();
});
socket.on('notice',data => toast(errorText(data.message),data.error));
socket.on('roomClosed',data => resetEntry(data.message));

async function action(data, success) {
  if (busy || !socket.connected) return;
  busy = true; if (state) renderGame();
  try {
    const r = await socket.timeout(12000).emitWithAck('action',data);
    if (!r.ok) toast(errorText(r.message),true);
    else if (success) toast(success);
  } catch { toast('连接中断，请查看最新状态后操作',true); }
  finally { busy = false; if (state) renderGame(); }
}
function renderLobby() {
  const room = joined?.room || roster?.room;
  if (!room) return;
  const players = roster?.players || [];
  $('app').innerHTML = `<section class="lobby"><p class="eyebrow">等朋友入座</p><h1>牌桌准备好了。</h1><p class="muted">分享房间号。四人入座后，自动进入第一局商店。</p><div class="roomline"><span>房间号 <strong class="room-code">${esc(room)}</strong></span><button data-do="copy">复制邀请链接</button></div><div class="lobby-seats">${Array.from({length:4},(_,i)=>{
    const p=players.find(p=>p.seat===i);
    return `<div class="lobby-seat ${p?'filled':''}">P${i+1}<strong>${p?esc(p.name):'等待入座'}</strong><small>${p?(p.connected?'已连接':'重连中'):'空位'}</small></div>`;
  }).join('')}</div><p class="muted">${players.length}/4 位玩家 · 可在另外三个标签页打开邀请链接，使用不同名字自测。</p><p class="fineprint">本测试版不需要额外点击准备；开局后座位保留，只能由原标签页重连。</p></section>`;
}
function poker(c, selectable = false) {
  const red = c.suit===1||c.suit===3;
  return `<${selectable?'button':'div'} class="poker ${red?'red':''} ${c.joker?'joker':''} ${selected.has(c.id)&&selectable?'selected':''}" ${selectable?`data-card="${c.id}" aria-label="${c.joker?'赖子':suits[c.suit]+rank(c.rank)}" aria-pressed="${selected.has(c.id)}"`:''}><span>${c.joker?(c.id===53?'小王':'大王'):rank(c.rank)}</span>${c.joker?'<small>赖子</small>':`<span class="suit">${suits[c.suit]}</span>`}</${selectable?'button':'div'}>`;
}
function cardFromId(id) { const r=Math.floor((id-1)/4)+3; return id>=53?{id,joker:true,suit:-1,rank:0}:{id,joker:false,suit:(id-1)%4,rank:r===15?2:r}; }
const disabled = condition => condition?'disabled':'';
function renderGame() {
  const g=state, me=g.players[g.seat], shop=g.shop, blocked=busy||!socket.connected;
  const myTurn=g.phase==='playing'&&g.acting===g.seat&&!g.choice;
  const heading=g.phase==='shopping'?'局前商店':g.phase==='finished'?'本场结束':g.acting===g.seat?'轮到你出牌':`等待 ${g.players[g.acting].name} 出牌`;
  $('app').innerHTML=`<section class="game"><div class="roomline"><span>房间 <b>${esc(g.room||joined?.room)}</b> <button class="quiet" data-do="copy">邀请</button></span><span>第 ${g.round} 局 · ${g.phase==='shopping'?'并行购物':'逆时针出牌'} · 牌堆 ${g.deck} 张</span><span>测试版 · Demo 卡池</span></div><div class="players">${g.players.map(p=>`<div class="player ${p.seat===g.seat?'mine':''} ${g.phase==='playing'&&p.seat===g.acting?'acting':''}"><div class="player-name"><span>P${p.seat+1} ${esc(p.name)} ${p.seat===g.seat?'· 你':''}</span><small>${p.connected?'在线':'离线'}</small></div><div class="player-stats"><span>分数 ${p.score??'—'}</span><span>金币 ${p.coins??'—'}</span><span>手牌 ${p.handCount}</span></div><div class="player-cards">${p.cards.length?p.cards.map(c=>cardName(c.definition)).join(' · '):g.phase==='shopping'&&p.seat!==g.seat?'购物信息私密':'暂无功能牌'}</div></div>`).join('')}</div>
    <section class="stage"><div class="stage-heading"><h2>${esc(heading)}</h2>${g.phase!=='finished'?'<span class="timer" id="timer"></span>':''}</div>
    ${g.phase==='shopping'?shopView(g,me,blocked):g.phase==='finished'?results(g):`<div class="table"><div class="table-label">${g.table.length?`桌面 · ${patterns[g.lastPlay[0]?.pattern]||''} ${g.lastPlay[0]?rank(g.lastPlay[0].rank):''}`:'新一巡 · 当前玩家领出'}</div><div class="table-cards">${g.table.map(id=>poker(cardFromId(id))).join('')}</div>${g.lastPlay[0]?.wildcards.length?`<div class="muted">${wildcards(g.lastPlay[0])}</div>`:''}</div>`}</section>
    ${g.phase==='playing'?`<section class="hand-section"><div class="hand-heading"><span>你的手牌 · ${g.hand.length} 张 · 已选 ${selected.size} 张</span><button class="quiet" data-do="clear">清空选择</button></div><div class="hand">${[...g.hand].sort((a,b)=>(a.joker?100:a.rank===2?15:a.rank)-(b.joker?100:b.rank===2?15:b.rank)||a.suit-b.suit).map(c=>poker(c,true)).join('')}</div><div class="actionbar"><button class="primary" data-do="play" ${disabled(blocked||!myTurn||!selected.size)}>出牌</button><button data-do="pass" ${disabled(blocked||!myTurn||!g.table.length)}>不出</button><button data-do="options" ${disabled(blocked||!selected.size||!!g.choice)}>查看方案</button><span class="action-hint">${g.choice?'请在弹窗确认出牌方案':myTurn?(g.table.length?'同型同张数，点数接大一档；2 / 炸弹按特殊规则。':'你领出，请选择手牌。'):'点击手牌预选，轮到你后出牌。'}</span></div><div class="personal-effects">${me.cards.map(c=>`<span class="effect-tag">${cardName(c.definition)} · ${esc(c.description||cardEffect(c.definition))}</span>`).join('')}</div>${g.preview.length?`<p class="muted">选牌方案：${g.preview.slice(0,8).map(m=>`${patterns[m.pattern]} ${rank(m.rank)}`).join(' / ')}${g.preview.length>8?` … 共 ${g.preview.length} 种`:''}</p>`:''}</section>`:''}</section>`;
  updateTimer();
}
const facilityInfo = [null,['货架扩展 I','普通货位增至 4',18],['货架扩展 II','普通货位增至 5',30],['功能槽 I','增加 1 个功能槽',20],['功能槽 II','再增加 1 个功能槽',35],['加强折扣','每次商店首次加强少 2 金币',14],null,['刷新折扣','每次商店首次付费刷新少 1 金币',12],['出售加成','出售返还购入价的 60%',12]];
function offerView(g,me,off,o) {
  if(!o.product) return '<div class="empty-offer">货位已空</div>';
  const consumable=o.kind==='consumable', targets=o.targets||[];
  const unavailable=off||me.coins<o.price||(!consumable&&me.cards.length>=g.self.functionSlots)||(o.needsTarget&&!targets.length);
  return `<article class="function-card ${o.locked?'locked':''}"><strong>${consumable?esc(o.name):cardName(o.definition)} ${o.locked?'· 已锁':''}</strong>
    <div class="effect">${consumable?esc(o.description):cardEffect(o.definition)}</div>
    ${consumable?'<small>消耗牌 · 购买即使用</small>':''}
    ${o.needsTarget?`<label class="target-label">强化目标<select data-target-product="${o.product}" aria-label="${esc(o.name)}的强化目标"><option value="">请选择功能牌</option>${targets.map(seq=>{const c=me.cards.find(c=>c.seq===seq);return `<option value="${seq}" ${enhancementTargets.get(o.product)===seq?'selected':''}>槽 ${(c?.slot??0)+1} · ${cardName(c?.definition)} +${c?.enhancementLevel??0}</option>`;}).join('')}</select></label>`:''}
    <div class="price">${o.price} <small>金币</small></div><div class="card-actions"><button class="buy" data-do="buy" data-slot="${o.slot}" ${disabled(unavailable)}>${consumable?'购买并使用':'购买'}</button><button data-do="${o.locked?'unlock':'lock'}" data-slot="${o.slot}" ${disabled(off)}>${o.locked?'解锁':'锁定'}</button></div></article>`;
}
function shopView(g,me,blocked) {
  const s=g.shop, off=blocked||s.done, fs=g.self.facilities;
  const refresh=Math.max(s.freeRefreshes?0:1,s.freeRefreshes?0:2+s.paidRefreshes-(fs.includes(7)&&!s.refreshDiscountUsed?1:0));
  const facility=facilityInfo[s.facility];
  return `<div class="shop-layout"><div><p class="muted">商店 Lv.${g.self.shopLevel??1} · ${s.offers.length}/6 货位 · 金币 ${me.coins}。消耗牌购买即生效。</p><div class="offers" style="margin-top:10px">${s.offers.map(o=>offerView(g,me,off,o)).join('')}</div><div class="shop-operations"><button data-do="refresh" ${disabled(off||s.refreshes>=5||me.coins<refresh)}>刷新 ${refresh} 金币 (${s.refreshes}/5)</button><button class="primary" data-do="done" ${disabled(off)}>${s.done?'已结束，等待其他玩家':'结束购物'}</button></div><p class="fineprint">锁定货位保留到下局；刷新不会影响锁定货位。全部结束或倒计时归零后发牌。</p></div><div><div class="section-label">你的功能牌 ${me.cards.length}/${g.self.functionSlots}</div><div class="inventory">${me.cards.length?me.cards.map(c=>`<div class="owned-card">${cardName(c.definition)}${c.enhancementLevel?` +${c.enhancementLevel}`:''}<small>${esc(c.description||cardEffect(c.definition))}</small><button data-do="sell" data-seq="${c.seq}" ${disabled(off)}>出售</button></div>`).join(''):'<p class="muted">买一张功能牌试试。</p>'}</div><div class="section-label">牌型加强 · 基础奖励倍数提升</div><div class="upgrades">${patterns.map((p,i)=>{const level=g.self.upgrades[i],cost=Math.max(1,[4,6,8,10,10][i]*(level+1)-(fs.includes(5)&&!s.upgradeDiscountUsed?2:0));return `<button class="upgrade" data-do="upgrade" data-pattern="${i}" ${disabled(off||level>=4||me.coins<cost)}>${p} · ${level+1} 倍<span>${level>=4?'已满级':`加强 ${cost} 金币`}</span></button>`;}).join('')}</div>${facility?`<div class="facility">设施：${facility[0]}<br><span class="muted">${facility[1]}</span><button data-do="facility" ${disabled(off||me.coins<facility[2])}>购买 ${facility[2]} 金币</button></div>`:''}${fs.length?`<p class="fineprint">已有设施：${fs.map(id=>facilityInfo[id]?.[0]||`U${id}`).join('、')}</p>`:''}</div></div>`;
}
function results(g) {
  return `<div class="result"><p class="eyebrow">本场结算</p><h2>${g.winners.map(i=>esc(g.players[i].name)).join('、')} 获胜</h2><table class="scoreboard">${[...g.players].sort((a,b)=>b.score-a.score).map((p,i)=>`<tr><td>${i+1}</td><td>${esc(p.name)}</td><td>${p.score} 分</td><td>${p.coins} 金币</td></tr>`).join('')}</table><p class="muted">四人选择一致后继续。投票会保留到大家作出决定。</p><div class="shop-operations" style="justify-content:center"><button data-do="extra" ${disabled(busy)}>加赛 3 局</button><button data-do="endless" ${disabled(busy)}>无尽模式</button><button class="primary" data-do="new" ${disabled(busy)}>重新开始</button></div></div>`;
}
function wildcards(m) { return m.wildcards.map(([id,r,s])=>`${id===53?'小王':'大王'} → ${suits[s]}${rank(r)}`).join('，'); }
function renderChoice() {
  const choice=state?.choice, dialog=$('choiceDialog');
  if (!choice) { if(dialog.open) dialog.close(); return; }
  const pageSize=40, pages=Math.ceil(choice.options.length/pageSize);
  $('choiceCount').textContent=`${choice.options.length} 种方案`;
  $('choices').innerHTML=choice.options.slice(choicePage*pageSize,(choicePage+1)*pageSize).map(m=>`<button class="choice-option" data-choice="${m.id}">${patterns[m.pattern]} · ${rank(m.rank)}<small>${wildcards(m)||'按当前手牌直接出牌'}</small></button>`).join('');
  $('choicePages').innerHTML=pages>1?`<button data-page="-1" ${disabled(choicePage===0)}>上一页</button><span>${choicePage+1}/${pages}</span><button data-page="1" ${disabled(choicePage+1>=pages)}>下一页</button>`:'';
  if(!dialog.open) dialog.showModal();
}
function updateTimer() { if($('timer')) $('timer').textContent=`${Math.max(0,Math.ceil((deadline-Date.now())/1000))} 秒`; }
setInterval(updateTimer,250);
document.addEventListener('click', async e => {
  const card=e.target.closest('[data-card]');
  if(card) {
    const scroll=card.parentElement.scrollLeft, id=Number(card.dataset.card);
    selected.has(id)?selected.delete(id):selected.add(id); renderGame();
    document.querySelector('.hand').scrollLeft=scroll; return;
  }
  const choice=e.target.closest('[data-choice]');
  if(choice&&state?.choice) { await action({kind:'choose',choice:state.choice.id,option:Number(choice.dataset.choice)}); renderChoice(); return; }
  const page=e.target.closest('[data-page]'); if(page) {choicePage+=Number(page.dataset.page);renderChoice();return;}
  const b=e.target.closest('[data-do]'); if(!b) return;
  const kind=b.dataset.do;
  if(kind==='copy') {
    const url=`${location.origin}/?room=${state?.room||joined?.room||roster?.room}`;
    try{await navigator.clipboard.writeText(url);toast('邀请链接已复制');}
    catch {toast(`房间号：${state?.room||joined?.room||roster?.room}，分享地址栏网址和房间号即可`);} return;
  }
  if(kind==='clear') {selected.clear();renderGame();return;}
  if(!state) return;
  const g=state, shop=g.shop?.id, data={kind,shop};
  if(['buy','lock','unlock'].includes(kind)) {
    data.slot=Number(b.dataset.slot);const offer=g.shop.offers[data.slot];data.product=offer.product;
    if(kind==='buy'&&offer.needsTarget) {
      data.target=enhancementTargets.get(offer.product);
      if(!offer.targets.includes(data.target)) {toast('请先选择要强化的功能牌',true);return;}
    }
  }
  if(kind==='sell') data.seq=Number(b.dataset.seq);
  if(kind==='upgrade') data.pattern=Number(b.dataset.pattern);
  if(kind==='facility') data.definition=g.shop.facility;
  if(kind==='play'||kind==='options') data.cards=[...selected];
  if(kind==='play'||kind==='pass') data.turn=g.turn;
  if(kind==='extra'||kind==='endless') {data.kind='continue';data.mode=kind==='extra'?'EXTRA3':'ENDLESS';}
  await action(data,['extra','endless','new'].includes(kind)?'已投票，等待其他玩家':'');
});
$('rulesButton').onclick=()=>$('rules').showModal(); $('closeRules').onclick=()=>$('rules').close();
$('choiceDialog').addEventListener('cancel',e=>e.preventDefault());
$('fullscreen').onclick=async()=>{try{if(!document.fullscreenElement){await document.documentElement.requestFullscreen();await screen.orientation?.lock?.('landscape').catch(()=>{});}else await document.exitFullscreen();}catch{toast('此浏览器请手动横屏使用');}};
wireEntry(); socket.connect();

document.addEventListener('change', e => {
  const product=e.target.dataset?.targetProduct;
  if(product) enhancementTargets.set(Number(product),Number(e.target.value));
});
