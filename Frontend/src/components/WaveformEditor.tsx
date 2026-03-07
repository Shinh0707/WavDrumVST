import React, { useEffect, useRef, useState, useCallback } from 'react';
import type { PadConfig } from './DrumPad';

interface WaveformEditorProps {
    config: PadConfig;
    onConfigChange: (newConfig: PadConfig) => void;
}

export const WaveformEditor: React.FC<WaveformEditorProps> = ({ config, onConfigChange }) => {
    const canvasRef = useRef<HTMLCanvasElement>(null);
    const containerRef = useRef<HTMLDivElement>(null);
    const [audioBuffer, setAudioBuffer] = useState<AudioBuffer | null>(null);
    const [durationMs, setDurationMs] = useState<number>(1000); // 1s default
    const [isDragging, setIsDragging] = useState<string | null>(null);

    // Fetch and decode audio
    useEffect(() => {
        if (!config.audioUrl) return;

        const loadAudio = async () => {
            try {
                const response = await fetch(config.audioUrl!);
                const arrayBuffer = await response.arrayBuffer();
                const audioContext = new (window.AudioContext || (window as any).webkitAudioContext)();
                const buffer = await audioContext.decodeAudioData(arrayBuffer);
                setAudioBuffer(buffer);
                setDurationMs(buffer.duration * 1000);
            } catch (error) {
                console.error("Failed to decode audio URL", error);
            }
        };
        loadAudio();
    }, [config.audioUrl]);

    // Draw waveform
    useEffect(() => {
        if (!audioBuffer || !canvasRef.current) return;
        const canvas = canvasRef.current;
        const ctx = canvas.getContext('2d');
        if (!ctx) return;

        const width = canvas.width;
        const height = canvas.height;
        ctx.clearRect(0, 0, width, height);

        const channelData = audioBuffer.getChannelData(0); // use left channel
        const step = Math.ceil(channelData.length / width);
        const amp = height / 2;

        ctx.fillStyle = "#8a8f9c";
        for (let i = 0; i < width; i++) {
            let min = 1.0;
            let max = -1.0;
            for (let j = 0; j < step; j++) {
                const datum = channelData[(i * step) + j];
                if (datum < min) min = datum;
                if (datum > max) max = datum;
            }
            ctx.fillRect(i, (1 + min) * amp, 1, Math.max(1, (max - min) * amp));
        }

        // Darken unavailable range
        ctx.fillStyle = "rgba(0, 0, 0, 0.5)";

        const startX = (config.offsetMs / durationMs) * width;
        const endX = config.endMs === -1 ? width : (config.endMs / durationMs) * width;

        // Draw out of bounds dark masks
        ctx.fillRect(0, 0, startX, height);
        if (endX < width) {
            ctx.fillRect(endX, 0, width - endX, height);
        }

        // Draw loop region
        if (config.loopEnabled) {
            const loopStartX = (config.loopStartMs / durationMs) * width;
            const loopEndX = (config.loopEndMs / durationMs) * width;
            ctx.fillStyle = "rgba(76, 175, 80, 0.2)"; // Green tint
            ctx.fillRect(loopStartX, 0, loopEndX - loopStartX, height);
        }

    }, [audioBuffer, config.offsetMs, config.endMs, config.loopEnabled, config.loopStartMs, config.loopEndMs, durationMs]);

    // Convert mouse event X to corresponding Ms based on canvas width
    const getMsFromEvent = useCallback((e: React.MouseEvent | MouseEvent) => {
        if (!containerRef.current) return 0;
        const rect = containerRef.current.getBoundingClientRect();
        const x = Math.max(0, Math.min(e.clientX - rect.left, rect.width));
        return (x / rect.width) * durationMs;
    }, [durationMs]);

    const handlePointerDown = (e: React.PointerEvent, handleId: string) => {
        e.stopPropagation();
        (e.target as HTMLElement).setPointerCapture(e.pointerId);
        setIsDragging(handleId);
    };

    const handlePointerMove = (e: React.PointerEvent) => {
        if (!isDragging) return;
        const ms = getMsFromEvent(e);

        switch (isDragging) {
            case 'start':
                onConfigChange({ ...config, offsetMs: ms });
                break;
            case 'end':
                onConfigChange({ ...config, endMs: ms });
                break;
            case 'loopStart':
                onConfigChange({ ...config, loopStartMs: ms });
                break;
            case 'loopEnd':
                onConfigChange({ ...config, loopEndMs: ms });
                break;
        }
    };

    const handlePointerUp = (e: React.PointerEvent) => {
        if (isDragging) {
            (e.target as HTMLElement).releasePointerCapture(e.pointerId);
            setIsDragging(null);
        }
    };

    const getXPosition = (ms: number) => {
        if (ms === -1) return "100%";
        return `${(ms / durationMs) * 100}%`;
    };

    if (!config.audioUrl) {
        return <div className="waveform-placeholder">No Audio Data (Drop a new file to enable Waveform view)</div>;
    }

    return (
        <div
            className="waveform-editor"
            ref={containerRef}
            onPointerMove={handlePointerMove}
            onPointerUp={handlePointerUp}
            onPointerCancel={handlePointerUp}
        >
            <canvas ref={canvasRef} width={800} height={150} className="waveform-canvas" />

            {/* Playback Handles */}
            <div
                className="handle handle-start"
                style={{ left: getXPosition(config.offsetMs) }}
                onPointerDown={(e) => handlePointerDown(e, 'start')}
            ></div>
            <div
                className="handle handle-end"
                style={{ left: getXPosition(config.endMs) }}
                onPointerDown={(e) => handlePointerDown(e, 'end')}
            ></div>

            {/* Loop Handles */}
            {config.loopEnabled && (
                <>
                    <div
                        className="handle loop-handle loop-start"
                        style={{ left: getXPosition(config.loopStartMs) }}
                        onPointerDown={(e) => handlePointerDown(e, 'loopStart')}
                    ></div>
                    <div
                        className="handle loop-handle loop-end"
                        style={{ left: getXPosition(config.loopEndMs) }}
                        onPointerDown={(e) => handlePointerDown(e, 'loopEnd')}
                    ></div>
                </>
            )}
        </div>
    );
};
