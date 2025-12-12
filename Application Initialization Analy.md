# ESP32 Application Initialization Analysis

This document details the initialization flow of the application, starting from `app_main` and covering system, board, audio, and network components.

## 1. System Entry Point (`main.cc`)
The application starts in `app_main`.

1.  **NVS Flash Initialization**:
    *   **Function**: `nvs_flash_init()`
    *   **Condition**: Always.
    *   **Fallback**: If `ESP_ERR_NVS_NO_FREE_PAGES` or `ESP_ERR_NVS_NEW_VERSION_FOUND`, it erases NVS (`nvs_flash_erase()`) and re-initializes.
    *   **Purpose**: Essential for WiFi credentials and persistent settings (`Settings` class).

2.  **Application Startup**:
    *   **Function**: `Application::GetInstance().Initialize()` then `Application::Run()`.
    *   **Location**: `main.cc` -> `application.cc`.

## 2. Application Initialization (`application.cc`)
The `Application::Initialize()` method orchestrates the setup.

### 2.1. Board & Device State
*   **Board Introspection**: `Board::GetInstance()` is called.
    *   **Location**: `boards/common/board.h` (Static method) -> `create_board()` (Implemented by specific board macro, e.g., `boards/.../board.cc`).
    *   **Logic**: Creates the specific board singleton (e.g., `WifiBoard`, `EspBoxBoard`).
    *   **Board Constructor**:
        *   Initializes `Settings` storage for board data.
        *   Generates/Loads persistent UUID (`boards/common/board.cc`).
*   **State Machine**: `SetDeviceState(kDeviceStateStarting)` sets initial state.

### 2.2. Display & UI
*   **Get Display**: `board.GetDisplay()`.
*   **Show Info**: Sets chat message to User Agent (System Info) to show boot status.

### 2.3. Audio Subsystem Initialization
This is a major component handled by `AudioService::Initialize` (`audio/audio_service.cc`).
*   **Step 1: Codec Retrieval**: `board.GetAudioCodec()` returns the specific I2C/I2S codec driver.
*   **Step 2: Service Init**: `audio_service_.Initialize(codec)`.
    *   **Codec Start**: Calls `codec_->Start()` to enable hardware I2S/I2C.
    *   **Opus Codecs**: Allocates `OpusDecoderWrapper` and `OpusEncoderWrapper` (16kHz, 60ms frame).
    *   **Resamplers**: Configures `input_resampler_` and `reference_resampler_` if hardware rate != 16kHz.
    *   **Audio Processor (AFE)**:
        *   Condition: `CONFIG_USE_AUDIO_PROCESSOR` (defined in Kconfig).
        *   Action: Instantiates `AfeAudioProcessor` (Espressif AFE SR - AEC, BSS, NS) or `NoAudioProcessor`.
        *   Callbacks: Sets up `OnOutput` (to encode queue) and `OnVadStateChange` (Voice Activity Detection).
    *   **Power Management**: Creates `audio_power_timer` to manage codec power saving.

### 2.4. Audio Service Startup
*   **Function**: `audio_service_.Start()`
*   **Tasks Created**:
    1.  **Audio Input Task** (`audio_input`): Reads from Codec -> Feeds Wake Word / Audio Processor.
    2.  **Audio Output Task** (`audio_output`): Reads from Playback Queue -> Writes to Codec.
    3.  **Opus Codec Task** (`opus_codec`): Handles Encoding (PCM->Opus) and Decoding (Opus->PCM).

### 2.5. Event Handling Setup (`application.cc`)
*   **Audio Callbacks**: Registers generic callbacks for VAD, Wake Word, and Send Queue availability to trigger `xEventGroupSetBits`.
*   **State Listeners**: Triggers `MAIN_EVENT_STATE_CHANGED` on state transitions.
*   **System Timer**: Starts `1s` periodic timer for clock/status updates.

### 2.6. MCP (Model Context Protocol)
*   **Function**: `McpServer::GetInstance().AddCommonTools()`.
*   **Purpose**: Registers available tools for the AI agent.

### 2.7. Network Initialization
Network setup is delegated to the `Board` implementation.
*   **Function**: `board.StartNetwork()` (Virtual function).
*   **Implementation**: Typically `WifiBoard::StartNetwork()` (`boards/common/wifi_board.cc`).

#### WiFi Board Network Flow (`boards/common/wifi_board.cc`):
1.  **WiFi Manager Init**: `WifiManager::GetInstance().Initialize(config)`.
    *   Configures SSID prefix ("Xiaozhi").
2.  **Event Callbacks**: Binds internal WiFi events (Scanning, Connected, Disconnected) to the `Application`'s `NetworkEventCallback`.
    *   **Application Callback**: `application.cc` lines 102-157.
    *   Handles UI notifications (Show "Scanning", "Connected to...").
    *   Sets Event Bits: `MAIN_EVENT_NETWORK_CONNECTED` / `MAIN_EVENT_NETWORK_DISCONNECTED`.
3.  **Connection Logic** (`TryWifiConnect`):
    *   **Check Saved SSIDs**: Checks `SsidManager`.
    *   **If SSIDs exist**: Calls `WifiManager::GetInstance().StartStation()` to connect.
    *   **If NO SSIDs**: Enters **Config Mode** (`StartWifiConfigMode()`).
        *   Sets State: `kDeviceStateWifiConfiguring`.
        *   Starts AP Mode (`StartConfigAp`).
        *   Starts Acoustic Provisioning (if enabled).

## 3. Main Loop (`Application::Run`)
After `Initialize()` returns, `app_main` calls `Application::Run()`.
*   **Mechanism**: Infinite loop waiting on `xEventGroupWaitBits`.
*   **Event Sources**:
    *   Network (Connected/Disconnected).
    *   Audio (Wake Word, VAD, Send Ready).
    *   System (Clock, Errors).
*   **Key Reaction**:
    *   **On Network Connected**: Calls `HandleNetworkConnectedEvent`.
        *   Checks State: If `Starting`, transitions to `kDeviceStateActivating`.
        *   **Activation Task**: Starts background task to check OTA, Assets, and Protocol.
            *   **Protocol Init**: `InitializeProtocol()` creates `WebsocketProtocol` or `MqttProtocol` based on config.
            *   **Protocol Start**: Connects to server.
