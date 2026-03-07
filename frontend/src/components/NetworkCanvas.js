import { useEffect, useRef, useCallback } from 'react';
import gsap from 'gsap';

function sketchLine(ctx, x1, y1, x2, y2, jitter = 1.2) {
  const midX = (x1 + x2) / 2 + (Math.random() - 0.5) * jitter * 3;
  const midY = (y1 + y2) / 2 + (Math.random() - 0.5) * jitter * 3;
  ctx.beginPath();
  ctx.moveTo(x1 + (Math.random() - 0.5) * jitter, y1 + (Math.random() - 0.5) * jitter);
  ctx.quadraticCurveTo(midX, midY, x2 + (Math.random() - 0.5) * jitter, y2 + (Math.random() - 0.5) * jitter);
  ctx.stroke();
}

function sketchCircle(ctx, x, y, r) {
  ctx.beginPath();
  for (let i = 0; i <= 36; i++) {
    const a = (i / 36) * Math.PI * 2;
    const px = x + Math.cos(a) * (r + Math.random() * 0.8);
    const py = y + Math.sin(a) * (r + Math.random() * 0.8);
    if (i === 0) ctx.moveTo(px, py);
    else ctx.lineTo(px, py);
  }
  ctx.closePath();
}

export default function NetworkCanvas({ algorithm, status, compressionRatio, theme }) {
  const canvasRef = useRef(null);
  const stateRef = useRef({
    nodes: [], particles: [],
    phase: 'idle', progress: 0,
    mouse: { x: -1000, y: -1000 },
    animFrame: null, time: 0,
  });

  const initNodes = useCallback((w, h) => {
    const s = stateRef.current;
    s.nodes = [];
    for (let i = 0; i < 45; i++) {
      s.nodes.push({
        x: Math.random() * w, y: Math.random() * h,
        vx: (Math.random() - 0.5) * 0.3, vy: (Math.random() - 0.5) * 0.3,
        r: 1.5 + Math.random() * 3,
        label: String.fromCharCode(33 + Math.floor(Math.random() * 94)),
        opacity: 0.2 + Math.random() * 0.4,
        phase: Math.random() * Math.PI * 2,
      });
    }
    s.particles = [];
    for (let i = 0; i < 20; i++) {
      s.particles.push({
        x: Math.random() * w, y: Math.random() * h,
        char: Math.random() > 0.5 ? '0' : '1',
        speed: 0.1 + Math.random() * 0.2,
        opacity: 0.04 + Math.random() * 0.06,
      });
    }
  }, []);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    const s = stateRef.current;

    const getCSS = (prop) => getComputedStyle(document.documentElement).getPropertyValue(prop).trim();

    const resize = () => {
      const rect = canvas.parentElement.getBoundingClientRect();
      const dpr = window.devicePixelRatio || 1;
      canvas.width = rect.width * dpr;
      canvas.height = rect.height * dpr;
      canvas.style.width = rect.width + 'px';
      canvas.style.height = rect.height + 'px';
      ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
      if (s.nodes.length === 0) initNodes(rect.width, rect.height);
    };
    resize();
    window.addEventListener('resize', resize);

    const onMouse = (e) => { const r = canvas.getBoundingClientRect(); s.mouse.x = e.clientX - r.left; s.mouse.y = e.clientY - r.top; };
    const onLeave = () => { s.mouse.x = -1000; s.mouse.y = -1000; };
    canvas.addEventListener('mousemove', onMouse);
    canvas.addEventListener('mouseleave', onLeave);

    const w = () => canvas.width / (window.devicePixelRatio || 1);
    const h = () => canvas.height / (window.devicePixelRatio || 1);

    function draw() {
      const W = w(), H = h();
      s.time += 0.016;
      ctx.clearRect(0, 0, W, H);

      const dotColor = getCSS('--canvas-dot');
      const nodeColor = getCSS('--canvas-node');
      const edgeColor = getCSS('--canvas-edge');
      const textColor = getCSS('--canvas-text');
      const accent = getCSS('--accent');

      // Dot grid
      ctx.fillStyle = dotColor;
      for (let gx = 20; gx < W; gx += 20) {
        for (let gy = 20; gy < H; gy += 20) {
          ctx.beginPath();
          ctx.arc(gx, gy, 0.6, 0, Math.PI * 2);
          ctx.fill();
        }
      }

      // Floating bits
      ctx.font = '11px "Source Code Pro", monospace';
      s.particles.forEach(p => {
        p.y -= p.speed;
        if (p.y < -20) { p.y = H + 20; p.x = Math.random() * W; }
        ctx.fillStyle = textColor;
        ctx.globalAlpha = p.opacity;
        ctx.fillText(p.char, p.x, p.y);
      });
      ctx.globalAlpha = 1;

      // Update nodes
      const cx = W / 2, cy = H / 2;
      s.nodes.forEach(n => {
        n.x += n.vx + Math.sin(s.time * 0.4 + n.phase) * 0.1;
        n.y += n.vy + Math.cos(s.time * 0.3 + n.phase) * 0.1;
        if (n.x < 8) { n.x = 8; n.vx *= -0.7; }
        if (n.x > W - 8) { n.x = W - 8; n.vx *= -0.7; }
        if (n.y < 8) { n.y = 8; n.vy *= -0.7; }
        if (n.y > H - 8) { n.y = H - 8; n.vy *= -0.7; }

        const mdx = n.x - s.mouse.x, mdy = n.y - s.mouse.y;
        const md = Math.sqrt(mdx * mdx + mdy * mdy);
        if (md < 100 && md > 0) {
          const f = (100 - md) / 100 * 0.6;
          n.vx += (mdx / md) * f;
          n.vy += (mdy / md) * f;
        }
        n.vx *= 0.98;
        n.vy *= 0.98;

        if (s.phase === 'compressing') {
          const dx = cx - n.x, dy = cy - n.y, d = Math.sqrt(dx * dx + dy * dy);
          if (d > 25) { n.vx += (dx / d) * 0.4 * s.progress; n.vy += (dy / d) * 0.4 * s.progress; }
        }
        if (s.phase === 'decompressing') {
          const dx = n.x - cx, dy = n.y - cy, d = Math.sqrt(dx * dx + dy * dy);
          if (d < W * 0.4) { n.vx += (dx / (d || 1)) * 0.25 * s.progress; n.vy += (dy / (d || 1)) * 0.25 * s.progress; }
        }
      });

      // Edges
      const connDist = s.phase === 'compressing' ? 130 - s.progress * 50 : s.phase === 'decompressing' ? 70 + s.progress * 70 : 90;
      ctx.lineWidth = 0.8;
      s.nodes.forEach((a, i) => {
        for (let j = i + 1; j < s.nodes.length; j++) {
          const b = s.nodes[j];
          const dx = a.x - b.x, dy = a.y - b.y, d = Math.sqrt(dx * dx + dy * dy);
          if (d < connDist) {
            ctx.globalAlpha = (1 - d / connDist) * 0.15;
            ctx.strokeStyle = edgeColor;
            sketchLine(ctx, a.x, a.y, b.x, b.y, 0.8);
          }
        }
      });
      ctx.globalAlpha = 1;

      // Nodes
      s.nodes.forEach(n => {
        const pulse = Math.sin(s.time * 1.5 + n.phase) * 0.2;
        const r = n.r * (s.phase === 'compressing' ? (1 - s.progress * 0.35) : 1) + pulse;

        ctx.strokeStyle = edgeColor;
        ctx.lineWidth = 0.8;
        ctx.globalAlpha = n.opacity * 0.6;
        sketchCircle(ctx, n.x, n.y, r);
        ctx.stroke();

        if (n.r > 3.5) {
          ctx.fillStyle = nodeColor;
          ctx.globalAlpha = n.opacity * 0.3;
          ctx.font = `${Math.max(7, r * 1.6)}px "Source Code Pro", monospace`;
          ctx.textAlign = 'center';
          ctx.textBaseline = 'middle';
          ctx.fillText(n.label, n.x, n.y);
        }
      });
      ctx.globalAlpha = 1;

      // Compress/decompress ring
      if (s.phase === 'compressing' || s.phase === 'decompressing') {
        const ringR = s.phase === 'compressing' ? 180 * (1 - s.progress * 0.65) : 50 + s.progress * 160;
        ctx.strokeStyle = accent;
        ctx.globalAlpha = 0.12;
        ctx.lineWidth = 1.5;
        ctx.setLineDash([5, 7]);
        ctx.beginPath();
        ctx.arc(cx, cy, ringR, s.time * 1.5, s.time * 1.5 + Math.PI * 1.4);
        ctx.stroke();
        ctx.setLineDash([]);
        ctx.globalAlpha = 1;
      }

      // Done state
      if (s.phase === 'done' && compressionRatio != null) {
        ctx.font = '12px "Source Code Pro", monospace';
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillStyle = accent;
        ctx.globalAlpha = 0.25;
        ctx.fillText(compressionRatio < 100 ? 'Compressed' : 'Encoded', cx, cy - 6);
        ctx.font = '11px "Source Code Pro", monospace';
        ctx.globalAlpha = 0.18;
        ctx.fillText(`${compressionRatio.toFixed(1)}%`, cx, cy + 10);
        ctx.globalAlpha = 1;
      }

      s.animFrame = requestAnimationFrame(draw);
    }

    s.animFrame = requestAnimationFrame(draw);

    return () => {
      cancelAnimationFrame(s.animFrame);
      window.removeEventListener('resize', resize);
      canvas.removeEventListener('mousemove', onMouse);
      canvas.removeEventListener('mouseleave', onLeave);
    };
  }, [algorithm, initNodes, compressionRatio, theme]);

  useEffect(() => {
    const s = stateRef.current;
    if (status === 'compressing') {
      s.phase = 'compressing';
      gsap.to(s, { progress: 1, duration: 2, ease: 'power2.inOut' });
    } else if (status === 'decompressing') {
      s.phase = 'decompressing';
      gsap.to(s, { progress: 1, duration: 2, ease: 'power2.inOut' });
    } else if (status === 'done') {
      gsap.to(s, { progress: 0, duration: 1.5, ease: 'power3.out', onComplete: () => { s.phase = 'done'; } });
    } else {
      s.phase = 'idle';
      gsap.to(s, { progress: 0, duration: 1, ease: 'power2.out' });
    }
  }, [status]);

  return (
    <canvas
      ref={canvasRef}
      className="network-canvas"
      style={{ position: 'absolute', inset: 0, width: '100%', height: '100%', pointerEvents: 'auto', zIndex: 0 }}
    />
  );
}
