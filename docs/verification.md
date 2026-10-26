# Imperial Wings 검증 기록

검증일: **2026-10-26 KST**, R4. Windows / UE5.8.3 / Visual Studio2022 MSVC14.44.35229 / RTX5070Ti / RAM32GB. 제공 스타터는 Downloads에 보존했으며 실제 프로젝트는 `E:\ImperialWingsSimulator`다. R2의 과거 결과를 이번 변경의 검증 결과로 대신하지 않는다.

## 현재 빌드·렌더

- C++ 에디터 빌드 성공: `Saved/editor-r4c-build.log`.
- 맵·PBR 재질/카본 직조 질감 생성: `Saved/content-r3-build.log`,0 errors/0 warnings.
- Win64 Development build/cook/stage/package 성공: `Saved/package-r4c-console.log`.
- FIDA16×7×4.5m 경기장,40cm 구형 보호구,LOS 기본 카메라,역할 패드·앞뒤 LED를 실제 패키지에서 렌더했다. [경기장](images/arena-r4.png),[기체](images/airframe-r4.png).
- R4의1920×1080 경기장 장면에서1,323개 게임 프레임 평균11.343ms,95백분위13.266ms,평균88.16FPS. 실제 패키지의 오프스크린 GPU 렌더 기록이며 별도 headless 물리 시험이 함께 실행됐다. 화면 입력 지연이나 RTX5060 성능값이 아니다. 원문은 패키지 `Saved/render-profile.json`.

## 실제 Chaos 물리·네트·골대 시험

`Tests/run-runtime-tests.ps1 -Packaged`는 매번 새 결과 폴더를 만들고 실제 프로세스 종료와 새 JSON 생성·내용을 함께 확인한다. 오래된 파일을 성공으로 재사용하지 않는다. R4 결과 폴더는 `Saved/Verification/runtime-20261026-093559-730`이다.

| 고정 게임 계산 간격 | 마지막 목표 오차 | 네트 모델 최대 처짐 | 보이는 와이어 최대 처짐 | 반동 속도 |
|---|---:|---:|---:|---:|
| 1/30초 | 0.456cm | 5.297cm | 5.021cm | 1.1171m/s |
| 1/60초 | 0.284cm | 6.721cm | 6.437cm | 1.0507m/s |
| 1/120초 | 0.202cm | 7.467cm | 7.404cm | 1.0695m/s |

세 간격 모두 호버·수평 목표 이동,DISARM 낙하·바닥 접촉,25m/s 초기 속도의 보조벽 CCD 접촉,5m/s 네트 충돌·감쇠/반동·20cm 이하 처짐,스트라이커의 실제 골대 통과·정확히1골·오프사이드,10대 유지와 CSV 내보내기를 통과했다. 네트 충돌과 골대 시험의 첫 배치·충돌 초기 속도는 명시적인 fixture다. 이후 이동은 실제 모터 힘/Chaos 접촉이며 시험 중 위치를 따라 옮기지 않는다.

무게중심 오프셋에 루트 스케일0.4가 중복 적용되는 문제와 UE의 `InertiaTensorScale` 의미를 확인해 수정했다. Chaos 기본값에서 꺼져 있던 강체 자이로 결합도 명시적으로 켰다. 회전 중 몸체의 오일러 축 결합과 별도의 회전자 각운동량 토크를 함께 적용한다. 현재 질량0.9kg,전후 균형·중심 아래7.5mm와 계산 관성 대각을 실제 Chaos에 적용하고 시작 시 읽어 로그를 남긴다. 제조사 실측 질량 배분은 아니다. 프레임별 접촉 응답 차이가 남으므로 이 표를 모든 FPS에서 완전히 동일한 물리의 증거로 해석하지 않는다.

## 독립 시험

| 대상 | 결과 |
|---|---|
| 규정 상태·골대 통과·역방향·오프사이드·시간 | MSVC51개 통과 |
| USB 범위·중립·반전·데드존·연결 손실 수학 | MSVC115개 통과 |
| ESC·배터리·항력·근접/후류·센서·PID·접촉·관성 | MSVC83개,실패0 |
| 실제 upstream APF와HTTP·동시 요청·귀환 유도 | Python8개 통과 |

APF40개 동시 요청의 계산 중앙값2.472ms,테스트 전체1.414s. 이 유도 인터페이스의 이상적인 외부 제어기 시험은 Unreal 기체 전체의 비행 검증과 구분한다. 실제 upstream 원본 커밋은 `1e9d826265069696d7e415c7778d17483d7a8580`이며 원본 작업 사본은 수정하지 않았다.

## 화면 조작과 발견한 문제

마우스가 버튼 위에 있어도 클릭이 전달되지 않는 원인을 UE5.8 standalone viewport의 `NoCapture` 입력 경로에서 확인했다. 현재는 누르는 동안만 캡처하고 커서를 보인 채 잠그지 않는다. Canvas 버튼·목록은 하나의 명시적 포인터 경로로 처리해 중복 클릭을 막는다. 역할별 명칭·LED 색상과 목록 선택도 연결했다. F10으로 넓은 LOS 연습 화면을 사용할 수 있다.

R3h 실제 user32 키보드/마우스 검사에서는 **59개 확인 항목**을 통과했다. 0.2m에서4.1991m 수동 상승,스로틀0.5309,높은 스로틀 재ARM 거부,USB 미보정 활성화 거부,패널 뒤 명령 차단,10대 AUTO 평균고도2.34926m,HOLD2초 동안 최대하강2.36cm,RETURN 진입을 확인했다. 귀환 대기 중 다른 앱이 전면에 와 입력을 중단했으며 전체 흐름을 통과했다고 기록하지 않았다. 근거는 `Saved/Verification/ui-20261026-181339-226/result.json`이다. R4의 전체 재검사 결과는 완료 후 아래에 추가한다.

에디터 `-game` 장시간 검사에서 PythonScriptPlugin의 GC 콜백 접근 위반도 한 차례 발생했다. 배포 실행 파일의 검사는 에디터 Python이 포함되지 않는 실제 패키지를 사용한다. 해당 에디터 내부 오류를 프로젝트 물리 성공으로 숨기지 않는다.

## Boxer 실물 상태

Windows PnP의 상위 장치까지 확인했다. `0416:8258`은 **Archon M1 PRO MAX**였고 Boxer가 아니다. 사용자 재연결 후 `1209:4F54`의 실제 USB 제품명이 **Radiomaster Boxer Joystick**으로 확인됐다. WinMM슬롯1,6축·24버튼이며 실제 `joyGetPosEx` 데이터가 수신된다. 진단은 `Tests/radio_probe.py`로 수행하고 원본·장치 경로는 로컬 Saved에만 보관한다.

정지 잡음·초기 패킷 변화와 네 스틱의 전 범위 검증은 다르다. 현재 기록만으로 네 조종 축의 전 범위·방향·중립·최저 스로틀 보정을 완료했다고 주장하지 않는다. 실제 범위·배정이 유효하기 전까지 USB 조종을 자동 활성화하거나 가짜 프로필을 만들지 않는다. [연결·보정 절차](BOXER_SETUP.md)를 참고한다.

## 정밀도와 경기 기능의 한계

보호구는 구체 충돌체·반발/마찰과 제한된 시각 압축을 사용한다. 네트는 국소 스프링·댐퍼와 가우시안 와이어 표시이며 분포 장력/FEM 해석이 아니다. 표면 근접과 다운워시는 경험적 근사다. 회전수 변화의 추가 각운동량 항,정교한 로터 항력/진동,실기체 추력·배터리·관성 측정 보정은 남아 있다. 센서 융합·독립 고정 물리 스레드 PID는 미구현이다. 상세 범위는 [물리 모델](physics-models.md)에 명시했다.

규정 코어는 기본 세트·득점·오프사이드·경기 단계 ARM 제한을 구현한다. 인간 심판의 전체 경고/퇴장,실제 페널티 경기,공격수 승계,VAR,연장·승부치기까지 완성된 공식 대회 운영 시스템은 아니다. 자율 APF는 Practice 훈련 보조이며 공식 경기는 LOS 수동 조종이다.
## R4 전체 네이티브 UI 재검사 완료

실제 패키지 프로세스에서 **96개 항목 전부 통과**, ok=true다. 근거: Saved/Verification/ui-20261026-183035-815/result.json 및 같은 폴더의 실제 화면·CSV. 수동 상승은0.2→4.2258m,스로틀0.5279,10대 AUTO 평균고도2.34279m,HOLD 최대하강2.15cm였다. RETURN은11.82초 안에10대가 홈 근처 바닥으로 돌아와 모두 DISARM·자율 해제됐다. LOS/외부 관찰/경기장 시점1·2·3,마지막IW-10 목록 선택 후 내보내기·초기화·재이륙까지 통과했다. 이 결과는 앞선 입력 간섭으로 중단된 기록과 분리해 보관했다.

검사 뒤에는 전체 초기화·IW-01 선택·LOS 복귀 후 USB 패널에서 실제 Boxer에 대응하는6축·24버튼 장치를 선택했다. Saved/Verification/r4-boxer-panel.png에 실제 연결과 원시 축 표시를 남겼다. 보정 또는USB 활성화를 대신 성공 처리하지 않았으며 모든 기체는 DISARM 상태다.
최종 R4c에서도 강체 자이로 결합을 켠 상태로30/60/120Hz 물리 시험을 다시 통과했다. 네이티브 UI 재검사도 `ok=true`,96항목 전부 통과했다. 최종 근거: `Saved\Verification\ui-20261026-183656-498/result.json`. 소스·패키지·최종 검사 이후 물리 코드를 추가 변경하지 않았다.

지속 입력 기록을 준비한 뒤 사용자가 스틱을 다시 움직였고,X/Y/Z/V 네 아날로그 축에서 실제0–65535 끝값을 모두 관찰했다. 원본은 `Saved/Verification/boxer-final-range.json`과 시각별 실제 입력 journal이다. 물리 스틱별 정확한 채널 배정과 양의 방향·최저 스로틀 확정은 끝값 수집과 구분하며,미확정 프로필을 만들지 않는다. 첫 패킷 중심값을 이동으로 오인하지 않도록 읽기 전용 진단에 시작250ms 제외 구간을 추가했다. 별도의 HID 제품 정보 조회에서도 제품명이 Radiomaster Boxer Joystick으로 일치했다.
최종 설정 패키지 R4d에서는 F5의 엔진 디버그 화면 전환 바인딩을 제거했다. 실제 F5 입력으로 새10행 CSV 생성,LOS1·Boxer연결·10대DISARM을 확인했고 ShaderComplexity 모드로 바뀌지 않았다. `Saved/Verification/r4d-input-check.log`, `r4d-boxer-panel.png`에 근거를 보관했다. 이 변경은 입력 설정과 읽기 전용 진단에 한정하며 R4c의 물리 코드·자산을 바꾸지 않는다.

## R5 Mode 2 안내 보정과 설정창 입력 분리

사용자가 왼쪽 세로 스로틀인 Mode 2를 확인했다. 축 순서를 추측하는 대신 Roll→Pitch→Yaw→Throttle을 한 단계씩 실제로 움직여 식별하는 안내 보정을 추가했다. 매 단계의 준비/수집을 분리하고,안정 구간 중앙값으로 범위와 마지막 유지 방향을 판단한다. 동시 축 이동·중복 축·불충분 범위·불안정 끝값·높은 스로틀·연결/기능 변경은 거부한다. 마지막 중립 확인·검토·저장·USB 활성화는 각각 구분된다. 취소·실패 시 기존 프로필은 유지한다.

- 에디터 빌드 성공: `Saved/editor-r5-radio-build.log`.
- Windows build/cook/stage/package 성공: `Saved/package-r5-radio-console.log`.
- 독립 보정 수학 검사 **159개,실패0**. 이 내부 수치 자료는 실물 보정 파일을 만들지 않는다.
- 새 실제 패키지의 네이티브 키보드/마우스 검사 **119개 전부 통과**,`ok=true`: `Saved/Verification/ui-20261026-195728-781/result.json`.
- 실제 Boxer 드라이버 ID `1209:4F54` 선택·라이브 수집 샘플을 확인했다. 실제 정지 입력으로는 첫 단계가 통과하지 않았고,닫기 후 단계 초기화·10대DISARM·USB 비활성·보정 파일 미변경을 확인했다.
- 설정창이 열린 동안 Enter가 AUTO FLEET을 실행하고 Space가 ARM될 수 있던 경로를 차단했다. 실제 Enter는 첫 단계 수집을 시작했고 Space·위 방향키로 기체가 ARM되거나 키보드 스로틀이 누적되지 않았다.
- 기존 수동 상승·ARM 제한·10대 자율 훈련/HOLD/귀환·착륙·선택·세 카메라·초기화 재검사도 함께 통과했다. 비행 힘 모델·기체/경기장 자산은 R4c에서 변경하지 않았으며,30/60/120Hz 물리 결과는 위 R4 기록으로 구분한다.

검사 후 LOS1·IW-01·10대DISARM·Boxer선택·USB비활성으로 복귀하고 안내 보정 첫 준비 화면을 열어 뒀다. `Saved/Verification/r5-mode2-ready-live-fleet.csv`로 실제 상태를 확인했다. [실행 중 안내 화면](images/radio-mode2-setup.png).

**아직 실물 스틱별 축·방향·중립·최저 스로틀 보정과 USB 수동 비행을 완료하지 않았다.** 현재 검사는 보정 UI·계산·미완료 입력 거부·취소 흐름이며,사용자의 실제 단계별 스틱 움직임을 대신하지 않는다. CSV에는 검사용 `radio_setup_phase`(0대기/1준비/2수집/3중립/4검토/5저장완료/6중단),현재 채널·수집 수·드라이버ID를 추가했다.

## R6 / v0.4.0 — 전 기체 ANGLE·시작 위치·실물 보정

사용자가 전 기체 ANGLE 전환을 요청했다. 수동 Roll/Pitch는 최대35° 목표 기울기의 외부 자세 루프를 거쳐 기존 각속도PID·모터 믹싱으로 전달한다. 대각 스틱도 총 목표 기울기를35°로 제한한다. 중립은 수평 복원,Yaw는 각속도,고도는 수동 스로틀이다. 충돌로 생긴 순간적인 실제 기울기를 강제로 클램프하거나 물리 위치를 옮기지 않는다. 현재 루프는 게임 프레임의 PrePhysics이며 독립 고정 스레드 제어기를 구현했다고 주장하지 않는다.

- 에디터 빌드: `Saved/editor-r6-angle-build.log` 성공.
- Windows build/cook/stage/package: `Saved/package-r6-angle-console.log` 성공.
- 독립 물리96·보정161·규정51·요청 순서16개 검사 통과. 실제 upstream APF/HTTP 브리지10개 검사 통과.
- 실제 패키지의10대 수동 ANGLE 기체에서20/30/60/120/240 고정 게임FPS 모두 초기 기울기 복원·스틱 기울기 응답·해제 후 수평 복원 통과.

| 고정 게임FPS | 스틱 해제1.8초 후10대 중 최대 기울기 |
|---|---:|
| 20 | 1.3384° |
| 30 | 0.0526° |
| 60 | 0.0429° |
| 120 | 0.1243° |
| 240 | 0.1185° |

근거: `Saved/Verification/angle-20261026-115414-501/results.json`. 시험은 처음에만 명시적으로 초기 자세를 배치하고,비USB 시험용 스틱 명령과 수직 보상을 사용해 자세 제어를 분리했다. 이후 운동은 실제 모터/Chaos이며 USB 비행·실기체 성능·렌더링FPS 측정 결과가 아니다.

새 실행 창의 **69개 네이티브 화면 검사 전부 통과**. 실제10대의 초기XY·홈·그려진 패드 인스턴스의 월드XY·역할·중앙선을 향하는 초기 방향,10개 역할 각각의 외부LOS 카메라 위치를 대조했다. 근거: `Saved/Verification/ui-20261026-210203-219/result.json`과 같은 폴더의CSV/역할별 화면. 이 검사는 배치/수동/USB 기본 제한 범위이며 R4의 전체AUTO/HOLD/RETURN96개 검사를 R6에서도 반복했다고 주장하지 않는다. [실제 ANGLE LOS 화면](images/angle-los-r6.png).

실물 조작 과정에서는 한쪽 이동만 수집해도 안내 단계가 넘어가 마지막 중립을 거부할 수 있음을 발견했다. 시작 시 실제 중립을 기억하고 양쪽 범위를 확인한 후 단계 완료를 허용하도록 수정했다. 단발값을 끝값으로 꾸미거나 기준을 낮춰 완료시키지 않았다.

실제 사용자 조작으로 확인된 Roll=X,Pitch=Y,Yaw=V,Throttle=Z,모두NORMAL을 수동 범위 수집에 적용해 **6,454개 실물 입력**의0–1 끝값·중립·최저 스로틀을 검증하고 저장했다. X/Y/V중립은0.499016/0.499504/0.497551,최저Throttle=0이다. 저장된 실제 프로필은 로컬Saved에만 있다. `boxer-after-ManualSave.png`와 실제CSV의 `radio_calibrated=1`로 저장 성공을 확인했다. 이어 USB활성화·최저 스로틀ARM도 `boxer-after-EnableAndArm.png`와CSV로 확인했다. [저장 직후 실제 화면](images/boxer-calibrated-r6.png).

연속USB비행 검사는 포커스 상실로 중단됐다. `boxer-flight-20261026-203625/observation.json`의32개 샘플은 DISARM·키보드 상태이며 비행 성공으로 계산하지 않는다. 케이블 분리·재연결·기체 반응별 방향·USB지연 검증은 남아 있다.

## R6 빌드 출처와 작업 폴더의 추가 변경

화면·CSV·물리 결과와 패키지 옆manifest에 버전·빌드UTC·기반커밋/수정표시·빌드 입력SHA-256을 기록한다. 검증한 입력은62개이며 SHA-256은 **`01fa67e132697ad826c30bec585ca45242e3c9df97be4347689efd63046237c9`**다. C#빌드 계산과 독립 PowerShell 계산이 일치함을 확인했다. 소스 텍스트의CRLF/LF는 정규화하며 자체자산은 바이트 해시를 사용한다. 에디터와 게임manifest는 다른 파일명으로 저장한다.

빌드 이후 작업 폴더에 `ImperialPresentationEffects.{h,cpp}`와 `ImperialGameMode.cpp`의 효과 생성 변경이 추가됐다. 이 추가 작업은 덮어쓰지 않았고 검증한62개 입력과 구분한다. 원래GameMode와 효과 파일 제외 조건으로 재계산한 해시는 위 빌드값과 정확히 일치했다. 화면검사에는 이 명시적인 빌드 스냅샷을 지정했고 `workspace_matches_runtime=false`를 숨기지 않고 기록했다. 새 효과의 성능·시각 품질을 이 검사로 보증하지 않는다.

자율 통신은 세션UUID·명령세대·요청번호를 추가하고,모드/초기화 전환 시 대기 요청을 취소한다. 이전 콜백이 새 요청 대기 상태를 해제하지 못하며,서버도 이전세대/중복 요청을409로 거부한다. 임무·귀환에서2초 동안 유효 응답이 없으면 현 위치HOLD로 전환하고 복구만으로 임무를 재개하지 않는다. API/순서 수학·HTTP 검사와 UE컴파일은 통과했으나 실제UE귀환 도중 장애 검사는 남아 있다.

자동 승인 검토가 기존 브리지 재시작 및 별도 브리지/게임 창 시작 명령을 “blocked by policy”로 거부했다. 해당 명령을 다른 실행 수단으로 우회하지 않았고 사용자가 새 실행 파일을 직접 열었다. 기존브리지의 프로토콜과 맞지 않아 R6관제는SDACS오프라인 상태였으며,수동ANGLE·배치 검사는 브리지 없이 수행했다. 보안창/커서 이동으로 중단된 추가USB준비 검사는 실패 기록으로 별도 보관한다.

사용자가 보안창을 처리하고 입력을 멈춘 뒤 R6 USB준비 검사를 완료했다. **45개 항목 모두 통과**: `Saved/Verification/ui-20261026-211420-879/result.json`. 실제Boxer의 저장 프로필을 불러왔고 최종CSV에서 `input_source=USB,radio_connected=1,radio_calibrated=1,radio_enabled=1,armed=0,flight_mode=ANGLE,throttle=0`을 확인했다. 전체10대는DISARM이다. 이45개를 앞선69개 배치 검사와 중복 없는114개라고 합산하지 않는다.

## 2026-10-26 시각 효과·화질 개선

별도 `ImperialPresentationEffects` 액터와 GameMode 스폰 연결을 추가했다. 바닥 접촉 충격량 기반 먼지, 실제 추력·바닥 추적으로 제한한 저고도 로터 먼지, 유한 인스턴스 풀, 사진식 색 보정·낮은 bloom·넓은 보조광과 Balanced/High/Cinematic을 제공한다. 비행 힘·득점·ARM·USB 코드에는 이 변경을 넣지 않았다. 상세 범위는 [시각 효과 설명](presentation-upgrade.md).

- 최종 에디터 C++ 빌드 성공: `Saved/presentation-editor-build.log`.
- 에디터 효과 수명/풀 검사 3단계 통과: `Saved/Verification/presentation-20261026-115828-013`. 최대96/192/256, 수명 종료 후0개.
- 새 Windows 패키지 build/cook/stage/package/archive 성공: `Saved/presentation-isolated-package-console.log`, ExitCode0.
- 기존 `dist/Windows`가 실행 중이어서 첫 archive는 파일 잠금으로 실패했다. 실행 중인 게임을 종료하지 않고 새 패키지를 `dist/PresentationUpgrade/Windows`로 분리했다. 기존 런처는 이 새 경로로 전환하지 않았다.
- 패키지 효과 수명/풀 검사 3단계 통과: `Saved/Verification/presentation-20261026-120323-271`의 각 새 JSON ok=true.
- 패키지 실제 Chaos60Hz 시나리오 ok=true: `Saved/Verification/presentation-packaged-physics-60.json`. 호버 오차0.284cm, 바닥/벽·네트 접촉 및 실제 스트라이커 득점 통과, 네트 최대 처짐6.721cm.
- 에디터 GPU1920×1080 전후 렌더를 확인했다: `Saved/Verification/presentation-before.png`, `presentation-after.png`.
- 먼지 풀 시험은 명시적 가상 충격으로 시각 수명/상한을 검사한다. 실제 접촉 외형의 정밀도·실기체 먼지·실측 손상 검증을 뜻하지 않는다.
- 실제 USB비행, 전체 화면 조작, 지속 GPU 성능 비교는 이번에 재검사하지 않았다. 기존 30/120Hz·UI 수치를 이번 변경의 새 검증으로 재사용하지 않는다. 모터 고장·보호구 실제 파괴·역할 승계 구현은 별도 작업이다.
- 새 패키지1920×1080 렌더도15초 예열 후 확인했다: `Saved/Verification/presentation-packaged-warm.png`. 초기6초 캡처에는 HUD 일부 글리프가 미완성으로 보였고15초에는 정상 표시됐다. 초기 글꼴/렌더 예열 문제는 추가 확인 대상으로 남기며 전체 UI 시험 통과로 대신하지 않는다.

## 스트라이커 중앙 배치·양쪽 하단 띠 — 2026-10-26

사용자 요청을 적용했다. 양 팀 Striker는 자기 시작선 정중앙(x=±6.5m,y=0)이다. 조종자 기준 왼쪽부터 Libero/Guide/Striker/Sweeper/Keeper,간격1.25m다. 공유 배치 함수로 기체·홈·바닥 패드·LOS 위치를 동기화했다.

양 팀 Striker 초록/Guide 분홍 하단 띠를 좌우 각각2개,기체당4개 추가했다. 기존 역할LED·팀LED·S/G태그와 별개이며 비충돌 메시다. 흔들림은 시각 근사이며 cloth·파손·탈락·득점 자격 연동은 구현하지 않았다.

- 에디터 C++ 빌드 성공: `Saved/role-layout-editor-build.log`.
- 실제10대 패드·초기화·LOS 위치·중앙 Striker·띠 개수 검사 ok=true: `Saved/Verification/center-striker-ribbons.json`.
- 60Hz 실제 물리 시나리오 통과: `Saved/Verification/runtime-20261026-124018-448/results.json`,호버 오차0.341cm,바닥/벽·네트·실제 득점 통과.
- 실제1920×1080 근접 렌더 확인: `Saved/Verification/lower-role-ribbons.png`.
- 별도 Windows build/cook/stage/package/archive 성공: `Saved/role-layout-package-console.log`,ExitCode0.
- 새 패키지의10대 배치·초기화·LOS·띠 검사 통과: `Saved/Verification/role-layout-20261026-124413-578/result.json`.
- 실행 파일은 `dist/RoleLayoutUpgrade/Windows/ImperialWingsSimulator.exe`. 기존 실행 중인 게임을 종료하거나 교체하지 않았다. 실제 USB 비행·전체 UI·새 배치의 자율10대귀환은 이번에 재검사하지 않았다.
- `Tests/imperial-ui-smoke.ps1`의 기존 선형 역할 순서 기대값은 새 중앙 배치로 수정했다. 이 수정 자체를 전체 화면 검사 통과로 기록하지 않는다.

## 2026-10-26 오늘 최종 Windows 통합 빌드

현재 구현을 합친 v0.5.0을 `Scripts/Build.ps1 -Package`로 새로 build/cook/stage/package/archive했다. 미래 계획 전체의 구현 완료를 뜻하지 않는다.

- 실행 파일: `dist/Integrated/Windows/ImperialWingsSimulator.exe`.
- 바탕화면: `C:/Users/User/Desktop/Imperial Wings 2026-10-26.lnk`. 별도 서버를 시작하지 않는 직접 실행 바로가기다.
- 소스와 패키지 빌드 입력SHA-256 일치: `d5f05e26ebc0df6bdfdff5d4206802bfa376264ba9c3f0b4253203179ba31e10`.
- 빌드 ExitCode0 및 bootstrap 실행 ExitCode0,1920×1080 GPU 화면 확인: `Saved/Verification/final-build-20261026-124755-754`의 build-console.log/final-render.log/final-windows.png/final-summary.json.
- 새 패키지30/60/120Hz 실제 물리 검사 전부 통과: `Saved/Verification/runtime-20261026-124923-369/results.json`.
- 새 패키지10대 실제 패드/초기화/LOS/하단 띠 검사 통과: `Saved/Verification/role-layout-20261026-124923-365/result.json`.
- 새 패키지Balanced/High/Cinematic 효과 검사 통과: `Saved/Verification/presentation-20261026-124929-465`.
- 기존 실제RadioProfile을 새 패키지Saved/Config에 복사했다. 기존 파일은 덮어쓰지 않았고 새 보정 데이터나 USB 자동 활성화를 만들지 않았다.
- 자율훈련 브리지 백그라운드 시작 명령은 자동 승인 검토에서 blocked by policy로 거부됐다. 추가 사유는 제공되지 않았으며 다른 명령으로 재시도하지 않았다. 따라서 이번 최종 빌드는 자율 연결이나 실제 USB비행 검증 결과를 포함하지 않는다.
- 모터 고장·실제 파괴·하단 띠 탈락 득점 판정·공격수 승계는 여전히 후속 구현이다. 프로젝트 전체 계획의 완료나 GitHub 최종 반영으로 보고하지 않는다.


## v0.6.0 다른 PC용 포터블 배포·기체 모델 — 2026-10-26

Shipping build/cook/stage/package/archive 성공. 최종 배포 입력SHA-256은 `6ad24fa79cf3e75c6ae2235c51ce0283fc286e5065ad81d76af89f2ab1371a3c`(78개)다. 빌드 당시 부모커밋은85e5f99이며 최신 기체 변경이 적용된 작업 트리에서 컴파일했다. 배포 소스의 입력 지문으로 실행 파일과 내용을 대응시킨다.

- 최종 빌드: `Saved/portable-model-build.log`, ExitCode0.
- 배포 ZIP 실제 압축 해제 후 한글·공백 경로와 분리된 새 사용자 데이터에서15개 검사 전부 통과: `Saved/Verification/portable-final-extracted/result.json`.
- 자식 프로세스의 PATH에서 개발용 Python·Git·Unreal 경로와 관련 환경변수를 제거했다. 네이티브 실행기의 상대 경로, 독립 APF의 실제10대 protocol2 HTTP 응답·원본 소스해시·유한 수명 종료를 확인했다.
- 빈 사용자 데이터에서 시작은키보드/USB비활성, 임의 보정 생성 없음, 진단 파일 저장을 확인했다. 별도 QA 데이터에만 기존 실물Boxer 프로필을 복사하여 실제 장치와의 일치·최저 스로틀 활성화·실제 값 저장/재로딩·검사 종료 시 비활성을 확인했다. 배포 ZIP에 해당 프로필은 없다.
- 새42셀 기체의80접합점·720봉 조각·29체결부·48모터 슬롯·배선/클램프,역할/방향LED 수,무충돌 메시와0.9kg 질량을 실제10대에서 확인했다: `Saved/Verification/role-layout-20261026-133951-145/result.json`.
- 기하학4개 검사 통과: 닫힌 구면 위상,오각12/육각30,외경40cm,최대 이상 구면 개구부131.856cm²,중심 대칭.
- 최종 모델의30/60/120FPS 물리 시나리오 통과: `Saved/Verification/runtime-20261026-133958-374/results.json`. 호버 오차0.522/0.341/0.273cm,바닥/벽·네트 접촉과 실제 득점 확인.
- 최종 모델의20/30/60/120/240FPS ANGLE10대 자세 검사 통과: `Saved/Verification/angle-20261026-134007-196/results.json`. 명시적 비USB 입력/고도 보상 fixture이며 실물 비행 성적이 아니다.
- Shipping 엔진은 개발용 `-FPS` 지정 처리가 제외돼 초기 반복 검사에서 계산 간격이 같았다. 해당 초기 결과를 서로 다른 FPS 검증으로 사용하지 않는다. 명시적 smoke 시나리오에서만 간격을 설정하고 실제 Tick 최소/최대값이요청값과 일치하는지 검사한 위 결과로 대체했다.
- 화질3단계 풀 검사: `Saved/Verification/presentation-20261026-133307-772`. 모터·조종기 코드와 무관한 모델 변경 전 동일 표시효과의 수명/상한 검사다.
- 최종 모델 실제1600×1000 GPU 렌더를 시각 검토: `docs/images/airframe-v06.png`. 첫 사용 조종기 모달도 포터블 Shipping 렌더에서 확인했다(`Saved/Verification/portable-first-use.png`,모델 변경 전 동일 설정UI).
- 실제 APF HTTP10개,번들 원본 검증5개,비행기록 판정5개 수학 검사 통과. 번들 손상·잘못된출처/경로탈출·누락 라이선스를 거부한다.

최종 실행 ZIP `ImperialWings-Windows-v0.6.0-r2.zip`:372,189,778bytes, SHA-256 `0b79c05dda4b7b85ce776a11b383ec917a9df4b39b584e229b8aa2dfbbd71296`.

실제 다른 물리PC·새HID드라이버·미설치VC상태에서의 설치 대화상자·모든GPU에서 실행한 결과는 아니다. 해당 조합과 완전한 USB이륙/착륙·케이블 분리·입력 지연은 추가 현장 검증 대상이다. 재배포 ZIP은 VC x64 설치 파일을 포함하며 Python/Git/Unreal 설치를 요구하지 않는다. 사용자별 보정·화질·진단/비행 기록은 `%LOCALAPPDATA%/ImperialWingsSimulator/Saved`에 저장한다.

앞선 R7(v0.5.0,소스7a20a652)실물 USB기록에서는 이륙 연속7.145초·호버2.336초를 관측했지만 포커스 상실과 이후 AUTO전환으로 착륙/DISARM 연속 검증은 실패했다. 이 기록을 v0.6의 USB비행 완료로 재사용하지 않는다. 파일은 로컬 `Saved/FlightLogs`에만 보관한다.
