import { useEffect, useRef, useState, useCallback } from 'react';
import gsap from 'gsap';

function HuffmanTree({ tree }) {
  const svgRef = useRef(null);

  useEffect(() => {
    if (!tree || !svgRef.current) return;

    const nodes = [];
    const links = [];

    function layout(node, x, y, spread, depth) {
      if (!node) return;
      const id = nodes.length;
      nodes.push({ ...node, x, y, id, depth });
      if (node.left) {
        const childId = nodes.length;
        layout(node.left, x - spread, y + 70, spread * 0.55, depth + 1);
        links.push({ from: id, to: childId, label: '0' });
      }
      if (node.right) {
        const childId = nodes.length;
        layout(node.right, x + spread, y + 70, spread * 0.55, depth + 1);
        links.push({ from: id, to: childId, label: '1' });
      }
    }

    const maxDepth = (function getDepth(n) {
      if (!n) return 0;
      return 1 + Math.max(getDepth(n.left), getDepth(n.right));
    })(tree);

    layout(tree, 400, 30, Math.min(300, maxDepth * 60), 0);

    let minX = Infinity, maxX = -Infinity, maxY = 0;
    nodes.forEach(n => {
      if (n.x < minX) minX = n.x;
      if (n.x > maxX) maxX = n.x;
      if (n.y > maxY) maxY = n.y;
    });

    const padding = 40;
    const svg = svgRef.current;
    svg.setAttribute('viewBox', `${minX - padding} 0 ${maxX - minX + padding * 2} ${maxY + padding * 2 + 30}`);
    svg.innerHTML = '';

    links.forEach(link => {
      const from = nodes[link.from];
      const to = nodes[link.to];
      const line = document.createElementNS('http://www.w3.org/2000/svg', 'line');
      line.setAttribute('x1', from.x);
      line.setAttribute('y1', from.y);
      line.setAttribute('x2', to.x);
      line.setAttribute('y2', to.y);
      line.setAttribute('class', 'tree-link');
      svg.appendChild(line);

      const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      text.setAttribute('x', (from.x + to.x) / 2 + (link.label === '0' ? -10 : 10));
      text.setAttribute('y', (from.y + to.y) / 2 - 5);
      text.setAttribute('class', 'tree-link-label');
      text.textContent = link.label;
      text.style.opacity = '0';
      svg.appendChild(text);
    });

    nodes.forEach(node => {
      const g = document.createElementNS('http://www.w3.org/2000/svg', 'g');
      g.setAttribute('class', `tree-node ${node.byte !== undefined ? 'leaf' : ''}`);
      g.setAttribute('transform', `translate(${node.x}, ${node.y})`);
      const circle = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
      circle.setAttribute('r', node.byte !== undefined ? 20 : 16);
      g.appendChild(circle);
      const text = document.createElementNS('http://www.w3.org/2000/svg', 'text');
      text.setAttribute('dy', node.byte !== undefined ? -4 : 4);
      text.textContent = node.byte !== undefined ? (node.char || node.byte) : node.frequency;
      g.appendChild(text);
      if (node.byte !== undefined) {
        const freqText = document.createElementNS('http://www.w3.org/2000/svg', 'text');
        freqText.setAttribute('dy', 12);
        freqText.setAttribute('font-size', '9');
        freqText.classList.add('tree-link-label');
        freqText.textContent = `f:${node.frequency}`;
        g.appendChild(freqText);
      }
      svg.appendChild(g);
    });

    const tl = gsap.timeline();
    tl.fromTo(svg.querySelectorAll('.tree-link'),
      { opacity: 0, attr: { x2: (i) => nodes[links[i]?.from]?.x, y2: (i) => nodes[links[i]?.from]?.y } },
      { opacity: 0.5, attr: { x2: (i) => nodes[links[i]?.to]?.x, y2: (i) => nodes[links[i]?.to]?.y },
        duration: 0.3, stagger: 0.05, ease: 'power2.out' }
    );
    tl.fromTo(svg.querySelectorAll('.tree-node'),
      { opacity: 0, scale: 0, transformOrigin: 'center' },
      { opacity: 1, scale: 1, duration: 0.3, stagger: 0.04, ease: 'back.out(1.7)' },
      '-=0.5'
    );
    tl.fromTo(svg.querySelectorAll('.tree-link-label'),
      { opacity: 0 },
      { opacity: 1, duration: 0.2, stagger: 0.03 },
      '-=0.3'
    );
  }, [tree]);

  return (
    <div className="tree-container">
      <svg ref={svgRef} className="tree-svg" width="100%" height="400" />
    </div>
  );
}

export default function HuffmanViz({ steps, mode }) {
  const freqRef = useRef(null);
  const mergeRef = useRef(null);
  const codeRef = useRef(null);
  const encRef = useRef(null);
  const [openSections, setOpenSections] = useState({
    freq: true, merge: true, tree: true, codes: true, encoding: true
  });
  const [speed, setSpeed] = useState(1);

  const toggle = (key) => setOpenSections(prev => ({ ...prev, [key]: !prev[key] }));

  const animateSection = useCallback((ref, selector) => {
    if (!ref.current) return;
    gsap.fromTo(ref.current.querySelectorAll(selector),
      { opacity: 0, y: 12 },
      { opacity: 1, y: 0, duration: 0.25 / speed, stagger: 0.04 / speed, ease: 'power2.out' }
    );
  }, [speed]);

  useEffect(() => { if (openSections.freq) animateSection(freqRef, '.freq-item'); }, [openSections.freq, steps, animateSection]);
  useEffect(() => { if (openSections.merge) animateSection(mergeRef, '.merge-step'); }, [openSections.merge, steps, animateSection]);
  useEffect(() => { if (openSections.codes) animateSection(codeRef, '.code-table tr'); }, [openSections.codes, steps, animateSection]);
  useEffect(() => { if (openSections.encoding) animateSection(encRef, '.encode-step'); }, [openSections.encoding, steps, animateSection]);

  if (!steps) return null;

  const frequencies = steps.frequencies || [];
  const mergeSteps = steps.mergeSteps || [];
  const codeTable = steps.codeTable || [];
  const encodingSteps = steps.encodingSteps || [];
  const decodeSteps = steps.decodeSteps || [];

  return (
    <>
      <div className="playback-controls">
        <span style={{ fontSize: 13, fontWeight: 500 }}>
          Huffman {mode === 'compress' ? 'Compression' : 'Decompression'}
        </span>
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

      {/* Frequency */}
      <div className="viz-section">
        <div className="viz-section-header" onClick={() => toggle('freq')}>
          <div className="viz-section-title">
            <span>Step 1: Frequency Analysis</span>
            <span className="step-badge huffman">{frequencies.length} unique</span>
          </div>
          <span className="viz-section-toggle">{openSections.freq ? '\u25BE' : '\u25B8'}</span>
        </div>
        {openSections.freq && (
          <div className="viz-section-body" ref={freqRef}>
            <div className="freq-table">
              {frequencies.map((f, i) => (
                <div key={i} className="freq-item">
                  <span className="freq-char">'{f.char || f.byte}'</span>
                  <span className="freq-count">{f.frequency}</span>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Merge */}
      {mergeSteps.length > 0 && (
        <div className="viz-section">
          <div className="viz-section-header" onClick={() => toggle('merge')}>
            <div className="viz-section-title">
              <span>Step 2: Build Tree</span>
              <span className="step-badge huffman">{mergeSteps.length} merges</span>
            </div>
            <span className="viz-section-toggle">{openSections.merge ? '\u25BE' : '\u25B8'}</span>
          </div>
          {openSections.merge && (
            <div className="viz-section-body" ref={mergeRef}>
              <div className="merge-steps">
                {mergeSteps.map((step, i) => (
                  <div key={i} className="merge-step">
                    <span style={{ color: 'var(--text-muted)', minWidth: 20, fontSize: 11 }}>#{i + 1}</span>
                    <span className="merge-node left">
                      {step.leftByte !== undefined ? `'${String.fromCharCode(step.leftByte >= 32 && step.leftByte < 127 ? step.leftByte : 63)}' ` : ''}
                      f={step.leftFreq}
                    </span>
                    <span className="merge-arrow">+</span>
                    <span className="merge-node right">
                      {step.rightByte !== undefined ? `'${String.fromCharCode(step.rightByte >= 32 && step.rightByte < 127 ? step.rightByte : 63)}' ` : ''}
                      f={step.rightFreq}
                    </span>
                    <span className="merge-arrow">=</span>
                    <span className="merge-node parent">f={step.parentFreq}</span>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>
      )}

      {/* Tree */}
      {steps.tree && (
        <div className="viz-section">
          <div className="viz-section-header" onClick={() => toggle('tree')}>
            <div className="viz-section-title"><span>Huffman Tree</span></div>
            <span className="viz-section-toggle">{openSections.tree ? '\u25BE' : '\u25B8'}</span>
          </div>
          {openSections.tree && (
            <div className="viz-section-body">
              <HuffmanTree tree={steps.tree} />
            </div>
          )}
        </div>
      )}

      {/* Code Table */}
      {codeTable.length > 0 && (
        <div className="viz-section">
          <div className="viz-section-header" onClick={() => toggle('codes')}>
            <div className="viz-section-title">
              <span>Step 3: Code Assignment</span>
              <span className="step-badge huffman">{codeTable.length} codes</span>
            </div>
            <span className="viz-section-toggle">{openSections.codes ? '\u25BE' : '\u25B8'}</span>
          </div>
          {openSections.codes && (
            <div className="viz-section-body" ref={codeRef}>
              <table className="code-table">
                <thead>
                  <tr style={{ opacity: 1 }}>
                    <th>Char</th><th>Byte</th><th>Freq</th><th>Code</th><th>Len</th>
                  </tr>
                </thead>
                <tbody>
                  {codeTable.map((entry, i) => (
                    <tr key={i}>
                      <td style={{ fontWeight: 600 }}>{entry.char || '-'}</td>
                      <td style={{ color: 'var(--text-muted)' }}>{entry.byte}</td>
                      <td>{entry.frequency}</td>
                      <td className="code-bits">{entry.code}</td>
                      <td style={{ color: 'var(--text-muted)' }}>{entry.code?.length}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}

      {/* Encoding */}
      {(encodingSteps.length > 0 || decodeSteps.length > 0) && (
        <div className="viz-section">
          <div className="viz-section-header" onClick={() => toggle('encoding')}>
            <div className="viz-section-title">
              <span>Step 4: {mode === 'compress' ? 'Encoding' : 'Decoding'}</span>
            </div>
            <span className="viz-section-toggle">{openSections.encoding ? '\u25BE' : '\u25B8'}</span>
          </div>
          {openSections.encoding && (
            <div className="viz-section-body" ref={encRef}>
              <div className="encoding-steps">
                {(mode === 'compress' ? encodingSteps : decodeSteps).map((step, i) => (
                  <div key={i} className="encode-step">
                    <span className="encode-char">
                      {step.byte >= 32 && step.byte < 127 ? String.fromCharCode(step.byte) : `[${step.byte}]`}
                    </span>
                    <span className="encode-code">{step.code}</span>
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
