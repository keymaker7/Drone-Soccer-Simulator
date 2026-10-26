# v0.5 상세 실행 안내 보관본

이 문서는 이전 v0.5 배포의 안내를 보존합니다. 최신 v0.6의 실행기·사용자별 설정 경로·독립 자율 런타임은 [새 실행 안내](PORTABLE_QUICK_START.ko.md)를 따릅니다.

# Imperial Wings Drone Soccer Simulator

Unreal Engine 5.8 기반 Windows 드론축구 시뮬레이터입니다. 기본 조종은 경기장 밖에서 기체를 보는 **LOS(육안) 방식**입니다. 양 팀 10대가 배치된 경기장에서 키보드 또는 보정한 USB 조종기로 한 기체씩 선택해 비행합니다.

**현재 배포 버전: v0.5.0 · 2026-10-26**

**[Windows 실행 버전 ZIP 다운로드](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/download/v0.5.0/ImperialWingsSimulator-Windows-v0.5.0.zip)** · [릴리스 페이지](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.5.0)

| 하고 싶은 일 | 읽을 안내 | 필요한 준비 |
|---|---|---|
| 바로 실행하고 키보드로 비행 | [다운로드와 실행](#windows-run) → [첫 비행](#first-flight) | Windows 64비트 PC, DirectX 12·SM6 지원 그래픽 환경 |
| Boxer로 조종 | [조종기 연결](#boxer) | 실행 버전, Boxer, 데이터 전송용 USB 케이블 |
| 소스 수정·직접 빌드 | [소스 빌드](#source-build) | Git, UE 5.8, Visual Studio C++ 도구 |
| SDACS 자동 훈련 | [자율 훈련 연결](#sdacs) | 소스 프로젝트, Python, 별도 SDACS 원본 |
| 온라인·오프라인 여러 명 경기 | [멀티플레이 계획](development-roadmap.md#multiplayer-plan) | 후속 개발이며 현재 배포본에서는 미지원 |
| 개발 피드백용 진단 파일 | [데이터 수집 계획](development-roadmap.md#feedback-data-plan) | 현재 CSV 외의 진단 묶음은 후속 개발 |

**다운로드 실행 버전의 키보드·USB 수동 비행에는 Unreal Editor, Visual Studio, Python 설치가 필요하지 않습니다.** 개발 도구는 소스를 직접 빌드할 때 사용합니다.

## 목차

1. [Windows 다운로드·압축 해제·시작·종료](#windows-run)
2. [키보드로 첫 이륙·착륙](#first-flight)
3. [조작키와 화면 버튼](#controls)
4. [Boxer USB 연결·Mode 2 보정](#boxer)
5. [포지션·시작 위치·하단 식별 띠](#positions)
6. [수동 경기와 자율 훈련](#match)
7. [화질 설정](#graphics)
8. [소스 다운로드·빌드·실행](#source-build)
9. [SDACS 자율 훈련 연결](#sdacs)
10. [문제 해결](#troubleshooting)
11. [설정·로그·CSV·다운로드 확인](#saved-files)
12. [검증 범위·남은 기능·관련 문서](#verification)

<a id="windows-run"></a>
## 1. Windows 다운로드·압축 해제·시작·종료

### 1-1. 실행 버전 받기

1. 위의 **Windows 실행 버전 ZIP 다운로드**를 누릅니다. 또는 [v0.5.0 릴리스](https://github.com/sun475300-sudo/Drone-Soccer-Simulator/releases/tag/v0.5.0)를 엽니다.
2. 릴리스의 **Assets**에서 **`ImperialWingsSimulator-Windows-v0.5.0.zip`**을 받습니다. 크기는 약 360 MiB입니다.
3. 다운로드가 완전히 끝날 때까지 기다립니다.

저장소의 **Code → Download ZIP**과 릴리스의 **Source code (zip)**은 개발 소스입니다. 바로 실행하려면 **Windows** 이름이 들어간 ZIP을 선택합니다.

### 1-2. 전체 압축 해제하기

1. Windows 파일 탐색기에서 받은 ZIP을 찾습니다.
2. 마우스 오른쪽 버튼을 누르고 **모두 압축 풀기**를 선택합니다.
3. 예를 들어 `C:\Games` 아래에 풉니다. 이 경로는 예시이며 쓰기 가능한 다른 폴더여도 됩니다.
4. 완료 후 **압축이 풀린 일반 폴더**를 엽니다.
5. 실행 파일과 두 폴더가 함께 있는지 확인합니다.

```text
ImperialWingsSimulator-Windows-v0.5.0/
├─ ImperialWingsSimulator.exe       ← 실행할 파일
├─ Engine/                         ← 함께 보관
├─ ImperialWingsSimulator/          ← 게임 모듈·데이터
│  ├─ Binaries/
│  └─ Content/
├─ README-KO.txt
├─ THIRD_PARTY_NOTICES.md
└─ SHA256SUMS.txt
```

확장자가 숨겨져 있으면 파일이 **ImperialWingsSimulator**로 보일 수 있습니다. 종류가 **응용 프로그램**인 항목을 실행합니다. ZIP 창 안에서 실행하거나 exe 하나만 다른 폴더로 옮기면 게임 데이터를 찾지 못할 수 있습니다.

### 1-3. 게임 열기

1. 가장 바깥의 **`ImperialWingsSimulator.exe`**를 더블클릭합니다.
2. 경기장·10대 기체·상태 표시가 나타날 때까지 기다립니다. 처음에는 재질·글꼴 표시가 안정되는 데 시간이 걸릴 수 있습니다.
3. 게임 창의 경기장 빈 부분을 클릭해 키보드 입력을 받을 창으로 선택합니다.
4. **MANUAL TRAINING**, 선택 기체 **DISARM** 상태를 확인합니다.
5. **SDACS / OFFLINE**이 보여도 키보드·USB 수동 비행은 가능합니다. 별도 자율 훈련 연결 상태를 뜻합니다.
6. 처음에는 **START MATCH를 누르지 않고** 연습 상태에서 2절의 첫 비행을 진행합니다.

다운로드 ZIP은 바탕화면 바로가기를 자동 생성하지 않습니다. 원한다면 압축을 푼 exe의 바로가기를 만들고 실제 게임 폴더는 그대로 보관합니다.

### 1-4. 종료하기

스로틀을 줄여 착륙하고 **Space**로 **DISARM**한 뒤 창의 **X** 또는 **Alt+F4**로 닫습니다. 다음에도 같은 압축 해제 폴더에서 실행하면 그 폴더에 저장한 보정·화질 설정을 사용할 수 있습니다.

<a id="first-flight"></a>
## 2. 키보드로 첫 이륙·착륙

**ARM**은 모터 구동 허용, **DISARM**은 모터 정지입니다. ARM만으로 자동 이륙하지 않습니다. **스로틀**은 위쪽으로 미는 힘의 크기입니다.

1. **키보드를 선택합니다.** 처음 시작은 키보드입니다. USB 설정을 열었다면 **C → USE KEYBOARD → CLOSE**로 돌아옵니다.
2. **기체를 선택합니다.** 오른쪽 **TEAM ROSTER** 목록을 클릭하거나 **Tab**을 누릅니다. 처음에는 파란 팀 **STRIKER**로 연습합니다.
3. **시점을 정합니다.** **1**은 자기 진영 밖 조종자 시점입니다. 기체를 찾기 어렵다면 **3**으로 전체 위치를 본 뒤 **1**로 돌아옵니다.
4. **초기화합니다.** **Home**은 모든 기체·점수·경기를 초기화하고 연습으로 돌립니다. 진행 중 경기를 유지해야 하면 누르지 않습니다.
5. **스로틀을 내립니다.** **↓**로 화면의 **THR**을 **0%**로 만듭니다. USB 설정창이 열려 있다면 먼저 닫습니다.
6. **ARM합니다.** **Space** 또는 화면의 **ARM**을 한 번 누릅니다. 거부되면 스로틀·경기 단계·입력 장치 상태 메시지를 확인합니다.
7. **천천히 이륙합니다.** **↑**를 짧게 눌러 스로틀을 조금씩 올립니다. **키를 놓아도 현재 스로틀은 유지**되며 길게 누르면 계속 증가합니다.
8. **작게 움직여 봅니다.** **W/S**는 앞뒤 기울기, **A/D**는 좌우 기울기, **Q/E**는 기수 회전입니다. 기체가 향한 방향이 기준이므로 다가오는 기체는 좌우가 반대로 느껴질 수 있습니다.
9. **자세 복원을 확인합니다.** W/S/A/D를 놓으면 ANGLE 제어가 수평으로 돌립니다. **위치·고도 자동 고정은 아니므로** 관성 이동과 상승·하강은 남을 수 있습니다.
10. **착륙합니다.** **↓**로 스로틀을 조금씩 줄여 바닥에 내려온 뒤 **Space**로 DISARM합니다.
11. **다시 연습합니다.** **Backspace**는 선택 기체, **Home**은 전체 기체와 경기를 초기화합니다.

호버 스로틀은 기울기·배터리 등에 따라 달라집니다. 특정 퍼센트를 정답으로 사용하지 않습니다. 다른 기체를 선택하면 이전 수동 조종 기체가 DISARM될 수 있으므로 착륙 후 바꿉니다.

<a id="controls"></a>
## 3. 조작키와 화면 버튼

### 3-1. 기본 조종

| 입력 | 동작 | 참고 |
|---|---|---|
| **1** | LOS 조종자 시점 | 기본 시점, 자기 진영 밖에서 관찰 |
| **2** | 선택 기체 외부 추적 관찰 | 기체 내부 FPV가 아닌 외부 카메라 |
| **3** | 경기장 전체 관찰 | 기체·골대 위치 파악 |
| **마우스 오른쪽 드래그** | LOS 시선 회전 | 1번 시점의 경기장 빈 부분에서 드래그 |
| **마우스 휠** | LOS 시야각 확대·축소 | 기체 방향을 바꾸는 입력은 아님 |
| **Tab** / 기체 목록 | 조종 기체 선택 | 팀·역할·입력 상태 확인 |
| **Space** | ARM / DISARM | ARM은 낮은 스로틀·허용 경기 단계 필요 |
| **↑ / ↓** | 스로틀 증가 / 감소 | 키를 놓으면 변경된 스로틀 유지 |
| **W / S** | Pitch, 앞뒤 기울기 | 모든 기체에 ANGLE 적용 |
| **A / D** | Roll, 좌우 기울기 | 키를 놓으면 수평 복원 |
| **Q / E** | Yaw, 기수 회전 | 기체 방향 변경 |
| **Backspace** | 선택 기체 초기화 | 선택 기체를 시작 위치로 복귀 |
| **Home** | 전체 초기화 | 기체·점수·경기를 연습 상태로 복귀 |
| **C** | RADIO / USB 패널 | 창을 닫은 뒤 비행 |
| **F10** | 간소 화면 / 전체 패널 | 버튼이 안 보이면 다시 누름 |
| **F5** | 상태 CSV 내보내기 | `Saved/Reports`에 저장 |
| **Alt+F4** / X | 종료 | 착륙·DISARM 후 종료 |

### 3-2. 화면 버튼과 연습 관제

| 버튼·키 | 동작 |
|---|---|
| **MANUAL** | 선택 기체를 수동·DISARM으로 전환. 키보드/USB 선택은 유지 |
| **ARM / DISARM**, **ARM SELECTED** | 선택 기체 모터 구동 상태 변경 |
| **RESET** | 전체 초기화, 연습 상태 복귀 |
| **RADIO / USB** | 장치 선택·보정·입력 전환 |
| **EXPORT CSV / F5** | 실제 기체 상태 내보내기 |
| **START MATCH** | 초기화 후 10초 준비부터 수동 경기 시작 |
| **AUTO FLEET / Enter** | SDACS가 연결된 연습 상태에서 전체 자율 훈련 시작·자동 ARM |
| **HOLD / H** | ARM된 기체를 현재 위치 유지로 전환. 로컬 제어로 가능 |
| **RETURN / R** | 연결된 SDACS로 홈 패드 귀환·착륙. 연결 없으면 로컬 HOLD |

USB 안내 보정창의 **Enter는 보정 단계 진행**에 사용됩니다. 창이 열려 있으면 Space·기체 선택·비행 단축키가 차단됩니다. AUTO FLEET은 여러 사람의 접속 기능이 아니며 공식 경기에는 자율 훈련을 사용하지 않습니다.

<a id="boxer"></a>
## 4. Boxer USB 연결·Mode 2 보정

### 4-1. Windows에서 먼저 확인

1. Boxer를 켜고 **데이터 전송이 가능한 USB 케이블**로 PC에 연결합니다.
2. 송신기에서 **USB Joystick**을 선택합니다. 충전 전용 케이블에서는 입력 장치가 나타나지 않을 수 있습니다.
3. **Win+R → `joy.cpl` → Enter**로 Windows 게임 컨트롤러 창을 엽니다.
4. 해당 장치의 **속성**에서 스틱을 하나씩 움직여 값이 실제로 변하는지 봅니다.
5. 여기서도 변화가 없으면 케이블·USB 모드·송신기 모델 출력을 먼저 확인합니다.

장치명과 축 순서는 펌웨어·USB 설정에 따라 다를 수 있습니다. 개발 PC의 실제 배정은 X/Y/V/Z였지만 다른 장치의 정답으로 쓰지 않습니다. **배포본에는 개인 보정값이 없으므로 자기 조종기에서 보정합니다.**

### 4-2. 게임에서 장치 선택

1. 기체를 바닥의 **MANUAL / DISARM** 상태로 둡니다.
2. **C / RADIO / USB**로 패널을 엽니다.
3. **REFRESH**로 검색합니다.
4. **NEXT DEVICE**로 사용할 장치를 명시적으로 선택합니다. 한 대만 있어도 최초 선택을 진행합니다.
5. **CONNECTED**, 장치명·축·버튼 수, **LIVE STICK AXES**를 봅니다.
6. 스틱 하나만 움직였을 때 막대가 변하는지 확인합니다. 다른 게임패드가 있다면 실제 Boxer인지 확인합니다.
7. **STICK SETUP / MODE 2**를 선택합니다.

### 4-3. 네 축 안내 보정

Mode 2는 왼쪽 세로가 스로틀, 오른쪽 스틱이 Roll/Pitch입니다. 매 단계 전에 나머지 축은 중립, 스로틀은 최저로 둡니다.

| 순서 | 축 | 움직임 | 끝값 확인 시 유지 위치 |
|---|---|---|---|
| 1 | Roll | 오른쪽 스틱 좌우 양 끝 | 오른쪽 끝 |
| 2 | Pitch | 오른쪽 스틱 앞뒤 양 끝 | 앞쪽 끝 |
| 3 | Yaw | 왼쪽 스틱 좌우 양 끝 | 오른쪽 끝 |
| 4 | Throttle | 왼쪽 스틱 위아래 전 범위 | 맨 아래 |

각 단계에서 다음 순서를 지킵니다.

1. **START THIS STEP / Enter**로 시작합니다.
2. 지정한 축만 양 끝까지 천천히 움직이고 각 끝에서 약 1초 유지합니다.
3. 표의 마지막 위치를 유지하며 **CONFIRM HELD END / Enter**를 누릅니다.
4. 다음 단계 전에 중립·최저 스로틀로 돌려놓습니다.
5. 잘못 움직였다면 **RETRY THIS STEP**으로 다시 진행합니다.

네 축이 끝나면 다음을 진행합니다.

1. Roll/Pitch/Yaw는 중립, 스로틀은 맨 아래로 두고 약 1초 유지합니다.
2. **CAPTURE NEUTRAL / Enter**를 누릅니다.
3. 배정 축과 NORMAL/INVERTED를 검토합니다.
4. **CONFIRM & SAVE를 클릭**합니다. 최종 저장은 Enter로 실행되지 않습니다.
5. 보정된 값에서 중립 Roll/Pitch/Yaw가 0 부근, 최저 Throttle이 0 부근인지 확인합니다.
6. 스틱을 하나씩 움직여 방향·끝값을 확인한 뒤 스로틀을 다시 맨 아래로 내립니다.
7. **ENABLE RADIO**를 누릅니다. 유효 보정·정상 연결·스로틀 5% 이하가 필요합니다.
8. 패널을 닫고 게임 창을 선택한 뒤 **Space**로 별도 ARM합니다.

**저장 → USB 활성화 → ARM은 별도 단계**입니다. 재연결·프로필 불러오기만으로 자동 활성화나 ARM은 하지 않습니다. 축 배정·반전을 바꾸면 다시 보정합니다.

### 4-4. 입력 전환과 연결 끊김

- 조종기 스위치 ARM 배정은 아직 미지원입니다. Space 또는 화면 버튼을 사용합니다.
- 키보드 복귀: **C → USE KEYBOARD → CLOSE**.
- USB 입력 손실·포커스 상실 시 조종 기체가 DISARM될 수 있습니다. 공중 DISARM 후 자동 호버하지 않으며 중력에 따라 내려갑니다.
- 재연결 후 최저 스로틀에서 ENABLE RADIO → 창 닫기 → ARM을 다시 수행합니다.
- **CANCEL SETUP / CLOSE / C**로 미완료 보정을 취소하면 이전 저장 프로필을 보존합니다.

[Boxer 상세 안내](BOXER_SETUP.md)에 수동 보정·메시지·저장 위치를 설명했습니다. 실제 보정·USB 활성화·최저 스로틀 ARM은 확인했지만 연속 USB 비행·케이블 분리 복구 시험은 미완료입니다.

<a id="positions"></a>
## 5. 포지션·시작 위치·하단 식별 띠

**스트라이커는 자기 시작선의 정중앙**입니다. 경기장 중앙선에서 시작한다는 뜻은 아닙니다. 조종자가 자기 팀을 보는 기준으로 왼쪽부터 다음과 같습니다.

```text
Libero  —  Guide  —  Striker  —  Sweeper  —  Keeper
하늘색      분홍색      초록색       노란색       흰색
```

시작선은 길이 방향 `x=±6.5m`, Striker는 `y=0`이며 간격은 1.25m입니다. 반대 팀은 대칭입니다. 기체·패드·초기화·홈·LOS 조종석에 같은 배치를 적용합니다.

| 표시 | 배포본 표현 |
|---|---|
| 팀 | 빨강 / 파랑 원형 LED |
| 역할 | 위 역할색과 목록의 역할명 |
| 전방 / 후방 | 흰색 한 줄 / 빨간색 두 줄 방향 LED |
| Striker 하단 | 초록 띠, 좌우 각각 2개, 기체당 4개 |
| Guide 하단 | 분홍 띠, 좌우 각각 2개, 기체당 4개 |
| 보조 표시 | 기존 S/G 태그 |

[포지션·LED 설계](position-and-led-design.md)에서 역할 규정과 훈련용 방향등·배치를 구분합니다. **좌우 각각 2개는 사용자 요청에 따른 설계**이며 공식 규정의 개수로 단정하지 않습니다.

[하단 띠 실제 장착 조사](striker-tag-mounting.md)도 참고합니다. 현재 띠는 시각 메시이며 실제 천·탈락·파손 물리는 없습니다. **띠 탈락의 득점 불인정·공격수 승계도 아직 구현하지 않았습니다.**

<a id="match"></a>
## 6. 수동 경기와 자율 훈련

### 6-1. 수동 경기 시작

1. 착륙·DISARM 후 **START MATCH**를 누릅니다.
2. 기체·점수를 초기화하고 LOS 시점으로 바뀝니다.
3. **READY / MOTORS LOCKED** 10초 동안 기다립니다. 이때 ARM은 제한됩니다.
4. **SET** 진행 상태가 되면 낮은 스로틀에서 직접 ARM하고 비행합니다.
5. 세트는 180초, 세트 사이 정비는 300초, 최대 3세트·2선승의 기본 흐름입니다.
6. **Home / RESET**은 점수까지 초기화하고 연습으로 복귀합니다.

경기 시작은 자동 ARM·이륙이 아닙니다. 현재 한 사람이 기체를 선택해 조종하며 **10명 동시 멀티플레이는 후속 개발**입니다.

### 6-2. 규정·구현 범위

기준은 [FIDA Class40 제5차 개정·2026년 시행 규정](https://fidasystem.com/rules.html)입니다. 경기장 16×7×4.5m, 보호구 외경 40cm, 골대 내경 60cm, 선택한 골대 중심 높이는 3.0m입니다.

스트라이커가 상대 골대를 앞에서 뒤로 완전히 통과해야 득점하고 활성 기체 모두 자기 진영으로 복귀해야 팀 오프사이드를 해제합니다. 심판·페널티·연장·승부치기·VAR·공격수 승계 전체를 완성한 버전은 아닙니다. [규정 근거와 한계](rules-sources-2026.md)를 확인합니다.

**AUTO FLEET은 Practice 자동 훈련 보조**입니다. 수동 경기와 구분하며 [9절](#sdacs)의 별도 연결이 필요합니다.

<a id="graphics"></a>
## 7. 화질 설정

1. 전체 관제 패널 왼쪽의 **GRAPHICS**를 찾습니다.
2. 숨겨져 있다면 **F10**으로 패널을 표시합니다.
3. 다음 버튼을 클릭합니다.

| 버튼 | 용도 |
|---|---|
| **BALANCED** | 끊김이 있을 때. 내부 렌더 해상도·효과 수를 낮춤 |
| **HIGH** | 기본 화질, 원래 해상도로 기체·경기장 관찰 |
| **CINEMA** | 반사·간접광·효과 품질 향상, 그래픽 부하 증가 |

선택은 `Saved/Config/VisualQuality.txt`에 저장합니다. 조종 중 끊김이 느껴지면 BALANCED를 먼저 사용합니다. 먼지는 바닥 충격·저고도 추력에 반응하는 시각 효과이며 실제 부품 파괴 판정은 아닙니다.

검증 PC는 UE 5.8.3 / RTX 5070 Ti / RAM 32GB입니다. 최종 패키지의 실제 1920×1080 화면을 확인했지만 모든 PC의 프레임 수를 보장하지 않습니다. RTX 5060 지속 성능은 별도 측정 대상입니다.

<a id="source-build"></a>
## 8. 소스 다운로드·빌드·실행

### 8-1. 개발 도구 준비

일반 실행 사용자는 이 절차를 건너뜁니다.

- **Git**: 소스 다운로드.
- **Unreal Engine 5.8**: 현재 검증 버전 5.8.3.
- **Visual Studio 2022 C++ 도구·Windows SDK**: 설치 관리자에서 C++ 게임 개발 구성 요소 확인.
- **Python 3.12**: SDACS 자동 훈련 설치 시 사용. 아래 수동 패키지 빌드·실행에는 별도 Python 브리지가 필요하지 않습니다.
- 소스·엔진 중간 파일·패키지를 위한 충분한 디스크 여유 공간. 실행 ZIP보다 훨씬 큰 공간이 필요할 수 있습니다.

### 8-2. 소스 받기

PowerShell을 열고 프로젝트를 둘 폴더에서 실행합니다.

```powershell
git clone https://github.com/sun475300-sudo/Drone-Soccer-Simulator.git
Set-Location .\Drone-Soccer-Simulator
Get-Item .\ImperialWingsSimulator.uproject
```

프로젝트 파일이 보이면 올바른 폴더입니다. 이후 명령은 이 **프로젝트 루트**에서 실행합니다. Git을 찾지 못하면 설치 후 새 터미널을 엽니다. 이미 소스가 있으면 자기 프로젝트 폴더에서 시작합니다.

### 8-3. Windows 실행 파일 만들기

엔진 경로를 실제 PC에 맞춥니다. 지정한 폴더 바로 아래에 `Engine`이 있어야 합니다.

```powershell
$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat')
.\Scripts\Build.ps1 -EngineRoot $EngineRoot -Package
```

1. 경로 검사 결과가 **True**인지 봅니다.
2. 에디터 C++ 컴파일·Windows 콘텐츠 패키징이 끝날 때까지 기다립니다.
3. 중단됐다면 앞쪽의 첫 오류부터 확인합니다.
4. 성공 시 다음 실행 파일이 생성됩니다.

```text
dist/Integrated/Windows/ImperialWingsSimulator.exe
```

**`-Package`가 실행 파일 생성에 필요합니다.** `Build.ps1`만 실행하면 에디터용 컴파일을 하며 아래 실행 스크립트가 요구하는 Windows 패키지를 만들지 않습니다.

맵·자체 제작 콘텐츠는 저장소에 있습니다. 콘텐츠 생성은 `-GenerateContent`를 지정하거나 맵이 없을 때 수행합니다. 일반 빌드에서 무조건 콘텐츠를 다시 생성하지 않습니다.

### 8-4. 수동 실행·빌드 일치 확인

```powershell
.\ImperialWings.cmd -ManualOnly
```

브리지 없이 게임을 엽니다. 이후 [첫 비행](#first-flight)을 따릅니다. 창을 열지 않고 상태만 확인하려면 다음을 사용합니다.

```powershell
.\Scripts\Launch.ps1 -Inspect -ManualOnly
```

`exists=True`는 실행 파일 존재, `matches_source=True`는 빌드 입력 일치입니다. 소스·설정·콘텐츠를 바꾸었다면 다시 `Build.ps1 -EngineRoot $EngineRoot -Package`로 만듭니다. README 변경만으로 빌드 입력이 바뀌지는 않습니다.

### 8-5. 에디터로 열기

`ImperialWingsSimulator.uproject`를 UE 5.8로 엽니다. 에디터용 컴파일은 `Build.ps1 -EngineRoot $EngineRoot`로 실행합니다. 기본 맵은 `Content/Maps/L_DroneSoccerArena.umap`입니다.

에디터 시험과 패키지 시험은 따로 기록합니다. 다른 엔진 버전의 변환·빌드는 현재 검증 범위에 포함하지 않습니다.

<a id="sdacs"></a>
## 9. SDACS 자율 훈련 연결

**소스 프로젝트에서 자동 훈련을 별도 설치할 때** 사용합니다. 다운로드 ZIP에는 Python 환경·SDACS 원본·개인 PC 경로가 없습니다.

### 9-1. 경로 설정·설치

프로젝트 루트의 PowerShell에서 엔진·Python 경로를 자기 PC에 맞춥니다. 예시는 SDACS를 프로젝트와 같은 상위 폴더에 둡니다.

```powershell
$EngineRoot = 'C:\Program Files\Epic Games\UE_5.8'
$PythonExe = 'C:\Path\To\Python312\python.exe'
$SdacsRoot = [IO.Path]::GetFullPath((Join-Path $PWD '..\swarm-drone-atc'))

.\Scripts\Configure-Environment.ps1 -EngineRoot $EngineRoot -Python $PythonExe -RepoRoot $SdacsRoot -BridgePort 8767
.\Scripts\Setup-Bridge.ps1
```

- `C:\Path\To\Python312\python.exe`는 **수정해야 하는 예시 자리**입니다.
- Configure는 `SimulatorEnvironment.local.json`에 경로를 저장합니다.
- Setup은 SDACS 작업 사본이 없으면 복제하고 프로젝트 `.venv`에 의존성을 설치합니다. 이 단계는 인터넷이 필요합니다.
- SDACS가 이미 있다면 `$SdacsRoot`에 그 실제 폴더를 지정합니다.
- 로컬 경로·가상환경·개인 보정값은 Git에 올리지 않습니다.

### 9-2. 빌드·통합 실행

```powershell
.\Scripts\Build.ps1 -Package
.\ImperialWings.cmd
```

이제 Build는 저장한 엔진 경로를 사용합니다. 같은 입력의 통합 패키지가 이미 있다면 다시 빌드할 필요는 없습니다.

일반 `ImperialWings.cmd`는 연결 상태를 확인하고 필요하면 브리지를 시작한 뒤 게임을 엽니다. 기본 주소는 **`http://127.0.0.1:8767`**, 실행 파일은 `dist/Integrated/Windows`의 패키지입니다.

1. **SDACS / APF LINKED**를 확인합니다.
2. 연습 상태인지 봅니다. 경기 중이면 **Home / RESET**으로 연습에 복귀합니다.
3. **AUTO FLEET / Enter**로 훈련을 시작합니다. 전체 자동 ARM을 수행합니다.
4. **HOLD / H**는 현재 위치 유지, **RETURN / R**은 홈 패드 귀환·착륙입니다.
5. 수동 복귀는 기체 선택 → **MANUAL** → 낮은 스로틀에서 별도 ARM입니다.

### 9-3. 연결 상태·포트

브리지가 이미 실행된 상태에서 조회만 하려면 다음을 사용합니다.

```powershell
Invoke-RestMethod 'http://127.0.0.1:8767/health'
```

정상 응답은 `source=swarm-drone-atc`, `protocol_version=2`입니다. 실제 APF 파일·소스 커밋·SHA-256도 확인합니다. 응답이 없으면 설치·실행 단계와 `Saved/bridge-integrated-errors.log`를 확인합니다.

`Bridge/sdacs_bridge.py`를 직접 실행할 때의 기본값 **8766**은 통합 기본값 **8767**과 다릅니다. 직접 구성한다면 브리지 `--port`와 게임 `-ImperialBridgeUrl`을 맞춥니다. 처음에는 위 통합 실행을 사용합니다. 다른 서비스가 포트를 쓰면 임의 종료하지 말고 설정 포트를 변경합니다.

확인한 upstream 기준 커밋은 `1e9d826265069696d7e415c7778d17483d7a8580`입니다. 새로 받은 원본은 달라질 수 있으므로 /health를 확인합니다. Unreal의 실제 상태와 원본 APF를 사용하며 브리지가 합성 비행 위치를 만들지 않습니다.

**최종 v0.5.0 검사에서는 브리지 자동 시작이 실행 정책에 차단되어 자동 연결을 검증하지 못했습니다.** 위는 스크립트에 근거한 안내이며 새 PC에서 자동 연결을 시험 완료했다는 뜻은 아닙니다. [연동 계약](sdacs-integration.md)과 [검증 이력](verification.md)을 참고합니다.

<a id="troubleshooting"></a>
## 10. 문제 해결

| 증상 | 확인·해결 순서 |
|---|---|
| 다운로드에 exe가 없음 | 소스 ZIP인지 확인하고 릴리스의 **Windows 실행 ZIP** 선택 |
| 게임 데이터를 못 찾음 | 전체 압축 해제, exe와 `Engine`·`ImperialWingsSimulator` 폴더 함께 보관 |
| DLL·Visual C++ 런타임 오류 | 오류 문구를 확인하고 [Microsoft 공식 런타임 안내](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)의 해당 PC용 x64 패키지 확인 |
| DirectX·그래픽 장치 오류 | 현재 DX12·SM6 사용. GPU 지원·드라이버·로그 확인 |
| 첫 글꼴·재질이 늦게 나타남 | 초기 표시가 안정되는지 기다림. 계속되면 로그·화면 기록 |
| 키보드 반응 없음 | 게임 창 클릭 → USB 보정창 닫기 → 선택 기체·입력 방식 확인 |
| ARM 거부 | 스로틀 최저 → 창 닫기 → 연습/SET 진행 단계 확인 → USB면 유효 보정·ENABLE RADIO 확인 |
| ↑를 놓아도 상승 | 스로틀 유지 방식임. ↓로 THR을 줄임 |
| 스틱을 놓아도 이동 | ANGLE은 수평 복원이며 위치·고도 고정이 아님. 관성·스로틀 확인 |
| 버튼이 안 보임 | F10으로 전체 패널 표시 |
| Boxer가 안 보임 | USB Joystick·데이터 케이블 → `joy.cpl` → REFRESH/NEXT DEVICE |
| 보정 단계 거부 | 양 끝 범위·마지막 유지 방향·안정 시간 확인, 지정한 축만 이동 |
| 저장했는데 키보드 상태 | 최저 스로틀에서 ENABLE RADIO → 창 닫기 → 별도 ARM |
| 다른 앱 클릭 후 USB 중단 | 포커스 상실 DISARM 가능. 게임 복귀 후 장치·스로틀 확인, ENABLE RADIO·ARM 재진행 |
| 새 버전에 보정 없음 | 실행 폴더마다 Saved가 별도. 자기 실제 장치로 확인·보정 |
| SDACS / OFFLINE | 수동 비행 가능. 자동 훈련이 필요할 때만 9절 진행 |
| 통합 실행 파일 없음 | 소스에서 `Build.ps1 -Package`를 먼저 수행 |
| 소스와 실행 파일 불일치 | 지정한 엔진으로 다시 패키징. 검사 생략으로 해결하지 않음 |
| 엔진 경로 오류 | `Engine\Build\BatchFiles\Build.bat` 존재와 경로 설정 확인 |
| cmd가 오류 후 멈춤 | 문구 확인. PowerShell에서 같은 폴더의 명령으로 열면 읽기 쉬움 |
| 화면 끊김 | GRAPHICS → BALANCED, 다른 고부하 작업을 줄이고 확인 |

문제를 기록할 때 **버전·입력 방식·선택 기체·경기 단계·스로틀·상태 메시지**를 함께 적으면 원인 구분에 도움이 됩니다.

<a id="saved-files"></a>
## 11. 설정·로그·CSV·다운로드 확인

### 11-1. 저장 위치

압축 해제 폴더 기준의 일반 위치입니다.

```text
ImperialWingsSimulator/Saved/
├─ Config/
│  ├─ RadioProfile.json       # 자기 조종기 보정
│  └─ VisualQuality.txt       # 화질 설정
├─ Logs/                     # 실행 로그
└─ Reports/                  # F5 / EXPORT CSV
```

에디터의 `Saved`와 `dist/Integrated/Windows/ImperialWingsSimulator/Saved`는 다릅니다. 환경에 따라 경로가 달라지면 실제 생성된 로그 위치를 확인합니다. 배포 ZIP에는 개인 Saved를 넣지 않습니다.

브리지 기록은 **소스 루트**의 `Saved/bridge-integrated.log`·`bridge-integrated-errors.log`입니다. 기존 CSV는 현재 물리 상태 내보내기이며 연속 리플레이나 피드백 자동 전송이 아닙니다. [진단 묶음·데이터 수집 계획](development-roadmap.md#feedback-data-plan)은 후속 개발입니다.

### 11-2. 다운로드 해시 확인 — 선택 사항

다운로드한 파일을 확인할 때 실제 경로로 바꿔 실행합니다.

```powershell
Get-FileHash -Algorithm SHA256 -LiteralPath 'C:\Downloads\ImperialWingsSimulator-Windows-v0.5.0.zip'
```

v0.5.0 Windows ZIP의 SHA-256:

```text
5547ca179ecbf13ab8c7fbdcbe33667e48dd40e29a49968a341422e4f378e655
```

릴리스의 `SHA256SUMS.txt`·`build-provenance.json`에도 배포 해시·소스 정보가 있습니다. 다른 버전은 해당 버전의 해시를 사용합니다.

<a id="verification"></a>
## 12. 검증 범위·남은 기능·관련 문서

### 12-1. v0.5.0 최종 패키지 확인 범위

- Windows C++ 빌드·콘텐츠 처리·패키징과 실제 실행 파일 시작.
- 실제 GPU의 1920×1080 경기장 화면.
- 고정 게임 계산 간격 30/60/120Hz 실제 Chaos 호버·바닥/벽·네트·스트라이커 득점 시나리오 통과.
- 10대 실제 패드·초기화·LOS·스트라이커 중앙·하단 띠 개수 검사 통과.
- Balanced/High/Cinematic 효과 수명·개수 상한 검사 통과.

실기체 정확도 인증, 최신 전체 UI 재검사, 장시간 GPU 측정, 연속 USB 비행을 뜻하지 않습니다. R4/R5/R6 과거 수치는 그때의 결과입니다. [v0.5.0 기록](releases/v0.5.0.md)·[검증 이력](verification.md)을 확인합니다.

### 12-2. 물리·기체 외형

모터별 힘·모멘트, 중력·항력·Chaos 접촉으로 비행합니다. 정상 비행에서 위치를 강제로 갱신하지 않으며 초기화에서는 되돌립니다. ANGLE 목표 Roll/Pitch 한계는 35°이며 Yaw·Throttle은 직접 조절합니다.

비선형 추력·모터 응답·배터리·무게중심·관성·자이로·상대 풍속·하향류·근접 효과·센서 오차·PID·접촉·네트 근사를 다룹니다. 모델별 검증 수준은 [물리 모델 범위](physics-models.md)를 참고합니다. 계수는 실험 가정이며 실측 디지털 트윈이 아닙니다.

HELSEL STRIKER V3를 참고한 자체 절차적 외형을 사용합니다. 제조사 메시·사진·로고를 복제한 공식 제품 모델은 아닙니다. [모델 근거](model-reference.md)·[외부 권리 고지](../THIRD_PARTY_NOTICES.md)를 참고합니다.

### 12-3. 다음 개발

모터 고장·실제 파괴, 띠 탈락 득점 자격, 공격수 승계, 심판·페널티 운영, 연속 USB 검증, 온·오프라인 멀티플레이, 개발 피드백 진단 묶음은 후속 작업입니다.

- [전체 개발 계획·완료 기준](development-roadmap.md)
- [28개 분야·292개 전체 기능 명세](simulator-feature-plan.md)
- [멀티플레이 계획](development-roadmap.md#multiplayer-plan)
- [개발 피드백 데이터 계획](development-roadmap.md#feedback-data-plan)
- [작업 체크리스트](../IMPERIAL_WINGS_CODEX_TASKS.md)
- [경기 영상 참고·확인 한계](match-video-references.md)
- [대진 결과 기반 기획](match-results-development-plan.md)

영상 링크·대진 통계는 모든 경기 영상을 장면별로 복기했다는 뜻이 아닙니다. 각 문서의 확인 범위와 미완료 항목을 참고합니다.

![실제 경기장 렌더 — 과거 R4 참고 화면](images/arena-r4.png)

![실제 기체 렌더 — 과거 R4 참고 화면](images/airframe-r4.png)
