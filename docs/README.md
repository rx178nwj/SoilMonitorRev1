### 2.2 docs/README.md (ドキュメント目次)
```markdown
# Documentation

ESP32-C3 Plant Monitor システムの包括的なドキュメントです。

## 📚 ドキュメント構成

### 🎯 [要件定義](requirements/)
システムの機能要件・非機能要件を定義

- [機能要件](requirements/functional-requirements.md)
- [非機能要件](requirements/non-functional-requirements.md)
- [BLE通信仕様](requirements/ble-communication-spec.md)

### 🏗️ [設計文書](design/)
システム・ハードウェア・ソフトウェアの設計文書

- [システム構成](design/system-architecture.md)
- [ハードウェア設計](design/hardware-design.md)
- [ソフトウェア設計](design/software-design.md)
- [BLEプロトコル設計](design/ble-protocol-design.md)

### 🔌 [API仕様](api/)
BLE GATT サービス・コマンド・データ構造の詳細仕様

- [GATT サービス](api/ble-gatt-services.md)
- [コマンドリファレンス](api/command-reference.md)
- [データ構造](api/data-structures.md)

### ⚙️ [開発ガイド](development/)
開発環境構築・コーディング規約・テスト手順

- [環境構築](development/setup-guide.md)
- [ビルド手順](development/build-instructions.md)
- [テストガイド](development/testing-guide.md)
- [コーディング規約](development/coding-standards.md)

### 🚀 [デプロイガイド](deployment/)
インストール・設定・運用手順

- [インストールガイド](deployment/installation-guide.md)
- [設定方法](deployment/configuration.md)
- [トラブルシューティング](deployment/troubleshooting.md)

### 👤 [ユーザーガイド](user/)
エンドユーザー向けの使用方法・FAQ

- [クイックスタート](user/quick-start.md)
- [ユーザーマニュアル](user/user-manual.md)
- [よくある質問](user/faq.md)

## 🖼️ 図表・リソース

- [images/](assets/images/) - スクリーンショット・写真
- [diagrams/](assets/diagrams/) - システム図・フローチャート
- [schemas/](assets/schemas/) - データスキーマ・定義ファイル

## 📝 ドキュメント作成ガイドライン

### マークダウン記法
```markdown
# レベル1見出し（ページタイトル）
## レベル2見出し（セクション）
### レベル3見出し（サブセクション）

**太字** *斜体* `コード`

- リスト項目
- リスト項目

1. 番号付きリスト
2. 番号付きリスト

| 列1 | 列2 | 列3 |
|-----|-----|-----|
| 値1 | 値2 | 値3 |
```c
// コードブロック
void function() {
    printf("Hello, World!\n");
}