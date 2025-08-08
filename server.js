// server.js
// Simple Express server: serves static frontend and forwards queries to compiled C++ executable.

const express = require('express');
const path = require('path');
const fs = require('fs');
const { spawn } = require('child_process');
const multer = require('multer');

const app = express();
const PORT = 5000;

app.use(express.json());
app.use('/static', express.static(path.join(__dirname, 'frontend')));

// sample policies folder
const SAMPLES = path.join(__dirname, 'samples');
if (!fs.existsSync(SAMPLES)) fs.mkdirSync(SAMPLES);

// load-sample endpoint
app.get('/load-sample', (req, res) => {
  const samplePath = path.join(SAMPLES, 'policy_sample.txt');
  if (!fs.existsSync(samplePath)) return res.json({ ok:false, error:'sample missing' });
  const txt = fs.readFileSync(samplePath, 'utf8');
  // we just index on the C++ side at runtime; for now return success
  res.json({ ok:true, indexed:'sample-policy' });
});

// upload-policy endpoint (accepts plain text policies)
const upload = multer({ dest: path.join(SAMPLES, 'uploads/') });
app.post('/upload-policy', upload.single('policy'), (req, res) => {
  const file = req.file;
  if (!file) return res.status(400).json({ ok:false, error:'no-file' });
  // For simplicity accept .txt and move to samples/policy_uploaded.txt
  const dst = path.join(SAMPLES, 'policy_uploaded.txt');
  fs.renameSync(file.path, dst);
  res.json({ ok:true, doc_id:'policy_uploaded' });
});

// process endpoint: read sample policy file and send to C++ as stdin JSON
app.post('/process', (req, res) => {
  const query = req.body.query || '';
  const samplePaths = [
    path.join(SAMPLES, 'policy_uploaded.txt'),
    path.join(SAMPLES, 'policy_sample.txt')
  ];
  // prefer uploaded, else fallback
  let policyText = '';
  for (let p of samplePaths) {
    if (fs.existsSync(p)) { policyText = fs.readFileSync(p,'utf8'); break; }
  }
  // split into clauses by newline (simple)
  const clauses = policyText.split(/\r?\n/).map(s => s.trim()).filter(s => s.length>0);

  // build JSON to send to C++ via stdin
  const payload = JSON.stringify({ query: query, policyClauses: clauses });

  // spawn C++ executable (must be compiled and present at ./policy_processor)
  const exe = path.join(__dirname, 'policy_processor');
  if (!fs.existsSync(exe)) {
    return res.status(500).json({ error: 'C++ executable not found. Compile policy_processor.cpp first.' });
  }

  const child = spawn(exe, [], { stdio: ['pipe','pipe','pipe'] });

  let stdout = '', stderr = '';
  child.stdout.on('data', d => { stdout += d.toString(); });
  child.stderr.on('data', d => { stderr += d.toString(); });

  child.on('close', code => {
    if (code !== 0) {
      console.error('C++ stderr:', stderr);
      return res.status(500).json({ error: 'C++ process failed', details: stderr });
    }
    try {
      const out = JSON.parse(stdout);
      res.json(out);
    } catch (e) {
      res.status(500).json({ error: 'Invalid JSON from C++', raw: stdout, err: e.message });
    }
  });

  // write payload and close stdin
  child.stdin.write(payload);
  child.stdin.end();
});

app.listen(PORT, ()=> console.log(`Server: http://localhost:${PORT}`));
