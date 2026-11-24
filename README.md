# usb2can-stm32-mdk-firmware

Robotell USB-CAN 어댑터(STM32F103 / APM32F103)용 펌웨어입니다. 같은 보드에 올리는 펌웨어 두 가지가 폴더별로 들어 있습니다.

| 폴더 | 용도 | PC에서 보이는 것 |
|---|---|---|
| [`firmware-can`](firmware-can/README.md) | 원래의 USB-CAN 어댑터 펌웨어 | Robotell 패킷(`AA AA … 55 55`)에 담긴 CAN 프레임 |
| [`firmware-softuart`](firmware-softuart/README.md) | CAN 트랜시버를 물리 계층으로만 쓰고 실제 신호는 UART인 현장용 | 버스에 실린 UART 바이트 그대로 |

두 폴더는 각각 독립된 Keil 프로젝트이며 서로 파일을 공유하지 않습니다. 폴더 하나만 복사해도 빌드됩니다.

## 어느 쪽을 써야 하나

- 버스에 **CAN 프레임**이 흐르면 `firmware-can`을 씁니다.
- 버스에 CAN 트랜시버가 달려 있지만 파형을 재 보면 **UART**(스타트 비트 + 8비트 + 스톱 비트)이면 `firmware-softuart`를 씁니다.

`firmware-can`을 UART 버스에 연결하면 안 됩니다. CAN 컨트롤러가 UART 파형을 깨진 CAN 프레임으로 보고 에러 프레임을 내보내 설비 통신을 방해할 수 있습니다.

## 공통 하드웨어

| 항목 | 내용 |
|---|---|
| MCU | STM32F103 (핀·레지스터 호환인 APM32F103 포함) |
| 클럭 | 외부 12MHz 크리스털, PLL ×6 = 72MHz |
| CAN 트랜시버 | RXD = PB8, TXD = PB9 (CAN 리맵) |
| PC 연결 | USART2 (PA2/PA3) → 보드의 USB-시리얼 칩, 기본 115200bps 8N1 |
| LED | PA0, 데이터가 지나가면 깜빡임 |

## 빌드

1. Keil MDK(µVision)에서 각 폴더의 `Project\USB2CAN.uvproj`를 엽니다.
2. Build(F7)를 누릅니다.
3. 결과물은 `Project\Obj`에 생깁니다.

| 폴더 | hex 파일 |
|---|---|
| `firmware-can` | `Project\Obj\Template.hex` |
| `firmware-softuart` | `Project\Obj\USB2CAN_SoftUart.hex` |

빌드 환경 조건은 다음과 같습니다.

- MDK4 형식 프로젝트라 MDK5에서는 Legacy Support가 필요합니다.
- 컴파일러는 ARM Compiler 5(armcc)입니다. MDK 5.42 + ARM Compiler 5.06u7에서 빌드를 확인했습니다.
- RTX 커널(RL-ARM)은 Keil에 포함된 라이브러리를 씁니다.
- 코드 크기가 약 22KB라 MDK-Lite(32KB 제한)로도 빌드됩니다.

## 굽기

SWD 디버거(ST-Link, J-Link 등)로 hex 파일을 씁니다. 굽기 전에 장비에 들어 있던 펌웨어를 먼저 읽어서 백업해 두세요. 읽기 보호(RDP)가 걸려 있으면 백업이 안 되고, 보호를 풀면 칩 전체가 지워집니다.

## 변경 이력

[VersionHistory.md](VersionHistory.md)를 참고하세요.
