# MCHOSE V9 PRO — Özellik ve Endpoint Analizi

Tarih: 2026-10-30. Kaynaklar:

- `examples/v9pro-control` (C#/WinForms, Windows) — APO registry eşlemesi + HID komutları
- `examples/MCHOSE_v9_PRO_Controller` (PyQt6, Linux) — hidapi + wpctl
- `examples/Be-Quiet-MCHOSE` — firmware formatı (AOTA), XOR pad, OTA flash protokolü
- MCHOSE HUB kurulumu (`~/.wine/drive_c/Program Files/MCHOSE HUB/`): `resources/app.asar`
  (Electron, main süreç V8 bytecode `.jsc`, preload düz JS), `libs/osConfLib.dll`, `libs/DLL/MicListen.dll`
- Hub arayüzü uzaktan yükleniyor: `https://www.mchose.com.cn/` ve cihaz sayfaları
  `https://cdn.mchose.com.cn/customPage/prod/` (Vue/Vite; WebHID kodu burada). İndirilen chunk'lar:
  `headsetPage-*.js`, `getTargetHook-*.js`, `hidDeviceOptions-*.js`, `eq-*.js`, `crc32-*.js`, `webHIDBase-*.js`
- Linux'ta canlı cihaz üzerinde salt-okuma probe'ları (`/dev/hidraw4`)

Hub'ın Wine altında çalışmamasının sebebi: arayüzün tamamı CDN'den geliyor, ses efektleri
Windows'un C-Media APO'suna (`CmApoIce.dll`) bağlı, HID ise Chromium WebHID üzerinden. Wine bunların
hiçbirini anlamlı şekilde sağlamıyor. Yani Hub'ı çalıştırmak yerine protokolü yeniden uygulamak doğru yol.

---

## 1. Cihaz kimliği (Linux'ta doğrulandı)

| Alan | Değer |
|---|---|
| USB | `291d:385d` "C-Media Electronics Inc MCHOSE V9 PRO" (dongle), full speed |
| Hub productKey | `MCHOSE V9 PRO PID14429` (Hub, PID 14429 için isme `PID14429` ekliyor) |
| Driver dizini (Win) | `libs/S9_PRO_14429` (S9 PRO / G9 PRO ile aynı donanım ailesi) |
| Çip | C-Media CM2025 (muhtemelen Actions tabanlı), headset ve dongle aynı çip |
| ALSA | `card 2 [PRO] USB-Audio`, snd-usb-audio (standart UAC1) |
| PipeWire sink | `alsa_output.usb-C-Media_Electronics_Inc_MCHOSE_V9_PRO_0123456789AB-01.analog-stereo` (s24le 2ch 48k) |
| PipeWire source | `alsa_input.usb-C-Media_Electronics_Inc_MCHOSE_V9_PRO_0123456789AB-01.mono-fallback` (1ch) |
| udev | `/etc/udev/rules.d/99-mchose.rules` mevcut, hidraw `0666` |

### HID report descriptor (hidraw4)

| Report ID | Usage page | Yön / boyut | Anlam |
|---|---|---|---|
| `0xED` | `0xFF21` vendor | in 1, out 1, feature 15 | Bilinmiyor (Hub kullanmıyor gibi) |
| `0xAA` | `0xFF22` vendor | feature 63 | Firmware sürümü sorgusu |
| `0x06` | `0x0C` consumer | in 8 bit | Vol+/Vol-/Mute/Play/Next/Prev/Stop — kernel input olarak işliyor |
| `0x07` | `0x0B` telephony | in (Hook Switch, Phone Mute, …), out LED (`0x08` page: Mute/Off-hook/Ring LED…) | Telefon/konferans kontrolü — mikrofon mute LED senkronu için aday |
| `0x55` | `0xFF90` vendor | in 63, out 63 | **Ana kontrol kanalı** (pil, ses modu, OTA giriş) |
| `0x41` | `0xFF82` vendor | in 63, out 63 | OTA veri akışı |

Linux'ta yazma: `write(fd, [reportId, payload...64])`. Feature: `HIDIOCSFEATURE/HIDIOCGFEATURE` ioctl.
WebHID'deki `sendReport(85, [cmd,...])` ≡ hidraw `write([0x55, cmd, ...])`.

---

## 2. Cihaz tarafı özellikler (HID — Linux'ta doğrudan çalışır)

### 2.1 Pil seviyesi ve şarj durumu — DOĞRULANDI

```
TX 55 65 01            -> RX 55 65 <percent 0..100> <state>
state: 2=deşarj, 3=şarj oluyor, 4=dolu, 26(0x1A)=uykuda
```

- Linux probe: `55 65 64 02` → %100, deşarj.
- Cihaz durumu değişince **kendiliğinden** aynı formatta input report gönderiyor
  (Hub `hidDeviceOptions` → `inputreport` dinleyicisi). Polling + dinleyici birlikte kullanılmalı.
- Eski firmware `0x65`'i yok sayıyor. Hub, dongle firmware'i belli sürümün altındaysa pil okumuyor.
- Tek seferde tek istek: eşzamanlı istekler cevap düşürüyor (C# örneğindeki `_gate`). İlk istek
  boşta kaldıktan sonra bazen düşüyor → 1 tekrar + 250 ms bekleme.

### 2.2 Ses modu (cihaz içi DSP preset) — okuma + yazma DOĞRULANDI (2026-10-30)

```
GET  TX 55 11                 -> RX 55 11 <mode>
SET  TX 55 21 01 <mode>       -> cevap yok (fire-and-forget)
mode: 0=Game 1, 1=Game 2, 2=Music   (0..2 dışı cihazda clamp)
NOTIFY: cihaz butonla mod değişince RX 55 11 <mode> gönderiyor
```

- Linux probe: `55 11 02` → Music.
- 2026-10-30: uygulamadan `55 21 01 <mode>` ile geçiş gerçek cihazda çalışıyor.
- Bluetooth'ta HID kanal yok → sadece 2.4G'de.
- Hub'da `setVoiceBroadcastState` (sesli bildirim aç/kapa) V9 PRO hook'unda **boş fonksiyon** —
  yani V9 PRO için resmi bir "prompt kapat" HID komutu yok. (G9 V2 ve K20 ailesinde `21 02 <0|1>` var,
  V9 PRO'da denenmedi — bkz. §2.5.)

### 2.3 Firmware sürümü — DOĞRULANDI (2026-10-30)

```
SET_FEATURE  AA 01 <target> 00...(63)   target: 1=dongle, 0=headset (Hub sRe())
(300/500/900 ms bekle)
GET_FEATURE  AA -> AA 01 b2 b3 b4 b5 ...
Hub: version = `${b2}${b3}${b4}${b5}` (ondalık birleştirme)
```

Linux ham cevaplar:

```
target=1: aa 01 00 00 01 02 ff 25 07 08 18 08 36 34 46 30
target=0: aa 01 01 00 01 05 ff 25 08 07 09 17 36 34 46 30
```

Python örneği farklı ofset kullanıyor (`d[8]d[9]` dongle, `d[10]d[11]` headset). Hub'ın bildiği son
sürümler: dongle `0008`, headset `0029`. Hangi yorumun doğru olduğu bir OTA sonrası karşılaştırmayla
netleşir. UI şimdilik ham hex + Hub yorumunu gösterir.

Hub yorumu doğru: dongle `0012`, headset `1015`. OTA manifest (`otaConfig_prod.json` →
`"MCHOSE V9 PRO PID14429"`) headset için `type 1` (İngilizce bildirimler) son sürüm `1015.bin`,
`type 0` (Çince) `0036.bin`; dongle son sürüm `0013_b0ab57.bin`. Yani bu kulaklık güncel İngilizce
firmware'de, dongle bir sürüm geride. Headset sürümünün ilk hanesi dil tipini taşıyor.

**Önemli:** Bir SET_FEATURE cevabı beklenirken ikinci SET_FEATURE gönderilirse dongle ikisini de
EPIPE ile durduruyor. Firmware okuması tek seferde bir tane olmalı (uygulamada `m_firmwareReading`).
0x55 trafiğinin araya girmesi sorun değil (test edildi).

### 2.4 OTA / firmware flash (Be-Quiet-MCHOSE)

```
Enter update : out 55, payload[0]=0x55, payload[1]=1 (headset) | 0 (dongle)
Ready        : in 55 [02 01 00]
Stream       : tüm .bin, 63 byte parçalar, out report 0x41
Flow control : in 55 [60 FF]=dur, [60 00]=devam
Result       : in 55 [01 01 00]=başarılı, [01 00 00]=başarısız
```

- Uygulamada: Lab → "Voice prompt silencer". C++ yamalayıcı (`src/core/FirmwareImage.*`) gerçek `1015.bin`
  üzerinde Be-Quiet `build_firmware.py 8 9` çıktısıyla **byte-byte aynı** (opt-in test `realImageMatchesReference`).
  Flasher (`FirmwareFlasher`) ayrı thread + ayrı hidraw handle; flash sırasında HeadsetController askıda.
  **Flash henüz cihazda denenmedi.**
- Firmware: AOTA konteyner, partition'lar 32 byte sabit XOR pad ile "şifreli",
  imza kontrolü yok. `tone.bin` = 18 sesli bildirim klibi; İngilizce FW'de **klip 8 = mic on, 9 = mic off**.
- OTA config: `https://cdn.mchose.com.cn/OTA/PC/prod/otaFile/otaConfig_prod.json`,
  dosya: `.../otaFile/V9Pro/headset/<fileName>.bin`.
- **Kullanıcının asıl derdi** (mikrofon aç/kapa yapınca bildirim sesi çalıp dinlenen sesi ~1.5 sn
  susturması) firmware davranışı. Çözüm yolları:
  1. Firmware yaması (klip 8/9 sessizleştirme) + OTA flash — kanıtlanmış, geri alınabilir. Susturma
     (~1.5 sn ducking) kalır ama konuşma yerine sessizlik olur.
  2. Prompt volume komutu (§2.5) V9 PRO'da çalışırsa ses seviyesini 1'e çekmek.
  3. Ducking'in kendisi firmware `app.bin` içinde; kaldırmak için kod yaması gerekir (araştırma konusu).

### 2.5 Legacy `0x22` alt komutları — V9 TURBO ailesi resmi, **V9 PRO'da TEST EDİLMEDİ**

Uygulamada: Lab sekmesi → "Experimental headset settings". Önce Read; okuma cevap verirse Write açılır.
Cevaplar ham akıştan alt-komut eşleşmesiyle yakalanır (`55 22 <sub> <value>`).

Hub `hidDeviceOptions` (reportId 85, `cmd=34 (0x22)`):

| TX | Anlam | Not |
|---|---|---|
| `55 22 00 <v>` | Otomatik kapanma ayarla | Hub telemetrisi: v=1→15dk, 4→30dk, 0→45dk(?) |
| `55 22 01` | **FACTORY RESET** | ASLA rastgele gönderme |
| `55 22 02` | Otomatik kapanma oku → `rx[2]` | |
| `55 22 03 <v>` | Bildirim (prompt) ses seviyesi ayarla | Aralık muhtemelen 1..16 |
| `55 22 04` | Bildirim ses seviyesi oku → `rx[2]` | |

Hub bunları sadece `V9 TURBO`, `V9 TURBO+`, `V9 TURBO+ HEADSET` için açıyor. Aynı çip ailesi olduğu
için V9 PRO'da çalışma ihtimali var. **Sadece kullanıcı onayıyla ve önce okuma komutlarıyla (02, 04)**
denenecek. Uygulamada "Deneysel" bölümünde, varsayılan kapalı.

### 2.6 Yeni çerçeveli protokol (V9 PRO 2 / R9, `isNewLayout`) — büyük ihtimalle V9 PRO'da YOK

Report 0x55 içinde çerçeve: `[AA, ver=1, flags(check), len=payload+1, cmdType(2), cmd, payload..., xorChk]`.
cmdType: Request `00 01`, Set `00 02`, Notify `00 04`. Komutlar: `SET_EQ_MODE 1`, `GET_EQ_STATE 4`
(cihaz içi PEQ noktaları: fc/gain/q/type/enable), `GET_EQ_MODE_DESC 5`, `GET_BATTERY_SLEEP 11`,
`GET_CURRENT_EQ_MODE 12`, `GET_PROMPT_VOLUME 13`, `GET_AUTO_SHUTDOWN 14`, `SET_PEQ_ALL 16`,
`SET_PEQ_ALL_GAIN 18`, `SET_EQ_MODE_NAME 24`, `RESTORE_DEFAULT 25`, `SET_AUTO_SHUTDOWN 26`,
`FACTORY_RESET 27`, `SET_PROMPT_VOLUME 33`. Referans için kayıtlı; V9 PRO için uygulanmayacak.

### 2.7 Telephony / mikrofon butonu — trafik YOK, DOĞRULANDI (2026-10-30)

Uygulamada: Lab → "HID monitor". Polling (pil/mod) gizlenir, geri kalan her rapor vurgulanır.

Descriptor'da telephony (Phone Mute 0x2F) input ve mute LED output var. C# yazarı mikrofon butonunun
**hiç USB trafiği üretmediğini** söylüyor. 2026-10-30 Linux'ta doğrulandı: dongle'ın tek hidraw
düğümü (`hidraw4`, 0xED/0xAA/0x06/0x07/0x55 hepsi aynı düğümde) mic on/off sırasında hiçbir rapor
üretmiyor. Mute kulaklık içinde yapılıyor; host'tan görünmüyor → PipeWire mute ile buton senkronu
HID üzerinden mümkün değil.

---

## 3. Host tarafı özellikler (Windows'ta APO — Linux'ta yeniden uygulanmalı)

Hub'daki ses efektleri **cihazda değil**, Windows'un C-Media APO'sunda (`CmApoIce.dll`) çalışıyor;
ayarlar `MMDevices\...\FxProperties` altında. Linux'ta APO yok → **PipeWire filter-chain** ile
yazılımda uygulanacak. Hub preload API isimleri (`electronAPI.*`) = tam özellik listesi:

| Hub özelliği (APO adı) | Windows anlamı | Linux karşılığı | Öncelik |
|---|---|---|---|
| `setAudioVolume/Mute` | Endpoint ses/mute | PipeWire node volume (wpctl / libpipewire) | P0 |
| Mic volume/mute | Capture endpoint | PipeWire source | P0 |
| `setEnable_EQ_GFX`, `setEQ_Slider` 1..10 | 10 band EQ, ±20 dB, 20/60/100/250/500/1k/2k/4k/8k/20k Hz | filter-chain `bq_lowshelf`/`bq_peaking`/`bq_highshelf` sanal sink | P1 |
| EQ profilleri (`{"name","data":[...]}`) | Hub JSON export | Aynı format, `~/.config/v9pro-control/profiles/` | P1 |
| `AUDIOBRILLIANT` (enable, level 0..10) | Tiz parlatma | high-shelf ~6 kHz, kazanç level'a bağlı | P2 |
| `VOICECLARITY` (enable, level, noise supp 0..20) | Vokal netlik | 1–4 kHz peaking boost | P2 |
| `VIRTUALBASS` (enable, level 0..10, cutoff 50..400 Hz) | Dinamik bas | low-shelf @cutoff (+ ileride harmonik) | P2 |
| `ADAPTIVEVOLUME` (enable, level, mode) | Akıllı ses (AGC) | LADSPA/LV2 kompresör (ör. `sc4m`, LSP) — opsiyonel bağımlılık | P3 |
| `Enable_Channel_COPY_LFX` | Kanal kopyalama | mono downmix (filter-chain mixer) | P3 |
| `AI_NR_Enable`, `NR_Enable` | Mikrofon AI gürültü azaltma | `libpipewire-module-echo-cancel` (webrtc NS) veya rnnoise LADSPA | P2 |
| `Enable_MAGICVOICE`, `MagicVoice_Selection` | Ses değiştirici | pitch shift LADSPA (rubberband) — opsiyonel | P4 |
| `XearSurround` (enable, mode, room) | Sanal surround | filter-chain `spatializer`/convolver + HRIR — opsiyonel | P3 |
| `MicListen` (`MicListen.dll`) | Mikrofonu dinle (sidetone) | ALSA `PCM Playback Volume',1` (mono, 0..151) **olası donanım sidetone** — doğrulanacak; yoksa `pw-loopback` | P1 |
| `playTestSound` | Test sesi | `pw-play` / QtMultimedia | P3 |
| Otomatik başlatma, tray | | XDG autostart + `QSystemTrayIcon` | P2 |

Kapsam dışı (Hub'da var, V9 PRO ile alakasız): KuGou/QQ/NetEase şarkı sözü, oyun/uygulama tarama,
duvar kağıdı, klavye/fare/hoparlör sayfaları, THX (V9 PRO 2), WeChat login, telemetri.

ALSA mixer (card 2) kontrolleri:

```
'PCM Playback Switch'           bool
'PCM Playback Volume'           2ch 0..151 (-28.37..-0.06 dB)   ana çıkış
'PCM Playback Volume',index=1   1ch 0..151                      SIDETONE ADAYI (değer 44)
'Mic Capture Switch'            bool
'Mic Capture Volume'            1ch 0..151
```

---

### 3.1 Linux karşılıkları — uygulandı (2026-10-30)

Hepsi ayrı `pipewire -c <conf>` alt süreçleri (`$XDG_RUNTIME_DIR/v9pro-control/*.conf`), canlı parametre `pw-cli set-param <node> Props`.
Gerçek PipeWire 1.6'da yüklendiği ve varsayılanların kapanışta geri döndüğü doğrulandı.

| Özellik | Düğüm | Not |
|---|---|---|
| EQ + bas/parlaklık/vokal + preamp | `v9pro_equalizer` (Audio/Sink) → kulaklık sink | builtin `bq_*` + `mixer`; açıkken default sink |
| Gürültü engelleme | `v9pro_mic_denoised` (Audio/Source) | WebRTC (echo-cancel modülü) veya RNNoise LADSPA (kurulu değil) |
| Mikrofon monitörü (yazılım) | `v9pro_mic_monitor` | mic → kulaklık sink, `mixer` kazancı, 128/48000 latency |
| Mikrofon monitörü (donanım) | ALSA `'PCM',1` | Doğrulanmadı; monitör kapalıyken uygulama bunu 0'a çeker |

EQ bant eşlemesi: Hub etiketleri 20/60/100/250/500/1k/2k/4k/8k/20k; uç bantlar raf filtre (32 Hz low-shelf,
14 kHz high-shelf), diğerleri peaking. Bas = low-shelf @cutoff (level×1.2 dB), parlaklık = high-shelf 6 kHz
(level×0.8 dB), vokal = peaking 2.5 kHz (level×0.6 dB). Hub'ın DSP'siyle birebir değil, yaklaşım.

## 4. Linux uygulama mimarisi (karar)

- **C++20 + Qt 6.8+ (sistemde 6.12)**, Qt Quick/QML arayüz, Controls.Basic üzerine özel tema.
- `v9core` statik kütüphane (QML'den bağımsız, test edilebilir):
  - `HidRawDevice` — sysfs ile `291d:385d` + report descriptor'da `0x55` olan hidraw'ı bulur,
    `QSocketNotifier` ile async okuma, feature ioctl. hidapi bağımlılığı yok.
  - `V9Protocol` — saf fonksiyonlar: istek kurma, cevap çözme (unit test'li).
  - `HeadsetController` — istek kuyruğu (tek seferde tek exchange), polling, bildirim işleme.
  - `AudioMixer` — PipeWire (ilk sürüm `wpctl`/`pactl` QProcess; sonra libpipewire).
  - `EqualizerEngine` — filter-chain config üretimi + canlı parametre (P1).
- `udev` kuralı (`packaging/99-mchose-v9pro.rules`, `uaccess` tag'i ile) — root gerekmez.
- Tüm deneysel/riskli komutlar (0x22, OTA) ayrı, onaylı, varsayılan kapalı.
