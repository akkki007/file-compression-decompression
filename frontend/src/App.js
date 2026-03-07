import React, { useState, useCallback, useMemo, useEffect } from 'react';
import './App.css';
import HuffmanViz from './components/HuffmanViz';
import LzwViz from './components/LzwViz';
import NetworkCanvas from './components/NetworkCanvas';

const API_URL = 'http://localhost:18080';

function formatBytes(bytes) {
  if (bytes === 0) return '0 B';
  const k = 1024;
  const sizes = ['B', 'KB', 'MB'];
  const i = Math.floor(Math.log(bytes) / Math.log(k));
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + sizes[i];
}

function fileToBase64(file) {
  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onload = () => resolve(reader.result.split(',')[1]);
    reader.onerror = reject;
    reader.readAsDataURL(file);
  });
}

function App() {
  const [file, setFile] = useState(null);
  const [algorithm, setAlgorithm] = useState('huffman');
  const [loading, setLoading] = useState(false);
  const [result, setResult] = useState(null);
  const [mode, setMode] = useState(null);
  const [dragging, setDragging] = useState(false);
  const [error, setError] = useState(null);
  const [theme, setTheme] = useState(() =>
    localStorage.getItem('theme') || 'light'
  );

  useEffect(() => {
    document.documentElement.setAttribute('data-theme', theme);
    localStorage.setItem('theme', theme);
  }, [theme]);

  const toggleTheme = () => setTheme(t => t === 'light' ? 'dark' : 'light');

  const handleDrop = useCallback((e) => {
    e.preventDefault();
    setDragging(false);
    const f = e.dataTransfer.files[0];
    if (f) { setFile(f); setResult(null); setError(null); }
  }, []);

  const handleFileChange = (e) => {
    const f = e.target.files[0];
    if (f) { setFile(f); setResult(null); setError(null); }
  };

  const handleCompress = async () => {
    if (!file) return;
    setLoading(true);
    setError(null);
    setMode('compress');
    try {
      const b64 = await fileToBase64(file);
      const resp = await fetch(`${API_URL}/api/compress`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ data: b64, algorithm, filename: file.name }),
      });
      const data = await resp.json();
      if (data.error) throw new Error(data.error);
      setResult(data);
    } catch (err) {
      setError(err.message);
    }
    setLoading(false);
  };

  const handleDecompress = async () => {
    if (!file) return;
    setLoading(true);
    setError(null);
    setMode('decompress');
    try {
      const b64 = await fileToBase64(file);
      const resp = await fetch(`${API_URL}/api/decompress`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ data: b64, algorithm }),
      });
      const data = await resp.json();
      if (data.error) throw new Error(data.error);
      setResult(data);
    } catch (err) {
      setError(err.message);
    }
    setLoading(false);
  };

  const handleDownload = () => {
    if (!result) return;
    const b64 = result[mode === 'compress' ? 'compressedData' : 'decompressedData'];
    if (!b64) return;
    const binary = atob(b64);
    const bytes = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i++) bytes[i] = binary.charCodeAt(i);
    const ext = mode === 'compress' ? (algorithm === 'huffman' ? '.huffz' : '.lzwz') : '.decompressed';
    const blob = new Blob([bytes], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = (file?.name || 'file') + ext;
    a.click();
    URL.revokeObjectURL(url);
  };

  const canvasStatus = useMemo(() => {
    if (loading && mode === 'compress') return 'compressing';
    if (loading && mode === 'decompress') return 'decompressing';
    if (result) return 'done';
    return 'idle';
  }, [loading, mode, result]);

  return (
    <div className="app">
      <header className="header">
        <div className="header-left">
          <span className="header-icon">Z</span>
          <span className="header-title">File Compression</span>
          <span className="header-breadcrumb">/ {algorithm === 'huffman' ? 'Huffman' : 'LZW'}</span>
        </div>
        <div className="header-right">
          <button className="theme-toggle" onClick={toggleTheme} title="Toggle theme">
            {theme === 'light' ? '\u263E' : '\u2600'}
          </button>
        </div>
      </header>

      <div className="main-content">
        <div className="left-panel">
          {/* Upload */}
          <div className="section-block">
            <div className="section-label">File</div>
            <div style={{ padding: '0 8px' }}>
              <div
                className={`upload-zone ${dragging ? 'dragging' : ''} ${file ? 'has-file' : ''}`}
                onDragOver={(e) => { e.preventDefault(); setDragging(true); }}
                onDragLeave={() => setDragging(false)}
                onDrop={handleDrop}
                onClick={() => document.getElementById('file-input').click()}
              >
                <input id="file-input" type="file" style={{ display: 'none' }} onChange={handleFileChange} />
                <div className="upload-text">{file ? 'File selected' : 'Drop a file here or click to browse'}</div>
                {!file && <div className="upload-hint">Any file type</div>}
              </div>
              {file && (
                <div className="file-info">
                  <div>
                    <div className="file-info-name">{file.name}</div>
                    <div className="file-info-size">{formatBytes(file.size)}</div>
                  </div>
                  <button className="file-remove" onClick={(e) => { e.stopPropagation(); setFile(null); setResult(null); }}>
                    &#x2715;
                  </button>
                </div>
              )}
            </div>
          </div>

          {/* Algorithm */}
          <div className="section-block">
            <div className="section-label">Algorithm</div>
            <div className="algo-options">
              <button
                className={`algo-option ${algorithm === 'huffman' ? 'selected' : ''}`}
                onClick={() => { setAlgorithm('huffman'); setResult(null); }}
              >
                <div className="algo-name">Huffman</div>
                <div className="algo-desc">Frequency-based tree encoding</div>
              </button>
              <button
                className={`algo-option ${algorithm === 'lzw' ? 'selected' : ''}`}
                onClick={() => { setAlgorithm('lzw'); setResult(null); }}
              >
                <div className="algo-name">LZW</div>
                <div className="algo-desc">Dictionary-based compression</div>
              </button>
            </div>
          </div>

          {/* Actions */}
          <div className="section-block">
            <div className="section-label">Actions</div>
            <div className="actions">
              <button className="btn btn-compress" disabled={!file || loading} onClick={handleCompress}>
                Compress
              </button>
              <button className="btn btn-decompress" disabled={!file || loading} onClick={handleDecompress}>
                Decompress
              </button>
            </div>
            {error && <div className="error-box" style={{ margin: '8px 8px 0' }}>{error}</div>}
          </div>

          {/* Results */}
          {result && (
            <div className="section-block">
              <div className="section-label">Results</div>
              <div style={{ padding: '0 8px' }}>
                <div className="stats-grid">
                  <div className="stat-item">
                    <div className="stat-value accent">
                      {formatBytes(result.originalSize || result.steps?.originalSize || 0)}
                    </div>
                    <div className="stat-label">Original</div>
                  </div>
                  <div className="stat-item">
                    <div className="stat-value green">
                      {formatBytes(mode === 'compress' ? (result.compressedSize || 0) : (result.decompressedSize || 0))}
                    </div>
                    <div className="stat-label">{mode === 'compress' ? 'Compressed' : 'Output'}</div>
                  </div>
                  {mode === 'compress' && (
                    <div className="stat-item">
                      <div className={`stat-value ${result.compressionRatio < 100 ? 'green' : 'orange'}`}>
                        {(result.compressionRatio || 0).toFixed(1)}%
                      </div>
                      <div className="stat-label">Ratio</div>
                    </div>
                  )}
                </div>
                <button className="btn btn-download" onClick={handleDownload} style={{ width: '100%' }}>
                  Download {mode === 'compress' ? 'compressed' : 'decompressed'} file
                </button>
              </div>
            </div>
          )}
        </div>

        <div className="right-panel">
          <div className="canvas-layer">
            <NetworkCanvas
              algorithm={algorithm}
              status={canvasStatus}
              compressionRatio={result?.compressionRatio ?? null}
              theme={theme}
            />
          </div>
          <div className="viz-layer">
            {!result ? (
              <div className="empty-state">
                <div className="empty-title">Algorithm Visualization</div>
                <div className="empty-desc">
                  Upload a file and run compression to see the step-by-step process
                </div>
              </div>
            ) : (
              <div className="viz-container">
                {algorithm === 'huffman' ? (
                  <HuffmanViz steps={result.steps} mode={mode} />
                ) : (
                  <LzwViz steps={result.steps} mode={mode} />
                )}
              </div>
            )}
          </div>
        </div>
      </div>

      {loading && (
        <div className="loading-overlay">
          <div className="loading-spinner">
            <div className="spinner" />
            <div className="loading-text">
              {mode === 'compress' ? 'Compressing' : 'Decompressing'} with {algorithm === 'huffman' ? 'Huffman' : 'LZW'}...
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

export default App;
