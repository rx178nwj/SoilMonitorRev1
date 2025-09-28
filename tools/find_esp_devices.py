#!/usr/bin/env python3
import subprocess
import sys
import re
import argparse

# pyserialライブラリが必要です。インストールされていない場合は、
# pip install pyserial
try:
    import serial.tools.list_ports
except ImportError:
    print("エラー: pyserialライブラリが見つかりません。", file=sys.stderr)
    print("次のコマンドでインストールしてください: pip install pyserial", file=sys.stderr)
    sys.exit(1)

def parse_esptool_output(output_text):
    """esptool.pyの出力を解析して情報を辞書として返します。"""
    info = {}
    patterns = {
        'esptool_version': r'esptool.py (v[\d.]+)',
        'chip_type': r'Detecting chip type... (ESP32-S\d|ESP32-C\d[\w-]*|ESP32-H\d[\w-]*|ESP32-P\d|ESP32)',
        'features': r'Features: (.+)',
        'crystal': r'Crystal is (\d+MHz)',
        'usb_mode': r'USB mode: (.+)',
        'mac': r'MAC: ([0-9a-fA-F:]+)',
        'manufacturer': r'Manufacturer: ([0-9a-fA-F]+)',
        'device': r'Device: ([0-9a-fA-F]+)',
        'flash_size': r'Detected flash size: (.+)',
    }

    for key, pattern in patterns.items():
        match = re.search(pattern, output_text)
        if match:
            info[key] = match.group(1).strip()
        else:
            info[key] = "N/A"
            
    return info

def find_esp_devices(verbose=False):
    """
    接続されているシリアルポートをスキャンし、ESPデバイスの情報を表形式で表示します。
    """
    if verbose:
        print(">>> 利用可能なシリアルポートをスキャンしています...")

    try:
        ports = serial.tools.list_ports.comports()
    except Exception as e:
        print(f"エラー: シリアルポートの取得に失敗しました: {e}", file=sys.stderr)
        return

    if not ports:
        if verbose:
            print("--- シリアルポートが見つかりませんでした。")
        return

    if verbose:
        print(f"--- {len(ports)}個のシリアルポートが見つかりました。ESPデバイスを確認します。")

    detected_devices = []

    for port in sorted(ports):
        if verbose:
            print(f"\n>>> ポート '{port.device}' を確認中...")
        
        command = ["esptool.py", "--port", port.device, "flash_id"]

        try:
            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                timeout=15
            )

            if result.returncode == 0:
                if verbose:
                    print(f"--- [成功] ESPデバイスを '{port.device}' で検出しました。")
                device_info = parse_esptool_output(result.stdout)
                device_info['port'] = port.device
                detected_devices.append(device_info)
            elif verbose:
                error_message = result.stderr.strip().split('\n')[-1]
                print(f"--- [失敗] '{port.device}': ESPデバイスに応答がありませんでした。({error_message})")

        except FileNotFoundError:
            print("エラー: 'esptool.py' が見つかりません。パスが通っているか確認してください。", file=sys.stderr)
            return
        except subprocess.TimeoutExpired:
            if verbose:
                print(f"--- [失敗] '{port.device}': タイムアウトしました。")
        except Exception as e:
            if verbose:
                print(f"--- [エラー] '{port.device}': {e}", file=sys.stderr)
    
    if detected_devices:
        # 表のヘッダー
        headers = {
            'port': 'Serial Port', 'esptool_version': 'esptool.py', 'chip_type': 'Chip Type',
            'features': 'Features', 'crystal': 'Crystal', 'usb_mode': 'USB Mode',
            'mac': 'MAC', 'manufacturer': 'Manufacturer', 'device': 'Device',
            'flash_size': 'Flash Size'
        }
        
        # 各列の最大幅を計算
        col_widths = {key: len(title) for key, title in headers.items()}
        for device in detected_devices:
            for key, value in device.items():
                col_widths[key] = max(col_widths.get(key, 0), len(value))

        # ヘッダー行の作成
        header_line = " | ".join(headers[key].ljust(col_widths[key]) for key in headers)
        separator = "-+-".join("-" * col_widths[key] for key in headers)
        
        print("\n" + "="*len(header_line))
        print("検出されたESPデバイス:")
        print("="*len(header_line))
        print(header_line)
        print(separator)

        # データ行の作成
        for device in detected_devices:
            row_line = " | ".join(device.get(key, "N/A").ljust(col_widths[key]) for key in headers)
            print(row_line)
        print("="*len(header_line))

    elif verbose:
        print("\n" + "="*50)
        print("有効なESPデバイスは見つかりませんでした。")
        print("="*50)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="接続されているESPデバイスをスキャンし、その情報を表形式で表示します。",
        epilog="使用例: python3 find_esp_devices.py -v"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="スキャン中の各ポートの試行など、詳細な情報を表示します。"
    )
    args = parser.parse_args()
    
    find_esp_devices(args.verbose)