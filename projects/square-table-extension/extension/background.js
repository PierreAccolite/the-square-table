const DEFAULTS={repo:'PierreAccolite/the-square-table',projectPath:'projects/square-table-ui',pagesUrl:'https://pierreaccolite.github.io/the-square-table/',gptUrl:'',grokUrl:'',pollMinutes:2,githubToken:''};
const getSettings=async()=>Object.assign({},DEFAULTS,await chrome.storage.local.get(DEFAULTS));

async function api(path){
  const s=await getSettings(),h={Accept:'application/vnd.github+json'};
  if(s.githubToken)h.Authorization=`Bearer ${s.githubToken}`;
  const r=await fetch(`https://api.github.com/repos/${s.repo}/contents/${path}?ref=main`,{headers:h,cache:'no-store'});
  if(!r.ok)throw new Error(`GitHub ${r.status}`);
  return r.json();
}

async function raw(path){
  const s=await getSettings();
  const r=await fetch(`https://raw.githubusercontent.com/${s.repo}/main/${path}`,{cache:'no-store'});
  if(!r.ok)throw new Error(`Raw ${r.status}`);
  return r.text();
}

function meta(t,k){
  const m=t.match(new RegExp(`\\*\\*${k}:\\*\\*\\s*(.+)`,'i'));
  return m?m[1].trim():'—';
}

async function poll(){
  try{
    const s=await getSettings();
    const files=(await api(`${s.projectPath}/messages`)).filter(x=>x.type==='file'&&x.name.endsWith('.md')).sort((a,b)=>b.name.localeCompare(a.name));
    const state=await chrome.storage.local.get({seenShas:[],latestMessages:[]});
    const seen=new Set(state.seenShas||[]),latest=[];
    let unread=0;
    for(const f of files.slice(0,8)){
      const text=await raw(f.path);
      const rec={name:f.name,path:f.path,sha:f.sha,from:meta(text,'From'),to:meta(text,'To'),type:meta(text,'Type')};
      latest.push(rec);
      if(!seen.has(f.sha))unread++;
    }
    await chrome.storage.local.set({latestMessages:latest,lastPoll:Date.now(),lastError:'',unreadCount:unread});
    await chrome.action.setBadgeText({text:unread?String(Math.min(unread,99)):''});
    await chrome.action.setBadgeBackgroundColor({color:'#d6a94d'});
  }catch(e){
    await chrome.storage.local.set({lastError:e.message,lastPoll:Date.now()});
    await chrome.action.setBadgeText({text:'!'});
  }
}

async function installAlarm(){
  const s=await getSettings();
  await chrome.alarms.clear('squareTablePoll');
  chrome.alarms.create('squareTablePoll',{periodInMinutes:Math.max(1,Number(s.pollMinutes)||2)});
}

function canonical(url){
  try{
    const u=new URL(url);
    u.hash='';
    return u.href.replace(/\/$/,'');
  }catch{return url||'';}
}

async function focusOrOpen(url,hostMatchers=[]){
  if(!url)return {reused:false,error:'URL not configured'};

  const allTabs=await chrome.tabs.query({});
  const wanted=canonical(url);

  // Prefer the exact configured conversation/project URL.
  let tab=allTabs.find(t=>t.url&&canonical(t.url)===wanted);

  // If navigation inside the app changed the URL slightly, prefer the same
  // conversation path before falling back to any tab on that host.
  if(!tab){
    try{
      const target=new URL(url);
      const targetPath=target.pathname.replace(/\/$/,'');
      tab=allTabs.find(t=>{
        if(!t.url)return false;
        try{
          const u=new URL(t.url);
          return u.hostname===target.hostname&&u.pathname.replace(/\/$/,'')===targetPath;
        }catch{return false;}
      });
    }catch{}
  }

  if(!tab&&hostMatchers.length){
    tab=allTabs.find(t=>{
      if(!t.url)return false;
      try{
        const host=new URL(t.url).hostname;
        return hostMatchers.some(x=>host===x||host.endsWith(`.${x}`));
      }catch{return false;}
    });
  }

  if(tab){
    await chrome.tabs.update(tab.id,{active:true});
    if(tab.windowId!=null)await chrome.windows.update(tab.windowId,{focused:true});
    return {reused:true,tabId:tab.id};
  }

  const created=await chrome.tabs.create({url});
  return {reused:false,tabId:created.id};
}

async function openTarget(target){
  const s=await getSettings();
  const map={
    square:{url:s.pagesUrl,hosts:['pierreaccolite.github.io']},
    gpt:{url:s.gptUrl,hosts:['chatgpt.com']},
    grok:{url:s.grokUrl,hosts:['grok.com']},
    github:{url:`https://github.com/${s.repo}/tree/main/${s.projectPath}`,hosts:['github.com']}
  };
  const t=map[target];
  if(!t)return {error:'Unknown target'};
  return focusOrOpen(t.url,t.hosts);
}

chrome.runtime.onInstalled.addListener(async()=>{await installAlarm();await poll()});
chrome.runtime.onStartup.addListener(async()=>{await installAlarm();await poll()});
chrome.alarms.onAlarm.addListener(a=>{if(a.name==='squareTablePoll')poll()});
chrome.storage.onChanged.addListener((changes,area)=>{
  if(area==='local'&&(changes.pollMinutes||changes.repo||changes.projectPath||changes.githubToken)){
    installAlarm();poll();
  }
});

chrome.runtime.onMessage.addListener((msg,_sender,send)=>{
  (async()=>{
    if(msg.type==='poll')return poll();
    if(msg.type==='markRead'){
      const st=await chrome.storage.local.get({latestMessages:[]});
      await chrome.storage.local.set({seenShas:(st.latestMessages||[]).map(x=>x.sha),unreadCount:0});
      await chrome.action.setBadgeText({text:''});
      return {ok:true};
    }
    if(msg.type==='open')return openTarget(msg.target);
  })().then(send).catch(e=>send({error:e.message}));
  return true;
});
