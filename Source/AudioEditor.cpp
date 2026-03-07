#include "AudioEditor.h"
#if !JUCE_DEBUG
#include "BinaryData.h"
#endif

WebPluginAudioProcessorEditor::WebPluginAudioProcessorEditor(
    WebPluginAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p) {
  auto options =
      juce::WebBrowserComponent::Options().withNativeIntegrationEnabled(true);

  options =
      options.withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
          .withWinWebView2Options(
              juce::WebBrowserComponent::Options::WinWebView2{}
                  .withUserDataFolder(juce::File::getSpecialLocation(
                      juce::File::SpecialLocationType::tempDirectory)))
          .withAppleWkWebViewOptions(
              juce::WebBrowserComponent::Options::AppleWkWebView{}
                  .withAllowAccessToEnclosingDirectory(true));
#if !JUCE_DEBUG
  auto getMimeType = [](const juce::String &url) -> juce::String {
    if (url.endsWithIgnoreCase(".html"))
      return "text/html";
    if (url.endsWithIgnoreCase(".js"))
      return "text/javascript";
    if (url.endsWithIgnoreCase(".css"))
      return "text/css";
    if (url.endsWithIgnoreCase(".svg"))
      return "image/svg+xml";
    return "application/octet-stream";
  };

  options = options.withResourceProvider(
      [getMimeType, &p](const juce::String &url)
          -> std::optional<juce::WebBrowserComponent::Resource> {
        // ルートへのアクセスは index.html とする
        juce::String reqUrl = url == "/" ? "index.html" : url;
        if (reqUrl.startsWith("/"))
          reqUrl = reqUrl.substring(1);
        if (reqUrl.startsWith("dynamic_audio/")) {
          // URLからパッド番号を抽出（キャッシュ対策の ?t=123 等を取り除く）
          int padIdx = reqUrl.substring(14)
                           .upToFirstOccurrenceOf("?", false, false)
                           .getIntValue();
          juce::String path = p.getFilePathForPad(padIdx);
          juce::File f(path);

          if (f.existsAsFile()) {
            juce::MemoryBlock mb;
            f.loadFileAsData(mb);
            const auto *byteData =
                reinterpret_cast<const std::byte *>(mb.getData());
            std::vector<std::byte> vecData(byteData, byteData + mb.getSize());
            return juce::WebBrowserComponent::Resource{std::move(vecData),
                                                       getMimeType(path)};
          }
          return std::nullopt;
        }
        // ディレクトリパスが含まれている場合("assets/xxx.js"など)、ファイル名のみを抽出する
        juce::String fileName = reqUrl.fromLastOccurrenceOf("/", false, false);
        if (fileName.isEmpty())
          fileName = reqUrl;

        // JUCEのBinaryData仕様に合わせて、英数字以外を '_' に置換する
        juce::String resourceName;
        for (auto c : fileName) {
          if (juce::CharacterFunctions::isLetterOrDigit(c)) {
            resourceName += c;
          } else {
            resourceName += '_';
          }
        }

        // C++の識別子は数字から始められないため、数字で始まる場合は先頭に '_'
        // を追加（JUCEの仕様）
        if (resourceName.isNotEmpty() &&
            juce::CharacterFunctions::isDigit(resourceName[0])) {
          resourceName = "_" + resourceName;
        }

        if (resourceName.isNotEmpty()) {
          int dataSize = 0;
          if (const char *data = BinaryData::getNamedResource(
                  resourceName.toRawUTF8(), dataSize)) {
            // 追加：const char* を std::byte のベクターに変換
            const auto *byteData = reinterpret_cast<const std::byte *>(data);
            std::vector<std::byte> vecData(byteData, byteData + dataSize);

            return juce::WebBrowserComponent::Resource{std::move(vecData),
                                                       getMimeType(reqUrl)};
          }
        }
        return std::nullopt;
      });
#endif

  // 1. Web UIからのステート受信（随時）
  options = options.withNativeFunction(
      "updatePluginState",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0) {
          audioProcessor.pluginStateJson = args[0].toString();
          completion("State Updated");
        } else {
          completion(juce::var::undefined());
        }
      });

  // 2. ステートのファイル保存
  // (DAWの外に個別のプリセットとして保存したい場合など)
  options = options.withNativeFunction(
      "savePluginState",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0) {
          bool includeAudio = args[0];
          juce::String payload =
              audioProcessor.getStateAsJsonString(includeAudio);
          chooser = std::make_unique<juce::FileChooser>(
              "Save State",
              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                  .getChildFile("state.json"),
              "*.json");
          chooser->launchAsync(
              juce::FileBrowserComponent::saveMode |
                  juce::FileBrowserComponent::canSelectFiles,
              [completion, payload](const juce::FileChooser &fc) {
                auto file = fc.getResult();
                if (file != juce::File{}) {
                  file.replaceWithText(payload);
                  completion(true);
                } else {
                  completion(false);
                }
              });
        } else {
          completion(false);
        }
      });

  // 3. ステートのファイル読み込み
  options = options.withNativeFunction(
      "loadPluginState",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        chooser = std::make_unique<juce::FileChooser>("Load State",
                                                      juce::File{}, "*.json");
        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                                 juce::FileBrowserComponent::canSelectFiles,
                             [this, completion](const juce::FileChooser &fc) {
                               auto file = fc.getResult();
                               if (file.existsAsFile()) {
                                 juce::String content = file.loadFileAsString();
                                 audioProcessor.setStateInformation(
                                     content.toRawUTF8(),
                                     content.getNumBytesAsUTF8());
                                 // Return UI lightweight state to remount View
                                 juce::String uiState =
                                     audioProcessor.getStateAsJsonString(false);
                                 completion(uiState);
                               } else {
                                 completion(juce::var::undefined());
                               }
                             });
      });

  // 4. 再生コントロール
  options = options.withNativeFunction(
      "togglePlayback",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (!audioProcessor.isHostPlaying.load()) {
          bool currentlyPlaying = audioProcessor.isStandalonePlaying.load();
          if (!currentlyPlaying && args.size() > 0) {
            double startTicks = static_cast<double>(args[0]);
            double newPpq = startTicks / 480.0;
            audioProcessor.currentPpqPosition.store(newPpq);
          }
          audioProcessor.isStandalonePlaying.store(!currentlyPlaying);
          completion("Toggled");
        } else {
          completion("Host is playing");
        }
      });

  // 5. ショートカットの登録
  options = options.withNativeFunction(
      "registerShortcuts",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0 && args[0].isArray()) {
          auto *shortcutsArray = args[0].getArray();
          const juce::ScopedLock sl(shortcutLock);
          registeredShortcuts.clear();
          if (shortcutsArray != nullptr) {
            for (auto &s : *shortcutsArray) {
              juce::String id = s.getProperty("id", "").toString();
              juce::String desc = s.getProperty("desc", "").toString();
              if (id.isNotEmpty() && desc.isNotEmpty()) {
                registeredShortcuts.push_back(
                    {juce::KeyPress::createFromDescription(desc), id});
              }
            }
          }
          completion("Registered");
        }
      });

  // 6. Drum Sampler Functions
  options = options.withNativeFunction(
      "loadDrumSample",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() >= 17) {
          audioProcessor.loadDrumSample(
              args[0], args[1].toString(), args[2].toString(),
              args[3].toString(), args[4], args[5], args[6], args[7], args[8],
              args[9], args[10], args[11], args[12], args[13], args[14],
              args[15], args[16], args[17]);
          completion("Loaded");
        } else {
          completion("Invalid args");
        }
      });

  options = options.withNativeFunction(
      "updatePadConfig",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() >= 15) {
          audioProcessor.updatePadConfig(args[0], args[1], args[2], args[3],
                                         args[4], args[5], args[6], args[7],
                                         args[8], args[9], args[10], args[11],
                                         args[12], args[13], args[14]);
          completion("Updated");
        } else {
          completion("Invalid args");
        }
      });

  // 8. Fetch Full State on Mount
  options = options.withNativeFunction(
      "requestFullState",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        // Exclude audio payloads to prevent UI mount crashing
        juce::String state = audioProcessor.getStateAsJsonString(false);
        completion(state);
      });

  options = options.withNativeFunction(
      "getPadAudioBase64",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0) {
          juce::String b64 = audioProcessor.getPadAudioBase64(args[0]);
          completion(b64);
        } else {
          completion("");
        }
      });

  // 9. Preview Audio
  options = options.withNativeFunction(
      "previewDrumPad",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() >= 2) {
          int padIndex = args[0];
          bool isNoteOn = args[1];
          int midiNote = 20 + padIndex;

          juce::MidiMessage msg;
          if (isNoteOn) {
            msg = juce::MidiMessage::noteOn(1, midiNote, 1.0f);
          } else {
            msg = juce::MidiMessage::noteOff(1, midiNote, 0.0f);
          }

          {
            std::lock_guard<std::mutex> lock(audioProcessor.uiMidiMutex);
            audioProcessor.uiMidiBuffer.addEvent(msg, 0);
          }

          completion("Preview Triggered");
        } else {
          completion("Invalid args");
        }
      });

  options = options.withNativeFunction(
      "openFileDialog",
      [this](const juce::Array<juce::var> &args,
             juce::WebBrowserComponent::NativeFunctionCompletion completion) {
        if (args.size() > 0) {
          int padIndex = static_cast<int>(args[0]);

          chooser = std::make_unique<juce::FileChooser>(
              "Select Audio File", juce::File{},
              "*.wav;*.aif;*.aiff;*.mp3;*.ogg");

          chooser->launchAsync(
              juce::FileBrowserComponent::openMode |
                  juce::FileBrowserComponent::canSelectFiles,
              [this, padIndex, completion](const juce::FileChooser &fc) {
                auto file = fc.getResult();
                juce::DynamicObject::Ptr resultObj = new juce::DynamicObject();

                // 1. ファイルが存在しない、またはキャンセルされた場合
                if (!file.existsAsFile()) {
                  resultObj->setProperty(
                      "error",
                      "ファイルが存在しないか、キャンセルされました。");
                  completion(juce::var(resultObj.get()));
                  return;
                }

                // 2. ファイルのデータをメモリに読み込めなかった場合
                // (Sandbox制限等)
                juce::MemoryBlock mb;
                if (!file.loadFileAsData(mb)) {
                  resultObj->setProperty("error",
                                         "ファイルの読み込みに失敗しました（Mac"
                                         "の権限不足の可能性があります）。");
                  completion(juce::var(resultObj.get()));
                  return;
                }

                juce::String fileName = file.getFileName();
                juce::String filePath = file.getFullPathName();

                // プロセッサにロード (Base64は使わないので第3引数を "" にする)
                audioProcessor.loadDrumSample(
                    padIndex, fileName, "", filePath, 0.0, -1.0, false, 0.0,
                    0.0, false, 0.0f, 0.0f, 1.0f, 0.1f, 1.0f, 1.0f, 1.0f, 1.0f);

                // 成功時はファイル名のみを返す
                resultObj->setProperty("sampleName", fileName);
                // 【削除】resultObj->setProperty("base64Data", base64);
                completion(juce::var(resultObj.get()));
              });
        } else {
          completion(juce::var::undefined());
        }
      });

  webComponentPtr = std::make_unique<juce::WebBrowserComponent>(options);
  addKeyListener(this);
  setWantsKeyboardFocus(true);

  setSize(800, 600);
  addAndMakeVisible(webComponentPtr.get());

#if JUCE_DEBUG
  // 開発時: Viteなどのローカル開発サーバーを使用
  webComponentPtr->goToURL("http://localhost:5173/");
#else
  // Release時: withResourceProviderで設定したローカルアセットを読み込む
  webComponentPtr->goToURL(
      juce::WebBrowserComponent::getResourceProviderRoot());
#endif
  startTimerHz(30);
}

WebPluginAudioProcessorEditor::~WebPluginAudioProcessorEditor() {
  removeKeyListener(this);
  stopTimer();
}

void WebPluginAudioProcessorEditor::timerCallback() {
  if (webComponentPtr == nullptr)
    return;

  bool isPlaying = audioProcessor.isHostPlaying.load() ||
                   audioProcessor.isStandalonePlaying.load();
  double ppq = audioProcessor.currentPpqPosition.load();
  double currentTicks = ppq * 480.0;

  if (isPlaying || currentTicks != lastReportedTicks) {
    lastReportedTicks = currentTicks;
    juce::String jsCode =
        "if (typeof window.updatePlayheadPosition === 'function') { "
        "  window.updatePlayheadPosition(" +
        juce::String(currentTicks) +
        ");"
        "}";
    webComponentPtr->evaluateJavascript(jsCode);
  }

  double bpm = audioProcessor.currentBpm.load();
  int num = audioProcessor.timeSigNumerator.load();
  int den = audioProcessor.timeSigDenominator.load();

  if (bpm != lastReportedBpm || num != lastReportedNum ||
      den != lastReportedDen) {
    lastReportedBpm = bpm;
    lastReportedNum = num;
    lastReportedDen = den;
    juce::String jsCode;
    jsCode << "if (typeof window.updateHostTempo === 'function') { "
           << "  window.updateHostTempo(" << bpm << ", " << num << ", " << den
           << ");"
           << "}";
    webComponentPtr->evaluateJavascript(jsCode);
  }
}

bool WebPluginAudioProcessorEditor::keyPressed(const juce::KeyPress &key,
                                               juce::Component *) {
  const juce::ScopedLock sl(shortcutLock);
  for (const auto &shortcut : registeredShortcuts) {
    if (shortcut.keyPress == key) {
      juce::String js;
      js << "if (typeof window.handleJuceShortcut === 'function') { "
         << "  window.handleJuceShortcut('" << shortcut.id << "'); "
         << "}";
      webComponentPtr->evaluateJavascript(js);
      return true;
    }
  }
  return false;
}

void WebPluginAudioProcessorEditor::paint(juce::Graphics &g) {
  g.fillAll(
      getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void WebPluginAudioProcessorEditor::resized() {
  webComponentPtr->setBounds(getLocalBounds());
  webComponentPtr->grabKeyboardFocus();
}