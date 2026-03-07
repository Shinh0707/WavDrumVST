import React, { useState, useCallback } from "react";
import { DrumPad } from "./DrumPad";
import type { PadConfig } from "./DrumPad";
import { AudioEditModal } from "./AudioEditModal";
import { getNativeFunction } from "../juce_interop";

const loadDrumSampleNative = getNativeFunction("loadDrumSample");
const updatePadConfigNative = getNativeFunction("updatePadConfig");
const requestFullStateNative = getNativeFunction("requestFullState");
//const getPadAudioBase64Native = getNativeFunction("getPadAudioBase64");
const openFileDialogNative = getNativeFunction("openFileDialog");
const savePluginStateNative = getNativeFunction("savePluginState");
const loadPluginStateNative = getNativeFunction("loadPluginState");

export function b64toBlob(b64Data: string, contentType = '', sliceSize = 512): Blob {
    const byteCharacters = atob(b64Data);
    const byteArrays = [];
    for (let offset = 0; offset < byteCharacters.length; offset += sliceSize) {
        const slice = byteCharacters.slice(offset, offset + sliceSize);
        const byteNumbers = new Array(slice.length);
        for (let i = 0; i < slice.length; i++) {
            byteNumbers[i] = slice.charCodeAt(i);
        }
        const byteArray = new Uint8Array(byteNumbers);
        byteArrays.push(byteArray);
    }
    return new Blob(byteArrays, { type: contentType });
}

export const DrumRack: React.FC = () => {
    const [pads, setPads] = useState<PadConfig[]>(
        Array(112).fill({
            sampleName: null, audioUrl: undefined, offsetMs: 0, endMs: -1,
            loopEnabled: false, loopStartMs: 0, loopEndMs: 0,
            reverse: false, attackMs: 0, decayMs: 0, sustainLevel: 1.0, releaseMs: 100,
            attackCurve: 1.0, decayCurve: 1.0, releaseCurve: 1.0, gainLevel: 1.0
        })
    );
    const [editingPadIndex, setEditingPadIndex] = useState<number | null>(null);
    const [currentPage, setCurrentPage] = useState(1);
    const [errorMessage, setErrorMessage] = useState<string | null>(null);

    const processStatePayload = useCallback((jsonStr: any) => {
        if (!jsonStr || typeof jsonStr !== 'string') return;
        try {
            const data = JSON.parse(jsonStr);
            if (data && data.pads && Array.isArray(data.pads)) {
                setPads(prev => {
                    const newPads = [...prev];
                    data.pads.forEach((p: any) => {
                        const idx = p.midiNote - 20;
                        if (idx >= 0 && idx < 112) {
                            let newAudioUrl = undefined;
                            newPads[idx] = {
                                sampleName: p.sampleName || null,
                                audioUrl: newAudioUrl,
                                offsetMs: p.offsetMs ?? 0,
                                endMs: p.endMs ?? -1,
                                loopEnabled: p.loopEnabled ?? false,
                                loopStartMs: p.loopStartMs ?? 0,
                                loopEndMs: p.loopEndMs ?? 0,
                                reverse: p.reverse ?? false,
                                attackMs: p.attackMs ?? 0,
                                decayMs: p.decayMs ?? 0,
                                sustainLevel: p.sustainLevel ?? 1.0,
                                releaseMs: p.releaseMs ?? 100,
                                attackCurve: p.attackCurve ?? 1.0,
                                decayCurve: p.decayCurve ?? 1.0,
                                releaseCurve: p.releaseCurve ?? 1.0,
                                gainLevel: p.gainLevel ?? 1.0
                            };
                        }
                    });
                    return newPads;
                });
            }
        } catch (e) {
            console.error("Failed to parse full state", e);
        }
    }, []);

    React.useEffect(() => {
        let mounted = true;
        requestFullStateNative().then((jsonStr: any) => {
            if (mounted) processStatePayload(jsonStr);
        });
        return () => { mounted = false; };
    }, [processStatePayload]);

    const handleExportLight = useCallback(() => {
        savePluginStateNative(false).catch((err: any) => console.error("Export Light failed", err));
    }, []);

    const handleExportFull = useCallback(() => {
        savePluginStateNative(true).catch((err: any) => console.error("Export Full failed", err));
    }, []);

    const handleImport = useCallback(() => {
        loadPluginStateNative().then((newStateJson: any) => {
            if (newStateJson) processStatePayload(newStateJson);
        }).catch((err: any) => console.error("Import failed", err));
    }, [processStatePayload]);

    const handleFileDrop = useCallback((padIndex: number, files: File[]) => {
        files.forEach((file, idx) => {
            const targetPad = padIndex + idx;
            if (targetPad >= 112) return;

            const objectUrl = URL.createObjectURL(file);
            const reader = new FileReader();
            reader.onload = async (e) => {
                const dataUrl = e.target?.result as string;
                if (dataUrl) {
                    const base64Data = dataUrl.split(',')[1];
                    if (base64Data) {
                        try {
                            const config = pads[targetPad];
                            await loadDrumSampleNative(
                                targetPad, file.name, base64Data, config.offsetMs, config.endMs,
                                config.loopEnabled, config.loopStartMs, config.loopEndMs,
                                config.reverse, config.attackMs, config.decayMs,
                                config.sustainLevel, config.releaseMs,
                                config.attackCurve, config.decayCurve, config.releaseCurve, config.gainLevel
                            );

                            setPads((prev) => {
                                const newPads = [...prev];
                                newPads[targetPad] = { ...newPads[targetPad], sampleName: file.name, audioUrl: objectUrl };
                                return newPads;
                            });
                        } catch (error) {
                            console.error("Failed to load sample via JUCE backend", error);
                        }
                    }
                }
            };
            reader.readAsDataURL(file);
        });
    }, [pads]);

    const handlePadClick = async (padIndex: number) => {
        try {
            const result = await openFileDialogNative(padIndex);

            if (result && result.error) {
                setErrorMessage("読み込みエラー: " + result.error);
                return;
            }

            if (result && result.sampleName) {
                // Base64の変換処理を全削除し、直接C++の波形提供URLを叩く
                // ※キャッシュによって古い波形が表示されないようにタイムスタンプ(t)を付与します
                const url = `/dynamic_audio/${padIndex}?t=${Date.now()}`;

                const newPads = [...pads];
                newPads[padIndex] = {
                    ...newPads[padIndex],
                    sampleName: result.sampleName,
                    audioUrl: url,
                    offsetMs: 0,
                    endMs: -1,
                    loopEnabled: false,
                    loopStartMs: 0,
                    loopEndMs: 0,
                    reverse: false,
                    attackMs: 0,
                    decayMs: 0,
                    sustainLevel: 1.0,
                    releaseMs: 0.1,
                    attackCurve: 1.0,
                    decayCurve: 1.0,
                    releaseCurve: 1.0,
                    gainLevel: 1.0
                };
                setPads(newPads);
                setErrorMessage(null);
            }
        } catch (error) {
            setErrorMessage("ダイアログの通信処理でエラーが発生しました。");
        }
    };

    const handleConfigChange = useCallback((padIndex: number, newConfig: PadConfig) => {
        setPads((prev) => {
            const newPads = [...prev];
            newPads[padIndex] = newConfig;
            return newPads;
        });

        updatePadConfigNative(
            padIndex, newConfig.offsetMs, newConfig.endMs,
            newConfig.loopEnabled, newConfig.loopStartMs, newConfig.loopEndMs,
            newConfig.reverse, newConfig.attackMs, newConfig.decayMs,
            newConfig.sustainLevel, newConfig.releaseMs,
            newConfig.attackCurve, newConfig.decayCurve, newConfig.releaseCurve, newConfig.gainLevel
        ).catch(err => console.error("Failed to update config on JUCE backend", err));
    }, []);

    return (
        <div className="drum-rack-container">
            {/* === 追加: エラー表示用のフローティングバナー === */}
            {errorMessage && (
                <div style={{
                    position: 'fixed',
                    top: '20px',
                    left: '50%',
                    transform: 'translateX(-50%)',
                    backgroundColor: '#d32f2f',
                    color: 'white',
                    padding: '12px 24px',
                    borderRadius: '8px',
                    zIndex: 9999,
                    display: 'flex',
                    alignItems: 'center',
                    gap: '16px',
                    boxShadow: '0 4px 12px rgba(0,0,0,0.5)',
                    fontWeight: 'bold'
                }}>
                    <span>{errorMessage}</span>
                    <button
                        onClick={() => setErrorMessage(null)}
                        style={{ background: 'transparent', border: '1px solid white', color: 'white', borderRadius: '4px', cursor: 'pointer', padding: '4px 8px' }}
                    >
                        閉じる
                    </button>
                </div>
            )}

            <div className="rack-header">
                <h2>WavDrum VST - MIDI Drum Sampler</h2>
                <div style={{ display: 'flex', gap: '8px', alignItems: 'center' }}>
                    <div className="preset-controls" style={{ display: 'flex', gap: '4px', marginRight: '16px' }}>
                        <button className="page-btn" style={{ borderColor: '#4CAF50', color: '#4CAF50' }} onClick={handleExportLight}>Export (Light)</button>
                        <button className="page-btn" style={{ borderColor: '#2196F3', color: '#2196F3' }} onClick={handleExportFull}>Export (Full)</button>
                        <button className="page-btn" style={{ borderColor: '#FF9800', color: '#FF9800' }} onClick={handleImport}>Import</button>
                    </div>
                    <div className="pagination-controls">
                        {[
                            { index: 0, label: "< C1" },
                            { index: 1, label: "C1 ~" },
                            { index: 2, label: "E2 ~" },
                            { index: 3, label: "G#3 ~" },
                            { index: 4, label: "C5 ~" },
                            { index: 5, label: "E6 ~" },
                            { index: 6, label: "G#7 ~" }
                        ].map(page => (
                            <button
                                key={page.index}
                                className={`page-btn ${currentPage === page.index ? 'active' : ''}`}
                                onClick={() => setCurrentPage(page.index)}
                            >
                                {page.label}
                            </button>
                        ))}
                    </div>
                </div>
            </div>

            <div className="drum-rack-grid">
                {pads.slice(currentPage * 16, (currentPage + 1) * 16).map((config, index) => {
                    const actualIndex = (currentPage * 16) + index;
                    return (
                        <DrumPad
                            key={actualIndex}
                            padIndex={actualIndex}
                            config={config}
                            onFileDrop={handleFileDrop}
                            onEditClick={setEditingPadIndex}
                            onPadClick={handlePadClick}
                        />
                    );
                })}
            </div>

            {editingPadIndex !== null && (
                <AudioEditModal
                    padIndex={editingPadIndex}
                    config={pads[editingPadIndex]}
                    onConfigChange={handleConfigChange}
                    onClose={() => setEditingPadIndex(null)}
                />
            )}
        </div>
    );
};
