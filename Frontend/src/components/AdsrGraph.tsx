import React, { useState, useRef, useEffect } from 'react';

export interface AdsrGraphProps {
    attackMs: number;
    decayMs: number;
    sustainLevel: number;
    releaseMs: number;
    attackCurve: number;
    decayCurve: number;
    releaseCurve: number;
    onChange?: (updates: Partial<AdsrGraphProps>) => void;
}

export const AdsrGraph: React.FC<AdsrGraphProps> = ({
    attackMs, decayMs, sustainLevel, releaseMs,
    attackCurve, decayCurve, releaseCurve,
    onChange
}) => {
    const svgRef = useRef<SVGSVGElement>(null);
    const [draggingNode, setDraggingNode] = useState<string | null>(null);

    const width = 300;
    const height = 150;
    const maxTime = 2000; // 2 seconds max for visual scaling
    const sustainWidth = 50;
    const timeScale = (width - sustainWidth) / maxTime;

    // Node Points
    const ax = Math.min(attackMs * timeScale, (width - sustainWidth) * 0.33);
    const ay = 0;
    const dx = ax + Math.min(decayMs * timeScale, (width - sustainWidth) * 0.33);
    const dy = height - (sustainLevel * height);
    const sx = dx + sustainWidth;
    const sy = dy;
    const rx = Math.min(sx + Math.min(releaseMs * timeScale, (width - sustainWidth) * 0.33), width);
    const ry = height;

    // Helper to generate a curved SVG path segment
    const getCurveParams = (x1: number, y1: number, x2: number, y2: number, curve: number, isConvexPrioritized: boolean) => {
        const midX = x1 + (x2 - x1) * 0.5;
        const midY = y1 + (y2 - y1) * 0.5;
        const curveFactor = curve < 1.0 ? 1.0 - curve : curve - 1.0;
        const sag = (curve < 1.0 ? -1 : 1) * (isConvexPrioritized ? -1 : 1);
        const cx = midX;
        const cy = midY + (50 * curveFactor * sag);
        return { cx, cy };
    };

    const c1 = getCurveParams(0, height, ax, ay, attackCurve, false);
    const c2 = getCurveParams(ax, ay, dx, dy, decayCurve, true);
    const c3 = getCurveParams(sx, sy, rx, ry, releaseCurve, true);

    let pathD = `M 0,${height} `;
    pathD += curvePathString(0, height, ax, ay, attackCurve, false);
    pathD += curvePathString(ax, ay, dx, dy, decayCurve, true);
    pathD += ` L ${sx},${sy}`;
    pathD += curvePathString(sx, sy, rx, ry, releaseCurve, true);
    pathD += ` L ${width},${height}`;

    function curvePathString(x1: number, y1: number, x2: number, y2: number, curve: number, isConvexPrioritized: boolean) {
        if (curve === 1.0) return `L ${x2},${y2}`;
        const p = getCurveParams(x1, y1, x2, y2, curve, isConvexPrioritized);
        return `Q ${p.cx},${p.cy} ${x2},${y2}`;
    }

    const clamp = (val: number, min: number, max: number) => Math.max(min, Math.min(max, val));

    useEffect(() => {
        if (!draggingNode || !onChange) return;

        const onPointerMove = (e: PointerEvent) => {
            if (!svgRef.current) return;
            const rect = svgRef.current.getBoundingClientRect();
            const x = clamp(e.clientX - rect.left, 0, width);
            const y = clamp(e.clientY - rect.top, 0, height);

            if (draggingNode === 'attack') {
                const newMs = clamp(x / timeScale, 0, 5000);
                onChange({ attackMs: newMs });
            } else if (draggingNode === 'decay') {
                const newMs = clamp((x - ax) / timeScale, 0, 5000);
                onChange({ decayMs: newMs });
            } else if (draggingNode === 'sustain_level') {
                const newLvl = clamp(1.0 - (y / height), 0, 1);
                onChange({ sustainLevel: newLvl });
            } else if (draggingNode === 'release') {
                const newMs = clamp((x - sx) / timeScale, 0, 5000);
                onChange({ releaseMs: newMs });
            } else if (draggingNode === 'curve_attack') {
                const midY = height / 2;
                const dist = (y - midY) / 50;
                let c = 1.0 + dist;
                c = clamp(c, 0.1, 5.0);
                onChange({ attackCurve: c });
            } else if (draggingNode === 'curve_decay') {
                const midY = (ay + dy) / 2;
                const dist = (midY - y) / 50;
                let c = 1.0 + dist;
                c = clamp(c, 0.1, 5.0);
                onChange({ decayCurve: c });
            } else if (draggingNode === 'curve_release') {
                const midY = (sy + ry) / 2;
                const dist = (midY - y) / 50;
                let c = 1.0 + dist;
                c = clamp(c, 0.1, 5.0);
                onChange({ releaseCurve: c });
            }
        };

        const onPointerUp = () => setDraggingNode(null);

        window.addEventListener('pointermove', onPointerMove);
        window.addEventListener('pointerup', onPointerUp);
        return () => {
            window.removeEventListener('pointermove', onPointerMove);
            window.removeEventListener('pointerup', onPointerUp);
        };
    }, [draggingNode, onChange, timeScale, ax, ay, dx, dy, sx, sy, rx, ry]);

    return (
        <svg
            ref={svgRef}
            width={width}
            height={height}
            viewBox={`0 0 ${width} ${height}`}
            className="adsr-graph"
            style={{ touchAction: 'none' }}
        >
            <path d={pathD} fill="rgba(33, 150, 243, 0.1)" stroke="#2196F3" strokeWidth="2" />
            <path d={`${pathD} L 0,${height} Z`} fill="rgba(33, 150, 243, 0.1)" stroke="none" />

            {/* Draggable Curve Tension Handles */}
            <circle cx={c1.cx} cy={c1.cy} r="6" fill="rgba(255,165,0,0.8)" style={{ cursor: 'ns-resize' }} onPointerDown={() => setDraggingNode('curve_attack')} />
            <circle cx={c2.cx} cy={c2.cy} r="6" fill="rgba(255,165,0,0.8)" style={{ cursor: 'ns-resize' }} onPointerDown={() => setDraggingNode('curve_decay')} />
            <circle cx={c3.cx} cy={c3.cy} r="6" fill="rgba(255,165,0,0.8)" style={{ cursor: 'ns-resize' }} onPointerDown={() => setDraggingNode('curve_release')} />

            {/* Draggable Time/Level Handles */}
            <circle cx={ax} cy={ay} r="6" fill="#64B5F6" style={{ cursor: 'ew-resize' }} onPointerDown={() => setDraggingNode('attack')} />
            <circle cx={dx} cy={dy} r="6" fill="#64B5F6" style={{ cursor: 'move' }} onPointerDown={() => setDraggingNode('decay')} />

            {/* Sustain Line Drag Target (invisible thick line) */}
            <line x1={dx} y1={dy} x2={sx} y2={sy} stroke="transparent" strokeWidth="12" style={{ cursor: 'ns-resize' }} onPointerDown={() => setDraggingNode('sustain_level')} />
            <circle cx={dx} cy={dy} r="4" fill="#64B5F6" style={{ pointerEvents: 'none' }} />
            <circle cx={sx} cy={sy} r="6" fill="#64B5F6" style={{ cursor: 'ns-resize' }} onPointerDown={() => setDraggingNode('sustain_level')} />

            <circle cx={rx} cy={ry} r="6" fill="#64B5F6" style={{ cursor: 'ew-resize' }} onPointerDown={() => setDraggingNode('release')} />
        </svg>
    );
};
