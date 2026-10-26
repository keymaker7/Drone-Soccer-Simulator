# Imperial Wings Drone Soccer Simulator

광주 임페리얼 윙스(Gwangju Imperial Wings)를 위한 Unreal Engine 5 기반
FPV 드론축구 훈련 시뮬레이터 **초기 코드 프로토타입**입니다.

## 진행 현황

- C++ 드론: 4모터 개별 추력, yaw 반작용 토크, Acro 각속도 PID, 항력·중력·충돌
- 입력: 키보드 및 RadioMaster Boxer USB용 구성 가능한 축
- 테스트 경기장: 코드에서 생성하는 바닥·벽·천장·골대 프레임·임시 조명
- **아직 검증되지 않은 항목:** Unreal Editor 실제 빌드, USB 축 연결, 기체 PID 안정성,
  고품질 경기장 재질, 실제 경기 규격, 90/120FPS 측정

## 개발 PC

- Windows / Unreal Engine 5 (실제 설치된 정확한 버전 확인 필요)
- GPU: NVIDIA RTX 5060
- 메모리: 32 GB
- C++ 빌드 도구: Visual Studio 2022, C++ 게임 개발 워크로드 권장

## 최초 실행

1. Windows PC에 이 폴더를 복사한다.
2. `ImperialWingsSimulator.uproject`를 설치된 UE5 버전으로 연다.
3. 안내에 따라 Visual Studio 프로젝트 파일을 생성하고 C++ 코드를 컴파일한다.
4. 새 Empty Level을 생성하고 `L_DroneSoccerArena`라는 이름으로 저장한다.
5. Place Actors에서 `PrototypeArenaBuilder`를 찾아 원점 `(0,0,0)`에 배치한다.
6. `SoccerDronePawn`을 찾아 `(0,0,150)`cm 위치에 배치한다.
7. 드론의 `Auto Possess Player`가 `Player 0`인지 확인한다.
8. 레벨을 저장한 후 Play를 누른다. 첫 비행은 키보드로 시험한다.
9. 컴파일 오류 또는 기체 이상이 있으면 `Window > Developer Tools > Output Log`와
   빌드 로그를 보관해 Codex에 전달한다.

## 키보드 컨트롤

| 기능 | 조작 |
| --- | --- |
| Roll | A / D |
| Pitch | W / S |
| Yaw | Q / E |
| Throttle 조절 | ↑ / ↓ |
| ARM / DISARM | Space |
| 초기 위치 복귀·DISARM | Backspace |

최저 스로틀에서 Space로 ARM한 뒤 천천히 출력을 올린다. 약 27%는
단순 중량/최대추력 계산상 호버링 근처의 **예상값**일 뿐, 실제 비행 테스트로
안정성을 확인해야 한다.

## 물리·그래픽 유의사항

- 현재 모터 추력과 PID 값은 실측 데이터가 아닌 초기값이다.
- 모터 위치와 추력은 SI 단위로 계산한 다음 UE의 cm 단위로 변환한다.
- 드론의 외형은 기본 구체이고 실제 보호가드/프로펠러/관성모멘트는 아직 없다.
- PID 갱신은 초기 구현에서 Actor Tick 기준이다. 이후 Chaos 고정 물리 스텝으로
  옮기고 데이터로 검증해야 '정밀 비행 시뮬레이터'라고 부를 수 있다.
- 고품질 Lumen 조명과 경기장 메시·재질은 다음 마일스톤이다. 현재는
  테스트용 기본 Cube와 Point Light만 자동 생성한다.
- 에디터에서 `stat fps`, `stat unit`, `stat gpu`로 프로파일링한다.

## Boxer 연결

`docs/BOXER_SETUP.md`를 참고한다. RawInput의 축 1~4가 실제
Roll/Pitch/Yaw/Throttle과 일치한다는 보장은 없다.

## 다음 작업

`IMPERIAL_WINGS_CODEX_TASKS.md`에서 완료 상태와 다음 우선순위를 관리한다.
