<!-- コメントは消さずに残しておく -->
## WavDrumVST
<!-- ↑ここは自動で入力されるので##のままにする -->
<!-- 説明文(何をする目的のVSTか。ユーザは何を入力して、何を得られるのか。どういうことに使えるか。) -->
WavDrumVSTは、112個のパッドに任意のオーディオファイル（WAVファイルなど）を割り当てて演奏できるMIDIドラムサンプラープラグインです。
手持ちのドラムサンプルや効果音をドラッグ＆ドロップまたはクリックでパッドに読み込ませることで、MIDIキーボードやDAWのシーケンサーから指定した音をトリガーできるようになります。
また、プラグイン内で波形の再生開始・終了位置の調整、ループ、リバース、ADSRエンベロープによる音量変化などを細かくエディットできるため、独自のオリジナルドラムキットを素早く構築して楽曲制作に活用することが可能です。
<!-- 説明文 -->
### Downloads
[VST3](./build/WavDrumVST_artefacts/Release/VST3/WavDrumVST.vst3), [AU](./build/WavDrumVST_artefacts/Release/AU/WavDrumVST.component), [Standalone](./build/WavDrumVST_artefacts/Release/Standalone/WavDrumVST.app)
<!-- ↑ここは自動で入力されるので[VST3](./build/WavDrumVST_artefacts/Release/VST3/WavDrumVST.vst3), [AU](./build/WavDrumVST_artefacts/Release/AU/WavDrumVST.component), [Standalone](./build/WavDrumVST_artefacts/Release/Standalone/WavDrumVST.app)のままにする -->
### How to use
<!-- 使い方 -->
![HowToUse](/figures/Howtouse.png)  
割り当てたいパッドをクリックして、オーディオファイルを割り当てる  
「Edit」をクリックするとオーディオの簡単な編集ができる
![HowToUseModifier](/figures/HowtouseModifier.png)  
音の始まりと終わり、逆再生を設定できる  
![HowToUseADSR](/figures/HowtouseADSR.png)  
音量変化をADSRで設定できる  
<!-- 使い方 -->
### Usage example
<!-- 使用例 -->
- **オリジナルドラムキットの構築:** 手持ちのキック、スネア、ハイハットなどのWAV素材を各パッドに割り当て、不要な無音部分をエディタでカット。ADSRを調整してキレのあるドラム音源として使用。
- **効果音や声ネタのサンプラーとして:** 楽曲のアクセントとなる環境音やボーカルフレーズ、SEなどをアサインし、MIDIノートタイミングでポン出しする。リバース機能を使ってビルドアップのFXを作るなどの応用も可能。
- **プリセットデータの管理・共有:** 作成したお気に入りのキット構成は、保存（Export）や読み込み（Import）ができるため、お気に入りのドラムセットをいつでも呼び出して利用。
<!-- 使用例 -->