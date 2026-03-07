import React from 'react';
import type { PadConfig } from './DrumPad';
import { AdsrGraph } from './AdsrGraph';
import { WaveformEditor } from './WaveformEditor';
import { getNativeFunction } from '../juce_interop';
import { b64toBlob } from './DrumRack';

const previewDrumPadNative = getNativeFunction("previewDrumPad");
const getPadAudioBase64Native = getNativeFunction("getPadAudioBase64");

interface AudioEditModalProps {
    padIndex: number;
    config: PadConfig;
    onConfigChange: (padIndex: number, newConfig: PadConfig) => void;
    onClose: () => void;
}

export const AudioEditModal: React.FC<AudioEditModalProps> = ({ padIndex, config, onConfigChange, onClose }) => {
    const handlePreviewDown = () => previewDrumPadNative(padIndex, true);
    const handlePreviewUp = () => previewDrumPadNative(padIndex, false);

    React.useEffect(() => {
        let mounted = true;
        if (!config.audioUrl && config.sampleName) {
            getPadAudioBase64Native(padIndex).then((b64: any) => {
                if (mounted && typeof b64 === 'string' && b64.length > 0) {
                    const blob = b64toBlob(b64, "audio/wav");
                    const url = URL.createObjectURL(blob);
                    onConfigChange(padIndex, { ...config, audioUrl: url });
                }
            }).catch((err: any) => console.error("Failed to load async base64", err));
        }
        return () => { mounted = false; };
    }, [padIndex, config.sampleName, config.audioUrl]);

    return (
        <div className="modal-overlay" onClick={onClose}>
            <div className="audio-edit-modal" onClick={e => e.stopPropagation()}>
                <header>
                    <div className="header-title">
                        <h3>Edit Pad {padIndex + 1} - {config.sampleName}</h3>
                        <button
                            className="preview-btn"
                            onPointerDown={handlePreviewDown}
                            onPointerUp={handlePreviewUp}
                            onPointerLeave={handlePreviewUp}
                        >
                            <span className="play-icon">▶</span> Preview
                        </button>
                    </div>
                    <button className="close-btn" onClick={onClose}>&times;</button>
                </header>

                <div className="edit-sections">
                    <section className="waveform-section">
                        <h4>Sample Playback</h4>
                        <WaveformEditor config={config} onConfigChange={(c) => onConfigChange(padIndex, c)} />
                        <div className="controls-grid">
                            <label>
                                Start (ms):
                                <input type="number" min={0} value={config.offsetMs} onChange={e => onConfigChange(padIndex, { ...config, offsetMs: Number(e.target.value) })} />
                            </label>
                            <label>
                                End (ms, -1 for full):
                                <input type="number" min={-1} value={config.endMs} onChange={e => onConfigChange(padIndex, { ...config, endMs: Number(e.target.value) })} />
                            </label>
                            <label className="checkbox-label">
                                <input type="checkbox" checked={config.reverse} onChange={e => onConfigChange(padIndex, { ...config, reverse: e.target.checked })} />
                                Reverse Playback
                            </label>

                            <h4>Looping</h4>
                            <label className="checkbox-label">
                                <input type="checkbox" checked={config.loopEnabled} onChange={e => onConfigChange(padIndex, { ...config, loopEnabled: e.target.checked })} />
                                Enable Loop
                            </label>
                            <label>
                                Loop Start (ms):
                                <input type="number" min={0} value={config.loopStartMs} onChange={e => onConfigChange(padIndex, { ...config, loopStartMs: Number(e.target.value) })} disabled={!config.loopEnabled} />
                            </label>
                            <label>
                                Loop End (ms):
                                <input type="number" min={0} value={config.loopEndMs} onChange={e => onConfigChange(padIndex, { ...config, loopEndMs: Number(e.target.value) })} disabled={!config.loopEnabled} />
                            </label>
                        </div>
                    </section>

                    <section className="adsr-section">
                        <h4>ADSR Envelope</h4>
                        <div className="adsr-graph-container">
                            <AdsrGraph
                                attackMs={config.attackMs}
                                decayMs={config.decayMs}
                                sustainLevel={config.sustainLevel}
                                releaseMs={config.releaseMs}
                                attackCurve={config.attackCurve}
                                decayCurve={config.decayCurve}
                                releaseCurve={config.releaseCurve}
                                onChange={(updates) => onConfigChange(padIndex, { ...config, ...updates })}
                            />
                        </div>
                        <div className="controls-grid">
                            <label>
                                Attack (ms):
                                <input type="range" min="0" max="2000" value={config.attackMs} onChange={e => onConfigChange(padIndex, { ...config, attackMs: Number(e.target.value) })} />
                                <span>{config.attackMs}</span>
                            </label>
                            <label>
                                Decay (ms):
                                <input type="range" min="0" max="2000" value={config.decayMs} onChange={e => onConfigChange(padIndex, { ...config, decayMs: Number(e.target.value) })} />
                                <span>{config.decayMs}</span>
                            </label>
                            <label>
                                Sustain Level:
                                <input type="range" min="0" max="1" step="0.01" value={config.sustainLevel} onChange={e => onConfigChange(padIndex, { ...config, sustainLevel: Number(e.target.value) })} />
                                <span>{config.sustainLevel.toFixed(2)}</span>
                            </label>
                            <label>
                                Release (ms):
                                <input type="range" min="0" max="5000" value={config.releaseMs} onChange={e => onConfigChange(padIndex, { ...config, releaseMs: Number(e.target.value) })} />
                                <span>{config.releaseMs}</span>
                            </label>
                            <label>
                                Volume:
                                <input type="range" min="0" max="2" step="0.01" value={config.gainLevel} onChange={e => onConfigChange(padIndex, { ...config, gainLevel: Number(e.target.value) })} />
                                <span>{config.gainLevel.toFixed(2)}</span>
                            </label>

                            <h4 style={{ gridColumn: 'span 2', marginTop: '16px' }}>Curve Tension</h4>
                            <label>
                                Attack Curve (-log to +exp):
                                <input type="range" min="0.1" max="5.0" step="0.1" value={config.attackCurve} onChange={e => onConfigChange(padIndex, { ...config, attackCurve: Number(e.target.value) })} />
                                <span>{config.attackCurve.toFixed(1)}</span>
                            </label>
                            <label>
                                Decay Curve (-log to +exp):
                                <input type="range" min="0.1" max="5.0" step="0.1" value={config.decayCurve} onChange={e => onConfigChange(padIndex, { ...config, decayCurve: Number(e.target.value) })} />
                                <span>{config.decayCurve.toFixed(1)}</span>
                            </label>
                            <label>
                                Release Curve (-log to +exp):
                                <input type="range" min="0.1" max="5.0" step="0.1" value={config.releaseCurve} onChange={e => onConfigChange(padIndex, { ...config, releaseCurve: Number(e.target.value) })} />
                                <span>{config.releaseCurve.toFixed(1)}</span>
                            </label>
                        </div>
                    </section>
                </div>
            </div>
        </div>
    );
};
