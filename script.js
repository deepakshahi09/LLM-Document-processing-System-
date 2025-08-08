(async ()=> {
  const EL = id => document.getElementById(id);
  const queryEl = EL('query');
  const askBtn = EL('askBtn'); // we will attach via event delegation below
  const loadBtn = EL('loadSample');
  const status = EL('status');
  const jsonOut = EL('jsonOut');
  const matches = EL('matches');
  const decisionBadge = EL('decisionBadge');
  const policyFileInput = EL('policyFile');

  // helpers
  function setStatus(s){ status.textContent = s; }
  function renderResult(res){
    jsonOut.textContent = JSON.stringify(res, null, 2);
    // badge
    const d = (res.Decision || res.decision || '—').toLowerCase();
    decisionBadge.className = 'badge ' + ( d.includes('approve') ? 'approved' : d.includes('reject') ? 'rejected' : 'neutral');
    decisionBadge.textContent = (res.Decision || res.decision || '—');
    // matches
    matches.innerHTML = '';
    const list = res.Justification || res.justification || [];
    list.forEach(j => {
      const li = document.createElement('li');
      const meta = document.createElement('div'); meta.className='meta';
      meta.textContent = `${j.doc_id || j.docId || j.source || 'policy'} • score ${(j.score||0).toFixed ? (j.score||0).toFixed(3) : (j.score||0)}`;
      const text = document.createElement('div'); text.textContent = j.text || j.clause || j;
      li.appendChild(meta); li.appendChild(text);
      matches.appendChild(li);
    });
  }

  // load sample policy (server will index it)
  EL('loadSample').addEventListener('click', async () => {
    setStatus('Loading sample policy into index...');
    try {
      const r = await fetch('/load-sample', {method:'GET'});
      const j = await r.json();
      setStatus('Sample loaded: ' + (j.indexed || 'sample-policy'));
      setTimeout(()=>setStatus(''), 2200);
    } catch (e) { setStatus('Failed to load sample'); console.error(e); }
  });

  // handle file upload for policy (optional)
  policyFileInput.addEventListener('change', async (ev) => {
    const f = ev.target.files[0];
    if (!f) return;
    setStatus('Uploading policy file...');
    const fd = new FormData(); fd.append('policy', f);
    try {
      const r = await fetch('/upload-policy', { method:'POST', body: fd });
      const j = await r.json();
      setStatus('Policy uploaded and indexed: ' + j.doc_id);
      setTimeout(()=>setStatus(''), 2200);
    } catch (err) { setStatus('Upload failed'); console.error(err); }
  });

  // main ask button
  EL('askBtn').addEventListener('click', async () => {
    const q = queryEl.value.trim();
    if (!q) { alert('Type a query'); return; }
    setStatus('Processing query...');
    try {
      const r = await fetch('/process', {
        method:'POST',
        headers:{'Content-Type':'application/json'},
        body: JSON.stringify({ query: q })
      });
      const j = await r.json();
      renderResult(j);
      setStatus('Done');
    } catch (err) {
      setStatus('Error processing'); console.error(err);
    }
  });

  // keyboard shortcut: Ctrl+Enter to submit
  queryEl.addEventListener('keydown', (e)=> { if (e.key==='Enter' && e.ctrlKey) EL('askBtn').click(); });

  // initial load: get empty state
  setStatus('');
})();
