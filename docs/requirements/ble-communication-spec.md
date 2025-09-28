# **BLE通信仕様**

## **1\. 概要**

本仕様書は、ESP32-C3植物監視システムにおけるBluetooth Low Energy (BLE) 通信プロトコルを定義します。システムはGATT (Generic Attribute Profile) サーバーとして動作し、センサーデータの提供、コマンドの受信、各種設定の管理を行います。

## **2\. GATTサービス**

### **2.1. サービスUUID**

カスタムサービスを1つ定義し、植物監視に関連するすべてのキャラクタリスティックを含みます。

| 項目 | UUID |
| :---- | :---- |
| **Soil Monitor Service** | 59462f12-9543-9999-12c8-58b459a2712d |

### **2.2. キャラクタリスティック**

#### **2.2.1. Sensor Data**

現在のセンサー測定値をクライアントに提供します。

| 項目 | 説明 |
| :---- | :---- |
| **UUID** | 6a3b2c1d-4e5f-6a7b-8c9d-e0f123456789 |
| **プロパティ** | Read, Notify |
| **データ形式** | soil\_ble\_data\_t 構造体 |

#### **2.2.2. Data Status**

デバイス内に保存されている履歴データの状態を提供します。

| 項目 | 説明 |
| :---- | :---- |
| **UUID** | 6a3b2c1d-4e5f-6a7b-8c9d-e0f123456790 |
| **プロパティ** | Read, Write |
| **データ形式** | ble\_data\_status\_t 構造体 |

#### **2.2.3. Command**

クライアントからデバイスへのコマンドを受信します。

| 項目 | 説明 |
| :---- | :---- |
| **UUID** | 6a3b2c1d-4e5f-6a7b-8c9d-e0f123456791 |
| **プロパティ** | Write, Write No Response |
| **データ形式** | ble\_command\_packet\_t 構造体 |

#### **2.2.4. Response**

デバイスからクライアントへのコマンド応答を送信します。

| 項目 | 説明 |
| :---- | :---- |
| **UUID** | 6a3b2c1d-4e5f-6a7b-8c9d-e0f123456792 |
| **プロパティ** | Read, Notify |
| **データ形式** | ble\_response\_packet\_t 構造体 |

#### **2.2.5. Data Transfer**

履歴データや設定ファイルなど、サイズの大きなデータを転送します。

| 項目 | 説明 |
| :---- | :---- |
| **UUID** | 6a3b2c1d-4e5f-6a7b-8c9d-e0f123456793 |
| **プロパティ** | Read, Write, Notify |
| **データ形式** | 可変長データ |

## **3\. コマンド・レスポンスシステム**

### **3.1. パケット構造**

#### **3.1.1. コマンドパケット**

クライアントからデバイスへ送信されるコマンドのフォーマットです。

typedef struct \_\_attribute\_\_((packed)) {  
    uint8\_t command\_id;     // コマンド識別子  
    uint8\_t sequence\_num;   // シーケンス番号  
    uint16\_t data\_length;   // データ長  
    uint8\_t data\[\];         // コマンドデータ  
} ble\_command\_packet\_t;

#### **3.1.2. レスポンスパケット**

デバイスからクライアントへ返信される応答のフォーマットです。

typedef struct \_\_attribute\_\_((packed)) {  
    uint8\_t response\_id;    // レスポンス識別子  
    uint8\_t status\_code;    // ステータスコード  
    uint8\_t sequence\_num;   // 対応するシーケンス番号  
    uint16\_t data\_length;   // レスポンスデータ長  
    uint8\_t data\[\];         // レスポンスデータ  
} ble\_response\_packet\_t;

### **3.2. コマンドID**

command\_id として使用される値の一覧です。

| ID | コマンド名 | 説明 |
| :---- | :---- | :---- |
| 0x01 | **CMD\_GET\_SENSOR\_DATA** | 最新のセンサーデータを取得します。 |
| 0x02 | **CMD\_GET\_SYSTEM\_STATUS** | メモリ使用量や稼働時間などのシステム状態を取得します。 |
| 0x03 | **CMD\_SET\_PLANT\_PROFILE** | 植物のプロファイル（閾値など）を設定します。 |
| 0x04 | **CMD\_GET\_HISTORY\_DATA** | 履歴データを取得します。 |
| 0x05 | **CMD\_SYSTEM\_RESET** | デバイスを再起動します。 |
| 0x06 | **CMD\_GET\_DEVICE\_INFO** | デバイス名やファームウェアバージョンなどの情報を取得します。 |
| 0x07 | **CMD\_SET\_TIME** | デバイスの時刻を設定します。 |
| 0x08 | **CMD\_GET\_CONFIG** | 現在の設定を取得します。 |
| 0x09 | **CMD\_SET\_CONFIG** | 新しい設定を書き込みます。 |
| 0x0A | **CMD\_GET\_TIME\_DATA** | 指定した日時のセンサーデータを取得します。 |
| 0x0B | **CMD\_GET\_SWITCH\_STATUS** | 本体スイッチの状態を取得します。 |

### **3.3. レスポンスステータスコード**

status\_code として使用される値の一覧です。

| コード | ステータス名 | 説明 |
| :---- | :---- | :---- |
| 0x00 | **RESP\_STATUS\_SUCCESS** | コマンドは成功しました。 |
| 0x01 | **RESP\_STATUS\_ERROR** | 不明なエラーが発生しました。 |
| 0x02 | **RESP\_STATUS\_INVALID\_COMMAND** | コマンドIDが無効です。 |
| 0x03 | **RESP\_STATUS\_INVALID\_PARAMETER** | コマンドのパラメータが無効です。 |
| 0x04 | **RESP\_STATUS\_BUSY** | デバイスは他の処理でビジー状態です。 |
| 0x05 | **RESP\_STATUS\_NOT\_SUPPORTED** | このコマンドはサポートされていません。 |

## **4\. データ構造**

通信で使用される主要なデータ構造です。

### **4.1. soil\_ble\_data\_t**

センサーデータ通知用。

typedef struct {  
    tm\_data\_t datetime;  
    float lux;  
    float temperature;  
    float humidity;  
    float soil\_moisture; // in mV  
} soil\_ble\_data\_t;

### **4.2. time\_data\_request\_t**

CMD\_GET\_TIME\_DATAコマンドのデータ部。

typedef struct \_\_attribute\_\_((packed)) {  
    struct tm requested\_time; // 要求する時間  
} time\_data\_request\_t;

### **4.3. time\_data\_response\_t**

CMD\_GET\_TIME\_DATAコマンドの応答データ部。

typedef struct \_\_attribute\_\_((packed)) {  
    struct tm actual\_time;    // 実際に見つかったデータの時間  
    float temperature;        // 気温  
    float humidity;           // 湿度  
    float lux;                // 照度  
    float soil\_moisture;      // 土壌水分  
} time\_data\_response\_t;

### **4.4. device\_info\_t**

CMD\_GET\_DEVICE\_INFOコマンドの応答データ部。

typedef struct \_\_attribute\_\_((packed)) {  
    char device\_name\[32\];  
    char firmware\_version\[16\];  
    char hardware\_version\[16\];  
    uint32\_t uptime\_seconds;  
    uint32\_t total\_sensor\_readings;  
} device\_info\_t;

## **5\. 通信フローの例**

### **5.1. 最新センサーデータの取得**

1. **クライアント** \-\> **デバイス**: **Command**キャラクタリスティックにCMD\_GET\_SENSOR\_DATAコマンドを書き込みます。  
2. **デバイス**: センサーデータを読み取ります。  
3. **デバイス** \-\> **クライアント**: **Response**キャラクタリスティックにRESP\_STATUS\_SUCCESSとsoil\_data\_t構造体を含む応答を送信（Notify）します。

### **5.2. 指定時間データの取得**

1. **クライアント** \-\> **デバイス**: **Command**キャラクタリスティックにCMD\_GET\_TIME\_DATAコマンドとtime\_data\_request\_tを書き込みます。  
2. **デバイス**: data\_buffer内を検索し、指定された時刻（分単位）に一致するデータを探します。  
3. **デバイス** \-\> **クライアント**:  
   * **成功時**: **Response**キャラクタリスティックにRESP\_STATUS\_SUCCESSとtime\_data\_response\_tを含む応答を送信（Notify）します。  
   * **失敗時**: **Response**キャラクタリスティックにRESP\_STATUS\_ERRORを含む応答を送信（Notify）します。