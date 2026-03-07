import { useEffect, useRef, useState, useCallback } from 'react';
import gsap from 'gsap';

export default function LzwViz({ steps, mode }) {
  const dictRef = useRef(null);
  const stepsRef = useRef(null);
  const [openSections, setOpenSections] = useState({ info: true, dict: true, steps: true });
  const [speed, setSpeed] = useState(1);

  const toggle = (key) => setOpenSections(prev => ({ ...prev, [key]: !prev[key] }));

  const animateSection = useCallback((ref, selector) => {
    if (!ref.current) return;
    gsap.fromTo(ref.current.querySelectorAll(selector),
      { opacity: 0, x: -12 },
      { opacity: 1, x: 0, duration: 0.2 / speed, stagger: 0.03 / speed, ease: 'power2.out' }
    );
  }, [speed]);

  useEffect(() => { if (openSections.dict) animateSection(dictRef, '.dict-entry'); }, [openSections.dict, steps, animateSection]);
  useEffect(() => { if (openSections.steps) animateSection(stepsRef, '.lzw-step'); }, [openSections.steps, steps, animateSection]);

  if (!steps) return null;

  const isCompress = mode === 'compress';
  const dictSteps = steps.dictionarySteps || [];
  const encodeSteps = steps.encodeSteps || [];
  const decodeSteps = steps.decodeSteps || [];
  const activeSteps = isCompress ? encodeSteps : decodeSteps;

  const fmt = (str) => {
    if (!str) return '';
    let d = '';
    for (let i = 0; i < str.length && i < 12; i++) {
      const c = str.charCodeAt(i);
      d += (c >= 32 && c < 127) ? str[i] : `\\x${c.toString(16).padStart(2, '0')}`;
    }
    return str.length > 12 ? d + '...' : d;
  };

  return (
    <>
      <div className="playback-controls">
        <span style={{ fontSize: 13, fontWeight: 500 }}>LZW {isCompress ? 'Compression' : 'Decompression'}</span>
        <div className="speed-control">
          <span>Speed</span>
          <select value={speed} onChange={(e) => setSpeed(Number(e.target.value))}>
            <option value={0.5}>0.5x</option>
            <option value={1}>1x</option>
            <option value={2}>2x</option>
            <option value={4}>4x</option>
          </select>
        </div>
      </div>

      {/* Params */}
      <div className="viz-section">
        <div className="viz-section-header" onClick={() => toggle('info')}>
          <div className="viz-section-title"><span>LZW Parameters</span></div>
          <span className="viz-section-toggle">{openSections.info ? '\u25BE' : '\u25B8'}</span>
        </div>
        {openSections.info && (
          <div className="viz-section-body">
            <div className="stats-grid" style={{ gridTemplateColumns: 'repeat(4, 1fr)' }}>
              <div className="stat-item">
                <div className="stat-value accent">{steps.bitSize || '-'}</div>
                <div className="stat-label">Bit Size</div>
              </div>
              <div className="stat-item">
                <div className="stat-value orange">{steps.finalDictSize || steps.totalCodes || '-'}</div>
                <div className="stat-label">{isCompress ? 'Dict Size' : 'Total Codes'}</div>
              </div>
              <div className="stat-item">
                <div className="stat-value accent">{steps.originalSize ? `${steps.originalSize} B` : '-'}</div>
                <div className="stat-label">Original</div>
              </div>
              <div className="stat-item">
                <div className="stat-value green">
                  {isCompress ? (steps.compressedSize ? `${steps.compressedSize} B` : '-') : (steps.decompressedSize ? `${steps.decompressedSize} B` : '-')}
                </div>
                <div className="stat-label">{isCompress ? 'Compressed' : 'Output'}</div>
              </div>
            </div>
            <p style={{ marginTop: 12, fontSize: 12, color: 'var(--text-muted)', lineHeight: 1.6 }}>
              {isCompress
                ? 'Builds a dictionary starting with 256 single-byte entries. Reads input to form longer sequences. When a new sequence appears, outputs the code for the known prefix and adds the new sequence.'
                : 'Reads codes and rebuilds the same dictionary the compressor built. Both sides construct identical dictionaries in lockstep.'}
            </p>
          </div>
        )}
      </div>

      {/* Dict */}
      {isCompress && dictSteps.length > 0 && (
        <div className="viz-section">
          <div className="viz-section-header" onClick={() => toggle('dict')}>
            <div className="viz-section-title">
              <span>Step 1: Dictionary Growth</span>
              <span className="step-badge lzw">{dictSteps.length} entries</span>
            </div>
            <span className="viz-section-toggle">{openSections.dict ? '\u25BE' : '\u25B8'}</span>
          </div>
          {openSections.dict && (
            <div className="viz-section-body" ref={dictRef}>
              <p style={{ marginBottom: 8, fontSize: 11, color: 'var(--text-muted)', fontStyle: 'italic' }}>
                Initial: 256 single-byte entries (0x00 - 0xFF)
              </p>
              <div className="dict-entries">
                {dictSteps.map((entry, i) => (
                  <div key={i} className="dict-entry">
                    <span className="dict-index">{entry.index}</span>
                    <span style={{ color: 'var(--text-muted)' }}>=</span>
                    <span className="dict-value" title={entry.entry}>"{fmt(entry.entry)}"</span>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      )}

      {/* Steps */}
      {activeSteps.length > 0 && (
        <div className="viz-section">
          <div className="viz-section-header" onClick={() => toggle('steps')}>
            <div className="viz-section-title">
              <span>Step {isCompress ? '2' : '1'}: {isCompress ? 'Encoding' : 'Decoding'}</span>
              <span className="step-badge lzw">{activeSteps.length} steps</span>
            </div>
            <span className="viz-section-toggle">{openSections.steps ? '\u25BE' : '\u25B8'}</span>
          </div>
          {openSections.steps && (
            <div className="viz-section-body" ref={stepsRef}>
              <div className="lzw-steps">
                {isCompress ? encodeSteps.map((step, i) => (
                  <div key={i} className="lzw-step">
                    <span className="lzw-step-num">#{step.step}</span>
                    <span className="lzw-step-seq">"{fmt(step.sequence)}"</span>
                    <span className="lzw-step-arrow">&rarr;</span>
                    <span className="lzw-step-code">code: {step.code}</span>
                    <span className="lzw-step-new">+ dict[{step.newCode}] = "{fmt(step.newEntry)}"</span>
                  </div>
                )) : decodeSteps.map((step, i) => (
                  <div key={i} className="lzw-step">
                    <span className="lzw-step-num">#{step.step}</span>
                    <span className="lzw-step-code">code: {step.code}</span>
                    <span className="lzw-step-arrow">&rarr;</span>
                    <span className="lzw-step-seq">"{fmt(step.output)}"</span>
                    <span className="lzw-step-new">+ dict[{step.newDictIndex}] = "{fmt(step.newDictEntry)}"</span>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      )}
    </>
  );
}
