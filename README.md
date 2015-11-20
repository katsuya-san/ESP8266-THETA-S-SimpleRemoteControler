
動作動画：https://youtu.be/Eq2YSG_tlbY<BR>
[![](http://img.youtube.com/vi/Eq2YSG_tlbY/0.jpg)](https://www.youtube.com/watch?v=Eq2YSG_tlbY)
<BR>
スイッチサイエンス社製 ESP-WROOM-02開発ボード( https://www.switch-science.com/catalog/2500/ )を<BR>
RICOH THETA S のリモコンにするためのファームウェアソースコードです。<BR>
（が、本ソースコードをESP8266を利用した他のチップに適用することは容易と思われます）<BR>
<BR>
今回は、はんだ付けなし でも動作するよう作成しました。0番ピンと繋がったボタンがレリーズボタンになります。<BR>
電源はモバイルブースターが簡単です。 3.3v~5v 200mA の電源を接続することも可能です。<BR>
詳細は上記URLの回路図から理解してください。<BR>

ハンダ付けができる方は こちらのLED（ https://www.switch-science.com/catalog/2397/ ）も追加すると、<BR>
より便利なリモコンとなります。結線方法はソースコードのファイルヘッダを参照ください。<BR>
増設したLED1,LED2の意味合いは以下となります。<BR>
  LED1 ＝ THETA S BUSY 状態　（OFF:idle状態、ON:静止画保存待ち or 動画撮影中 ）<BR>
  LED2 ＝ wifi接続状態　（OFF:接続確立、ON:THETA S との接続動作）<BR>
  LED1とLED2を同時点灯 ＝ 後述のTHETA S登録モード中<BR>

このファームウェアの特徴としては、<BR>
  ・電源Onからの起動、起動してからのwifi接続確立(平均3~5秒程度)、<BR>
    ボタン操作に対するTHETA Sの反応、wifi接続断認識、いずれも速いです。<BR>
　　（静止画撮影の応答の速さを重視しています。動画はTHETA S本体の反応自体が遅いのでそれほど頑張ってません）<BR>
<BR>
  ・静止画撮影では、BUSY中でも1秒間隔程度で連続操作を受け付けます。<BR>
    ただし9連続程度が限界です。連写後は電源OFFなど長く待たされます。<BR>
    1枚撮影したらBUSYが解除されるまで（約8秒程度）待ったほうが安全です。<BR>
<BR>
  ・「初回起動」 or 「THETA Sとの接続動作中に上記ボタンを5秒程長押し」すると、<BR>
  　THETA S のSSIDを検索して内部のflashメモリに記憶するというモードになります。<BR>
  　THETA S の SSID, パスワードは出荷状態のみを探索対象としています。<BR>
  　（独自設定のSSID、パスワードで使用した場合、ソースコードを変更してください）<BR>
  　登録モードの時は、離れたTHETA S を検索対象としないようにしました。<BR>
  　できるだけTHETA S を近づけてください。(RSSI値が-45[db]より近いと登録できます)<BR>
  　THETA S の探索が終わると自動で、THETA Sとの接続動作に切り替わります。<BR>
<BR>
  ・THETA S が登録された状態では、電源ONすると自動で「THETA S との接続動作」となります。<BR>
<BR>
  ・THETAが接続されると、本体の動作モード（静止画・動画）に連動して、静止画撮影 or 動画撮影開始／停止<BR>
  　が行えるようになります。　本体操作で動画撮影開始してリモコンで停止や、その逆も可能です。<BR>
<BR>
  ・動画撮影中に wifi 接続を切り、再びwifi接続した後などでも正常動作が可能です。<BR>
<BR>
  ・v01.01からインターバル撮影に対応しました。<BR>
  　Wi-Fi断状態でレリーズボタンを３回押してから Wi-Fi接続するとインターバル撮影モードとなりLED1が点滅。<BR>
  　1回のインターバル撮影が終わると、通常モードとなります。<BR>
  　インターバル撮影中にWi-Fiが切断されて再接続してもインターバル撮影を終了できます。<BR>
  　詳細は以下動画（ https://youtu.be/OOI3o3XWs1M ）を参照してください。<BR>
[![](http://img.youtube.com/vi/OOI3o3XWs1M&feature=youtu.be/0.jpg)](https://www.youtube.com/watch?v=OOI3o3XWs1M&feature=youtu.be)
<BR>
<BR>
<BR>
開発環境は、arduino IDE です。JSONの解釈にフリーのライブラリを使用しています。<BR>
以下２手順で開発環境をセットアップしてください。それほど難しくありません。<BR>
<BR>
  (1) ESP8266用にadruino IED を 10分でIDEのセットアップ<BR>
  　　http://qiita.com/azusa9/items/264165005aefaa3e8d7d<BR>
<BR>
  (2) JSON パーサーのライブラリを追加する。<BR>
  　　ライブラリは以下からzipをダウンロード<BR>
  　　https://github.com/bblanchon/ArduinoJson<BR>
  　　ライブラリの追加方法は以下<BR>
  　　http://make.bcde.jp/category/39/<BR>
<BR>
あとは、<BR>
  ・パソコンにESP-WROOM-02開発ボードを繋ぐ（ドライバがインストールされCOMポートが認識されるまで待つ）<BR>
  ・arduino IDE で本ファイル(ESP_ThetaRemote.ino)を開いて（ダブルクリック）<BR>
  　「スケッチ」→「マイコンボードに書き込む」をクリックしてしばしまつ。<BR>
で、出来上がります。<BR>
<BR>
その他の細々したことは、ご自身でなんとかしてください。<BR>
<BR>
