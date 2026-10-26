# Imperial Wings 다른 PC에서 시작하기

1. Windows 배포 ZIP **전체를 압축 해제**합니다. EXE만 따로 복사하지 않습니다.
2. `ImperialWingsLauncher.exe`를 실행합니다. Unreal Engine·Python·Git 설치는 필요 없습니다.
3. 필수 Visual C++ 구성요소 설치 창이 나오면 설치를 마친 뒤 다시 실행합니다. 설치 파일은 `Prerequisites`에 포함돼 있습니다.
4. 송신기의 전원을 켜고 **USB DATA 포트**에 데이터 케이블을 연결합니다. 송신기에서 **USB Joystick**을 선택합니다.
5. 처음 열린 간소 설정 화면에서 **장치 카드**로 조종기를 직접 선택합니다. **REFRESH / NEXT DEVICE**도 사용할 수 있습니다. 상단 STEP 안내를 따르고 X/Y/Z/R/U/V 막대가 실제 스틱 움직임에 따라 변하는지 확인합니다.
6. Mode 2(왼쪽 세로 스로틀)는 **STICK SETUP / MODE 2**를 선택합니다. 안내에 따라 오른쪽 좌우(Roll) → 오른쪽 앞뒤(Pitch) → 왼쪽 좌우(Yaw) → 왼쪽 위아래(Throttle)를 하나씩 양 끝까지 움직입니다. 마지막 유지 위치와 중립 수집 안내를 따릅니다.
7. **CONFIRM & SAVE** 후, 스로틀을 맨 아래로 두고 **ENABLE RADIO**를 누릅니다. 패널을 닫은 뒤 **Space**로 ARM합니다. 다른 스틱은 중앙에서 시작합니다.

각 PC의 실제 축 순서와 방향을 보정합니다. 개발 PC의 Boxer 값은 배포본에 들어 있지 않습니다. 수동 설정은 **ADVANCED OPTIONS**의 AXIS/NORMAL·INVERTED 버튼과 범위 보정으로 수행합니다. **SIMPLE VIEW**로 기본 화면에 돌아옵니다. 기본 입력 API는 Windows WinMM 조이스틱입니다. 모든 HID·XInput 장치가 호환된다고 보장하지 않으며, 네 개의 서로 다른 아날로그 축이 필요합니다.

모든 기체는 ANGLE이며, 스틱을 놓으면 수평을 복원합니다. 고도는 스로틀로 조절합니다. 기본 시점은 경기장 밖 조종자 위치의 LOS입니다. USB 활성화와 ARM은 매번 별도이며 저장된 보정값으로 자동 이륙하지 않습니다.

다음 실행에는 C로 설정을 열어 **CONNECT SAVED RADIO**를 누르거나, 최저 스로틀에서 **F8**로 일치하는 저장 프로필을 연결합니다. 장치 슬롯·펌웨어·모델 출력 설정이 바뀌면 다시 선택·보정해야 할 수 있습니다. 게임 포커스나 USB 연결이 끊기면 수동 USB 기체가 DISARM되며, 다시 연결해도 자동 재개하지 않습니다.

설정과 진단은 `%LOCALAPPDATA%\ImperialWingsSimulator\Saved`에 저장됩니다. `Config\RadioProfile.json`은 해당 Windows 사용자의 보정값, `Config\VisualQuality.txt`는 화질입니다. 버전 업데이트는 새 폴더에 압축 해제해도 설정을 유지합니다. 다른 사람에게 이 사용자 데이터 폴더를 복사하지 않습니다.

연결 문제는 설정 화면의 **EXPORT DEVICE REPORT**를 눌러 `Diagnostics`의 JSON을 확인합니다. 장치가 없으면 Windows의 `joy.cpl`에서도 움직이는지 먼저 확인합니다. 진단 파일에는 사용자 저장 경로와 장치 이름이 들어 있으므로 공개 저장소에는 올리지 않습니다.

**비행 데이터 수집:** F6로 연속 기록을 시작하고 다시 눌러 종료합니다. 결과는 사용자 데이터의 `FlightLogs/*.jsonl`입니다. F5는 그 순간의10대 상태를 `Reports/*-fleet.csv`로 저장합니다. 착륙·DISARM·F6 종료 후 폴더를 확인합니다. [README의 수집 순서·파일 해석·공유 안내](../README.md#data-collection)를 참고하세요.

자율 훈련용 실제 SDACS APF·Python·NumPy는 배포 폴더에 포함됩니다. 실행기는 로컬 전용 연결을 빈 포트에 만들고 게임 종료 시 함께 종료합니다. 자율 연결이 실패해도 수동 조종은 가능합니다. 공식 경기 조종은 사람이 수행하며 AUTO/HOLD/RETURN은 연습 보조입니다.

지원 대상은 Windows 10/11 x64, DirectX 12·Shader Model 6을 지원하는 GPU와 최신 드라이버입니다. 기본 화질은 Balanced이며 게임의 GRAPHICS에서 조절합니다. 모든 GPU·조종기·Windows 조합이나 실제 다른 PC의 USB 비행을 인증한 버전은 아닙니다. 개발 PC의 분리된 새 사용자 데이터·한글/공백 경로·설치 Python/Git 경로를 뺀 실행 검사는 공개 검증 기록에 따로 남깁니다.

종료는 게임 상단 **EXIT** 또는 창 닫기를 사용합니다. 배포본의 `SHA256SUMS.txt`와 `release-manifest.json`은 파일 손상·버전 확인용이며 코드 서명을 대체하지 않습니다.
