#!/bin/bash
#chmod +x collect_samples.sh

if [ "$#" -ne 2 ]; then
    echo "使用法: $0 <ソースディレクトリ> <出力ディレクトリ>"
    exit 1
fi

SRC_DIR="$1"
DEST_DIR="$2"

# 2. 環境に応じたシャッフルコマンドの決定
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS の場合
    if command -v gshuf >/dev/null 2>&1; then
        SHUF_CMD="gshuf"
    else
        echo "エラー: gshuf が見つかりません。'brew install coreutils' でインストールしてください。"
        exit 1
    fi
else
    # Linux 等、その他の場合
    if command -v shuf >/dev/null 2>&1; then
        SHUF_CMD="shuf"
    else
        echo "エラー: shuf コマンドが見つかりません。"
        exit 1
    fi
fi

# 3. 出力ディレクトリの作成
mkdir -p "$DEST_DIR"

# 音楽的なノート順（クロマチックスケール）
NOTES=("C" "C#" "D" "D#" "E" "F" "F#" "G" "G#" "A" "A#" "B")

# 連番用カウンター
idx=1

echo "環境判別: 使用コマンド -> $SHUF_CMD"
echo "処理を開始します..."

# 4. オクターブとノートのループ
for octave in {-2..3}; do
    for note in "${NOTES[@]}"; do
        DIR_NAME="${note}${octave}"
        TARGET_PATH="${SRC_DIR}/${DIR_NAME}"

        if [ -d "$TARGET_PATH" ]; then
            # 検知したシャッフルコマンドを使用して1つ選択
            SELECTED_FILE=$(find "$TARGET_PATH" -maxdepth 1 -type f \( -iname "*.wav" -o -iname "*.mp3" \) | $SHUF_CMD -n 1)

            if [ -n "$SELECTED_FILE" ]; then
                BASE_NAME=$(basename "$SELECTED_FILE")
                PREFIX=$(printf "%03d" $idx)
                
                # コピー実行
                cp "$SELECTED_FILE" "${DEST_DIR}/${PREFIX}_${BASE_NAME}"
                
                echo "[$PREFIX] ${DIR_NAME} -> ${BASE_NAME}"
                idx=$((idx + 1))
            fi
        fi
    done
done

echo "---------------------------------------"
echo "完了: $((idx - 1)) 個のファイルを ${DEST_DIR} に整理しました。"