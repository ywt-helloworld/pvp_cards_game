// Browser actions contain intent only. The gateway assigns request IDs and the
// C++ connection binds the actor; clients cannot submit internal fact events.
const integer = (v, max = Number.MAX_SAFE_INTEGER) => {
  if (!Number.isSafeInteger(v) || v < 0 || v > max) throw Error('无效的操作参数');
  return v;
};
export function command(a) {
  if (!a || typeof a !== 'object') throw Error('无效的操作');
  const shop = () => integer(a.shop);
  const cards = () => {
    if (!Array.isArray(a.cards) || !a.cards.length || a.cards.length > 54) throw Error('请选择手牌');
    const ids = a.cards.map(v => integer(v, 54));
    if (ids.includes(0) || new Set(ids).size !== ids.length) throw Error('手牌编号无效');
    return ids.join(' ');
  };
  switch (a.kind) {
    case 'state': return 'STATE';
    case 'done': return `DONE ${shop()}`;
    case 'buy': return `BUY ${shop()} ${integer(a.slot, 5)} ${integer(a.product)}`;
    case 'sell': return `SELL ${shop()} ${integer(a.seq)}`;
    case 'refresh': return `REFRESH ${shop()}`;
    case 'lock': case 'unlock': return `${a.kind.toUpperCase()} ${shop()} ${integer(a.slot, 5)}`;
    case 'upgrade': return `UPGRADE ${shop()} ${integer(a.pattern, 4)}`;
    case 'facility': return `FACILITY ${shop()} ${integer(a.definition, 8)}`;
    case 'play': return `PLAY ${integer(a.turn)} ${cards()}`;
    case 'options': return `OPTIONS ${cards()}`;
    case 'choose': return `CHOOSE ${integer(a.choice)} ${integer(a.option, 100000)}`;
    case 'pass': return `PASS ${integer(a.turn)}`;
    case 'continue':
      if (!['EXTRA3', 'ENDLESS'].includes(a.mode)) throw Error('无效的续局模式');
      return `CONTINUE ${a.mode}`;
    case 'new': return 'NEW';
    default: throw Error('不支持的操作');
  }
}
