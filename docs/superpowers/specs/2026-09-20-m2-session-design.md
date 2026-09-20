# M2 설계 — 별명 입력·세션·무작위 배정·이름표

작성일: 2026-09-20 · 상태: 사용자 승인(대화), 문서 검토 대기

## 목표

macOS 시제품에서 PRD 경험 흐름을 완성한다: 실행 → 산호초 배경 위 별명 입력 → 유효한 별명으로 입장 → 두 종 중 한 마리 무작위 배정 → 내 물고기 위 이름표 → 관람 → 나가기 → 입력 화면. SRS F-01~F-04, F-14를 Unreal 계층에서 구현·검증한다.

## 결정 사항 (2026-09-20 논의)

| 항목 | 결정 | 이유 |
|---|---|---|
| 두 번째 종 | Blender 스크립트로 흰동가리돔(클라운피시) 신규 제작 | 무작위 배정 결과가 한눈에 구별됨, 다양성이 실제 |
| UI 기술 | C++ UMG — `UUserWidget` 서브클래스에서 `WidgetTree`로 위젯 구성 | 에셋 없이 재현·테스트 가능, 한글 IME는 `EditableTextBox`가 처리 |
| 나가기 | Esc 키 + 화면 구석 반투명 작은 버튼 | 키를 모르는 아이도 나갈 수 있음 |
| 한국어 폰트 | Noto Sans KR (SIL OFL 1.1), `assets/fonts/` 원본, 스크립트 임포트 | 무료·재배포 가능, 출처 기록 |
| 원근 (사용자 요청 2026-09-20) | 플레이어 물고기는 카메라 앞 **2.2 m** 평면(가로 2.6 m × 세로 1.3 m, 75° FOV 안), 배경 물고기는 **3.3 m 이상**(M2b에서 3~7 m로 분산). 내 물고기가 앞에 있어 배경 물고기보다 크게 보이고, 가까울수록 커지는 원근이 자연히 적용됨 | 3D 카메라 원근을 그대로 사용; 별도 스케일 조작 없음 |

검토한 대안: UMG 블루프린트 에셋(Python으로 그래프 생성이 불확실, 재현 원칙과 충돌), 순수 Slate(스타일·폰트 연결 번거로움), 같은 메시 색만 다른 2종(구별이 약함).

## 구성 요소

### 규칙 계층 (변경 없음)
`ValidateNickname`(F-01), `PickFishIndex`(F-03), `SessionManager`(F-02, F-14). 새 규칙이 필요하면 순수 테스트를 먼저 추가한다.

### `AAquariumGameMode` (확장)
- 보유: `aquarium::SessionManager`, `TArray<FFishSpecies> Catalog` (`DisplayName`, `USkeletalMesh*`), `FRandomStream Rng`(`Seed` 프로퍼티, 테스트에서 고정). 플레이어 유영 평면 기본값 `PlaneOrigin=(220,0,105)`, `PlaneHalfWidth=130`, `PlaneHalfHeight=65` (배경보다 앞).
- `EBeginSessionResult BeginSession(const FString& RawNickname)`: `SessionManager.Begin(utf8, Catalog.Num(), pick)`에 `pick = [&](size_t n){ return Rng.RandRange(0, n-1); }` 주입 → 성공 시 플레이어 `AFishActor` 스폰(`bIsPlayerFish=true`, 배정 종 메시, 유영 평면은 배경 물고기와 같음) + `UNameTagComponent` 부착. 결과 enum은 규칙 `BeginResult`를 1:1 매핑.
- `void EndSession()`: 플레이어 물고기 제거, `SessionManager.End()`. 배경 물고기는 유지.
- `bool HasActiveSession()`, `FString CurrentNickname()`, `AFishActor* PlayerFish()`.
- 별명은 메모리에만 있다. 로그·저장·전송 금지(`UE_LOG`에 별명을 찍지 않는다).

### `ADiverPlayerController` (확장)
- BeginPlay: 기존 카메라 설정 + `UEntryWidget` 생성·표시, 입력 모드 UI.
- `OnEntrySubmitted(text)` → `GameMode->BeginSession` → 성공이면 입장 위젯 숨김, `UHudWidget` 표시, 입력 모드 게임+UI. 실패면 위젯에 오류 코드 전달.
- Esc(`EKeys::Escape`) 및 HUD 나가기 버튼 → `GameMode->EndSession` → HUD 숨김, 입장 위젯 다시 표시(입력란 비움).

### `UEntryWidget` (C++ UMG)
- 구성: 제목 텍스트, `UEditableTextBox`(힌트 "별명을 입력하세요", 최대 12자는 규칙이 판정), `UButton` 입장, 오류 `UTextBlock`.
- Enter 키(텍스트 상자 `OnTextCommitted` with `ETextCommit::OnEnter`)와 버튼 모두 제출.
- 오류 문구(한국어): Empty → "별명을 입력해 주세요", TooLong → "별명은 12자까지 쓸 수 있어요", InvalidCharacter → "쓸 수 없는 글자가 있어요", EmptyCatalog → "지금은 물고기가 없어요. 잠시 후 다시 시도해 주세요", AlreadyActive → 문구 없음(무시).
- 연타: 제출 처리 중 버튼 비활성화 + 규칙 계층의 AlreadyActive로 이중 방어(F-02).

### `UHudWidget`
- 오른쪽 위 반투명 "나가기" 버튼 하나. 나머지 화면은 비움.

### `UNameTagComponent`
- `UWidgetComponent`(Screen space, 물고기 위 +20 cm)로 별명 `UTextBlock` 표시. 플레이어 물고기에만 부착한다. 폰트는 Noto Sans KR.

### 폰트
- `assets/fonts/NotoSansKR-Regular.otf`(OFL 사본 `OFL.txt` 포함) → `Scripts/import_fonts.py`로 `/Game/UI/FF_NotoSansKR` FontFace 에셋 생성(UE 5.8 Python은 `Font` 합성 불가 → C++ `FUiFont`가 런타임 폰트를 합성). 위젯의 모든 텍스트가 이 폰트를 쓴다. `DefaultGame.ini`의 `DirectoriesToAlwaysCook=/Game/UI`로 쿡에 포함. 패키지에 포함되는지는 M5에서 확인.

### 클라운피시
- `assets/blender/fishlib.py`: 블루탱 스크립트의 몸통·지느러미·리깅·베이크·내보내기 단계를 함수로 추출(파라미터: 몸통 치수, 지느러미 표, 텍스처 노드 빌더, 이름).
- `assets/blender/make_bluetang.py`는 `fishlib`를 써서 **동일 산출물**을 내도록 리팩터(본 이름·치수 불변, FBX 재임포트로 본 목록 검증).
- `assets/blender/make_clownfish.py`: 몸길이 11 cm, 둥근 타원, 주황 몸통에 흰 줄 3개(검은 테두리), 같은 본 규약.
- `Scripts/import_fish.py`: 종 이름을 인자로 받아 `/Game/Fish/<Species>/`에 임포트(블루탱 스크립트를 일반화; 기존 `import_bluetang.py`는 얇은 래퍼 또는 삭제).
- `build_reef_m1.py`: 배경 물고기 2마리(블루탱·클라운피시, 다른 시드)로 갱신. 플레이어 물고기는 세션 시작 시 게임 모드가 스폰.

## 흐름

```
Launch → ReefM1 로드 → 컨트롤러 BeginPlay: 카메라 고정, EntryWidget 표시
Enter/입장 → EntryWidget.OnSubmit(text) → Controller → GameMode.BeginSession(text)
   ├ Ok → 플레이어 물고기 스폰(+이름표) → EntryWidget 숨김, HUD 표시
   └ 오류 → EntryWidget.ShowError(code)
Esc / 나가기 → Controller → GameMode.EndSession() → 플레이어 물고기 제거, 별명 폐기 → HUD 숨김, EntryWidget 표시(빈 입력란)
```

## 검증

| 계층 | 검증 |
|---|---|
| 규칙 | 기존 60개 유지 |
| Unreal Automation | `Aquarium.Session.*`: BeginSession Ok→플레이어 물고기 1 + 이름표 1; 두 번째 Begin은 AlreadyActive이며 액터 수 불변; 무효 별명·빈 카탈로그는 스폰 없음; 같은 Seed면 같은 종 배정; EndSession 후 플레이어 물고기 0·별명 빈 문자열, 재입장 시 새 배정. `Aquarium.UI.*`: EntryWidget에 각 오류 코드 → 기대 한국어 문구; 제출 중 버튼 비활성. 배경 물고기에는 NameTag 없음 |
| 에셋 | 클라운피시 FBX 본 목록 = 블루탱과 동일; 블루탱 리팩터 후 본 목록·치수 불변; 폰트 에셋 로드 |
| 시각 | `-game` 실행 캡처 3장: 입장 화면(한글 힌트·오류 문구), 배정된 물고기 위 한글 이름표, 나가기 후 초기 화면. 실제 한글 IME 타이핑은 사용자가 1회 직접 확인 |

## 데이터·오류 처리

- 별명은 세션 메모리에만. 로그·파일·네트워크 없음.
- 카탈로그가 비었거나 종 메시 로드 실패 → 해당 종을 카탈로그에서 제외, 남은 종으로 배정; 모두 실패면 EmptyCatalog 오류를 입장 화면에 표시(SRS 장애 처리).
- 폰트 로드 실패 → 엔진 기본 폰트로 표시하되 경고 로그(별명 내용은 로그에 남기지 않음).

## 범위 밖

배경 물고기 30~40마리 군집과 산호초·바위(**M2b, 사용자 요청 2026-09-20 — 다음 계획**), 방향키 제어·창 포커스(M3), 클릭 도망(M4), 성능·패키징·폰트 패키지 검증(M5). M2의 배경 물고기 2마리는 M2b의 군집으로 대체된다.
