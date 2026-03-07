#!/bin/bash
#chmod +x Run.sh && ./Run.sh

BUILD_TYPE=${1:-Debug}
AudioPluginHost=/path/to/AudioPluginHost

if [ $BUILD_TYPE = "Debug" ]; then
    # フロントエンドのDevサーバーをバックグラウンドで起動
    cd Frontend
    npm run dev &
    DEV_PID=$!
    cd ..
    $AudioPluginHost
    kill $DEV_PID
else
    $AudioPluginHost
fi