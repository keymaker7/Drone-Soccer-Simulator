# RadioMaster Boxer USB 조종기 연결·보정

현재 Windows 빌드는 **네이티브 WinMM 조이스틱 입력과 게임 내 보정 패널**을 사용합니다. 실행할 때마다 키보드 입력으로 시작합니다. 저장된 보정값이 있어도 조종기가 자동으로 활성화되거나 기체가 자동 ARM되지는 않습니다.

## 1. Windows에서 조종기 확인

1. 송신기를 켜고 데이터 전송이 가능한 USB 케이블로 PC에 연결합니다. 송신기에서 **USB Joystick**을 선택합니다.
2. Windows의 `joy.cpl`에서 해당 장치를 열고 스틱을 하나씩 움직여 축 값이 실제로 변하는지 확인합니다.
3. 조종기의 모델 출력·믹서와 USB 축 설정을 확인합니다. EdgeTX의 Classic/Advanced 설정에 따라 전달되는 축이 달라질 수 있으므로 채널 순서를 추측해서 적용하지 않습니다. 설정 설명은 [EdgeTX USB Joystick 공식 매뉴얼](https://manual.edgetx.org/color-radios/model-settings/model-setup/usb-joystick)을 참고합니다. 이 링크는 컬러 화면 매뉴얼이므로 Boxer의 펌웨어·화면에 따라 메뉴 위치와 모양은 다를 수 있습니다.

2026-10-26 Windows PnP 상위 장치 조회에서 `0416:8258`은 **Archon M1 PRO MAX**로 확인됐습니다. 사용자 재연결 후 `1209:4F54`의 USB 제품명이 **Radiomaster Boxer Joystick**으로 확인됐고, WinMM 슬롯1·6축·24버튼 실제 입력을 읽었습니다. WinMM UI에는 두 장치 모두 `Microsoft PC-joystick driver`라는 일반 이름으로 보일 수 있으므로6축·**24버튼** 장치를 선택합니다. 게임에서 실제 장치 선택·라이브 축 표시까지 확인했지만 네 스틱의 전체 범위·방향 보정은 별도 검증입니다. `Tests/radio_probe.py`는 읽기 전용 진단 도구이며 보정값을 만들어 넣거나 ARM하지 않습니다.

## 2. 게임에서 장치와 네 축 선택

1. 먼저 선택 기체를 **MANUAL / DISARM** 상태로 전환한 다음 **C** 또는 **RADIO** 버튼으로 `RADIO / USB CONTROLLER` 패널을 엽니다. 패널을 열면 선택 기체가 수동·DISARM 상태로 전환됩니다. 범위 수집 시작과 USB 활성화 버튼도 선택 기체를 수동·DISARM 상태로 전환합니다.
2. **REFRESH**로 연결 장치를 다시 조회하고 **NEXT DEVICE**로 사용할 장치를 명시적으로 선택합니다. 한 대만 연결되어 있어도 최초 장치 선택이 필요합니다. 표시된 장치 이름·축 수·`CONNECTED` 상태를 확인합니다.
3. `LIVE STICK AXES`의 X/Y/Z/R/U/V 막대를 보며 스틱을 **하나씩** 움직입니다. 움직임이 확인된 축을 오른쪽의 `ROLL`, `PITCH`, `YAW`, `THROTTLE`에 배정합니다. 각 **AXIS** 버튼은 사용할 수 있는 다음 축으로 바뀝니다.
4. 네 채널에는 서로 다른 네 축을 배정합니다. 잘못된 방향은 **NORMAL / INVERTED**로 뒤집습니다. 의도한 입력 방향은 Roll 오른쪽 +, Pitch 전진 +, Yaw 오른쪽 +, Throttle 최저 0·최대 1입니다.
5. Deadzone은 기본 4%이며 **− / +**로 0–25% 범위에서 조절합니다. 스틱 중립 부근의 작은 흔들림을 제거하는 값입니다.

막대 값은 Windows 드라이버 좌표를 장치가 보고한 범위로 나눈 0–1 값입니다. 원시 HID 패킷이나 비행 보정이 완료된 조종값을 뜻하지 않습니다. 축·반전을 바꾸면 기존 보정은 무효가 되고 USB 입력은 비활성화됩니다.

## 3. 실제 스틱 범위와 중립 보정

다음 버튼을 순서대로 사용합니다.

1. **1 START RANGE**: 네 스틱 채널을 실제 양 끝까지 천천히 움직입니다. 스로틀도 최저와 최고까지 움직여 전체 이동 범위를 수집합니다. 이 과정에서 기체는 DISARM 상태입니다.
2. **2 CAPTURE RANGE**: 범위 수집을 끝냅니다. 16개 이상의 실제 입력 샘플과 각 배정 축에서 드라이버 전체 범위의 25% 이상 이동이 필요합니다. 이 수치는 최소 검증 조건이며, 보정할 때는 스틱의 전체 이동 범위를 사용합니다. 정지한 축이나 중립값만으로는 완료되지 않습니다.
3. **3 CAPTURE NEUTRAL**: Roll/Pitch/Yaw를 중립에 놓고 스로틀을 물리적 최저점에 둔 뒤 누릅니다. 중립은 측정 범위의 가운데 60% 안에 있어야 하며 반전 적용 후 스로틀 명령이 5% 이하여야 합니다.
4. **4 FINISH & SAVE**: 네 축의 범위·중립·최저 스로틀 확인을 검증하고 장치 프로필을 저장합니다. 상태 메시지가 실패 원인을 표시하면 해당 단계부터 확인합니다.

보정 후 오른쪽 조종값에서 중립 Roll/Pitch/Yaw가 0, 최저 Throttle이 0 부근인지 확인합니다. 축을 하나씩 다시 움직여 방향과 끝값을 확인합니다. 잘못된 배정·반전을 수정했다면 범위 수집부터 다시 진행합니다. 버튼을 반복해서 누르는 것으로 실제 스틱 이동을 대체할 수 없습니다.

## 4. USB 입력 활성화와 ARM

**ENABLE RADIO**는 현재 연결이 정상이고 보정이 유효하며 스로틀이 5% 이하일 때만 USB 조종을 활성화합니다. 활성화 후에도 기체는 DISARM 상태입니다. 패널을 닫고 **Space** 또는 ARM 버튼으로 별도 ARM합니다. 경기 준비 10초·세트 휴식·종료 상태에서는 ARM이 차단되며 연습 상태와 경기 진행 중에만 허용됩니다.

수동 비행은 Acro 각속도 방식입니다. 스틱을 중립으로 놓으면 회전 명령이 중립이 되며, 기체 자세가 자동 수평으로 복원되는 모드는 아닙니다. 자동 위치 유지가 필요하면 관제의 HOLD 기능을 사용합니다.

**USE KEYBOARD**는 USB 입력을 비활성화하고 선택 기체를 DISARM합니다. 다른 기체를 선택하면 기존 수동 기체도 DISARM됩니다. 조종기 버튼의 ARM 스위치 매핑은 이 버전에 포함하지 않았으며 ARM은 게임의 Space/버튼으로 수행합니다.

## 5. 연결 손실과 프로필

- 장치 읽기가 실패하면 즉시 USB 입력을 비활성화하고 입력값을 지웁니다. 마지막 성공 입력이 250 ms를 넘으면 입력을 오래된 값으로 판정합니다. 이는 실제 입력 지연을 측정한 결과가 아니라 구현한 유효기간입니다.
- USB로 조종 중인 기체는 입력 손실·USB 비활성화·유효 보정 손실·게임 포커스 손실 시 DISARM합니다. DISARM 후에는 중력과 충돌이 계속 적용되므로 공중에서 정지하거나 위치가 유지되지는 않습니다.
- 재연결만으로 USB 입력이나 ARM이 재개되지 않습니다. 최저 스로틀에서 **ENABLE RADIO**와 ARM을 다시 수행합니다. 보정 도중 연결이 끊기면 범위 수집을 새로 시작합니다.
- 프로필은 실행 중인 프로젝트의 `Saved/Config/RadioProfile.json`에 장치 식별값별로 저장합니다. **SAVE PROFILE / LOAD PROFILE**로 수동 저장·불러오기를 할 수 있으며, 불러오기는 항상 USB 입력을 비활성화합니다. 개발 프로젝트와 패키지의 Saved 폴더는 서로 다를 수 있습니다.
- 식별값은 WinMM 슬롯, 드라이버가 보고한 이름·제조사/제품 값·범위·기능을 포함합니다. WinMM은 여기서 USB 일련번호를 제공하지 않으므로 동일 모델·동일 기능·동일 슬롯의 다른 실물을 완전히 구분할 수 없습니다. 조종기를 교체했거나 USB/모델 설정을 바꿨다면 실제 축을 다시 확인하고 보정합니다.
- 드라이버 슬롯이나 장치 기능이 바뀌면 기존 프로필과 일치하지 않을 수 있습니다. 잘못된 형식·범위·중복 축·다른 식별값의 프로필은 활성화에 사용하지 않습니다.

## 검증 범위

네이티브 입력 코드와 패키지 빌드는 완료했습니다. 독립 MSVC 보정 수학 테스트는 `Tests/run-radio-tests.ps1`에서 **115개 확인, 실패 0개**입니다. 범위 수집, 중립·반전·데드존, 잘못된 축·숫자, 연결 손실 시 값 제거를 검사합니다. 이 테스트는 내부 수치 자료를 사용하며 실물 조종기 프로필을 생성하지 않습니다.

실물 Boxer의 네 스틱 전 범위·방향 보정, USB로 실제 기체를 조종하는 검사와 케이블 분리 시 동작 확인은 아직 완료로 기록하지 않았습니다. 게임 화면에서의 네이티브 패널·버튼 검사는 `docs/verification.md`의 최신 기록을 확인합니다.

구현: `Source/ImperialWingsSimulator/ImperialRadioSubsystem.{h,cpp}`, `RadioCalibration.{h,cpp}`, `SoccerDronePawn.cpp`, `ImperialHUD.cpp`.

WinMM 입력은 Microsoft의 [joyGetPosEx](https://learn.microsoft.com/en-us/windows/win32/api/joystickapi/nf-joystickapi-joygetposex), [JOYCAPSW](https://learn.microsoft.com/en-us/windows/win32/api/joystickapi/ns-joystickapi-joycapsw), [joyGetNumDevs](https://learn.microsoft.com/en-us/windows/win32/api/joystickapi/nf-joystickapi-joygetnumdevs) 문서를 기준으로 구현했습니다.
