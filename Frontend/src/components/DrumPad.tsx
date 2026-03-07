import React from "react";

export interface PadConfig {
    sampleName: string | null;
    audioUrl?: string;
    offsetMs: number;
    endMs: number;
    loopEnabled: boolean;
    loopStartMs: number;
    loopEndMs: number;
    reverse: boolean;
    attackMs: number;
    decayMs: number;
    sustainLevel: number;
    releaseMs: number;
    attackCurve: number;
    decayCurve: number;
    releaseCurve: number;
    gainLevel: number;
}

interface DrumPadProps {
    padIndex: number;
    config: PadConfig;
    onFileDrop: (padIndex: number, files: File[]) => void;
    onEditClick: (padIndex: number) => void;
    onPadClick: (padIndex: number) => void;
}

export const DrumPad: React.FC<DrumPadProps> = ({ padIndex, config, onFileDrop, onEditClick, onPadClick }) => {
    const midiNote = 20 + padIndex;

    // Convert MIDI to Note Name string (C1 = 36)
    const noteNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"];
    const octave = Math.floor(midiNote / 12) - 2;
    const noteName = `${noteNames[midiNote % 12]}${octave}`;

    const handleDragOver = (e: React.DragEvent) => {
        e.preventDefault();
        e.currentTarget.classList.add('drag-over');
    };

    const handleDragLeave = (e: React.DragEvent) => {
        e.preventDefault();
        e.currentTarget.classList.remove('drag-over');
    };

    const handleDrop = (e: React.DragEvent) => {
        e.preventDefault();
        e.currentTarget.classList.remove('drag-over');
        if (e.dataTransfer.files && e.dataTransfer.files.length > 0) {
            onFileDrop(padIndex, Array.from(e.dataTransfer.files));
        }
    };

    return (
        <div
            className={`drum-pad ${config.sampleName ? 'loaded' : ''}`}
            onDragOver={handleDragOver}
            onDragLeave={handleDragLeave}
            onDrop={handleDrop}
            onClick={() => onPadClick(padIndex)}
        >
            <div className="pad-header">
                <span className="pad-number">Pad {padIndex + 1}</span>
                <span className="midi-note">{midiNote} ({noteName})</span>
            </div>

            <div className="pad-content">
                <span className="sample-name">
                    {config.sampleName ? config.sampleName : "Drop Audio"}
                </span>
            </div>
            <div className="pad-actions">
                <button
                    className="edit-pad-btn"
                    onClick={(e) => { e.stopPropagation(); onEditClick(padIndex); }}
                    disabled={!config.sampleName}
                >
                    Edit
                </button>
            </div>
        </div>
    );
};
