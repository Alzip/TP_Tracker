# TPTracker 일반 게임 지원 수정본

버전: `0.4.44+vanilla.1` — 비공식 호환성 수정본

대상: **Dusklight 2.0.3 / Windows x64 / 일반 게임 세이브**

원본: <https://github.com/mtrevor2/TP_Tracker>

수정본 소스: <https://github.com/Alzip/TP_Tracker>

기준 커밋: `3db24b37f6b9c0c2129ba6d5b2ef31a8478ec212`

## 바뀐 점

원본 트래커는 랜덤마이저 시드가 로드되지 않으면 지역 접근 계산을 시작하지 않습니다.
이 수정본은 일반 게임을 위한 별도 모드를 추가합니다.

- `Normal game (no randomizer)`를 켜면 시드 파일 없이 접근 가능성을 계산합니다.
- 원본 게임의 아이템, 실제 보스 처치, 황혼 해제, 프롤로그 진행 상태를 읽습니다.
- Auru의 메모, 소비된 퀘스트 아이템, 비전 기술, 천공 문자, 지갑, 워프 포털의 원본 저장 방식을 반영합니다.
- 보스 보상과 하트 그릇의 획득 기록을 구분하고, 랜덤마이저에서 다른 용도로 쓰는 일부 플래그의 오판정을 막습니다.
- 상자, NPC 보상, 상점, 황금 벌레, 포우, 비전 기술, 부엉이 석상 목록을 유지합니다.
- 랜덤마이저 전용 힌트 표지판은 숨깁니다. 바닥/숨겨진 루피는 위치와 완료 플래그가 랜덤마이저용으로 재배치되어 있어 이번 일반 게임 모드에서는 숨깁니다.
- 일반 게임 선택 옵션은 재시작 후에도 유지됩니다. 랜덤마이저로 바꾸면 이 옵션을 꺼야 합니다.

이 모드는 진행 상태를 **읽어서 트래커 표시를 바꾸는 기능**입니다.
게임의 스토리 플래그, 아이템, 던전 클리어 여부를 수정하지 않습니다.
트래커 자신의 메모/체크 기록과 설정은 기존 방식으로 저장합니다.

## Windows 파일 받기 — GitHub Actions

GitHub에서 Windows용 파일을 빌드하도록
[normal-game-windows 워크플로](.github/workflows/normal-game-windows.yml)를 준비했습니다.
**사용자 PC에 Git, Python, CMake, Visual Studio를 설치할 필요가 없습니다.**
아래 절차는 [수정본 저장소](https://github.com/Alzip/TP_Tracker)에서 진행합니다.

1. 해당 저장소의 **Actions** 탭을 엽니다.
2. 왼쪽 목록에서 **Normal game - Windows x64**를 선택합니다.
3. **Run workflow**를 누르고, 이 수정본이 들어 있는 브랜치를 선택해 실행합니다.
4. 실행 결과가 성공으로 표시되면 그 실행의 상세 페이지를 엽니다.
5. 아래쪽 **Artifacts**에서 **TPTracker-normal-game-windows-x64**를 내려받습니다.
6. 받은 ZIP 파일을 풀면 설치할 파일이 나옵니다.

   ```text
   TPTracker-0.4.44+vanilla.1-windows-amd64.dusk
   ```

빌드는 GitHub의 Windows 환경에서 소스 컴파일, 회귀 테스트, 리소스 검사,
모드 메타데이터 검증을 수행합니다. 게임 디스크 이미지나 사용자 세이브는 필요하지 않습니다.
결과물에 함께 들어 있는 `BUILD_INFO.json`과 `.sha256` 파일로 대상 Dusklight 버전,
소스 커밋, 패키지 해시를 확인할 수 있습니다. 자세한 실행 결과는 해당 Actions 실행 로그에 남습니다.

각 커밋의 Windows 빌드 결과는 [Actions](https://github.com/Alzip/TP_Tracker/actions)에서 확인할 수 있습니다.
성공한 실행의 결과물을 받아야 하며, 빌드 성공과 실제 게임 플레이 검증은 별도입니다.
개발 환경에서 만든 Linux용 `.dusk`는 Windows에서 사용할 수 없습니다.

## Dusklight 2.0.3에 적용

1. 실행 중인 Dusklight를 종료합니다.
2. 기존 TPTracker `.dusk`를 다른 폴더에 보관하고, **동일한 모드 ID의 파일이 중복으로 남지 않게** 합니다.
3. Windows용으로 빌드한 `.dusk`를 Dusklight의 모드 설치 기능으로 설치하거나 다음 폴더에 넣습니다.

   ```text
   %APPDATA%\TwilitRealm\Dusklight\mods
   ```

   사용자 지정 모드 경로를 지정한 경우에는 그 경로를 사용합니다.

4. Dusklight를 켜고 일반 게임 세이브를 불러옵니다.
5. **F1 → Mods → TPTracker → Normal game (no randomizer)**를 켭니다.
6. 트래커에 `Normal game: using native inventory and story progress` 상태가 표시되는지 확인합니다.
7. 잠시 기다린 다음 같은 체크의 `Area access`와 전체 상태를 확인합니다.

표시 의미:

| 표시 | 뜻 |
| --- | --- |
| OPEN | 추적된 진행 상태와 아이템으로 접근/획득 가능하다고 계산됨 |
| LOCKED | 필요한 진행 또는 아이템 조건이 충족되지 않음 |
| UNKNOWN | 현재 트래커가 접근 조건을 확정할 정보가 부족함 |
| DONE | 획득 기록을 인식함 |

`Area access`는 그 위치에 도달할 수 있는지, `Local requirement`는 그 위치에서 추가로 필요한 조건입니다.
지역이 OPEN이어도 해당 상자에 필요한 도구가 없으면 체크 자체는 LOCKED일 수 있습니다.

## 검증 범위와 남은 확인

소스의 일반 게임 분기, 초기 진행 제한, 아이템 획득 후 상태 변화, 일반/랜덤마이저 모드 전환,
원본 슬롯/플래그 어댑터, 보스 하트/보상 분리 등을 자동 테스트합니다.
카카리코에 대해서는 **게임 진행 상태를 코드로 구성한 모의 입력**으로 다음을 확인합니다.

- 엘딘 황혼이 남아 있는 첫 방문에서 늑대 형태의 지역 접근과 빛의 벌레 경로를 계산하는지
- 엘딘 황혼을 해제하면 인간 형태의 카카리코와 말로 마트 지역 접근이 열리는지
- 반즈의 폭탄 가방은 원본 게임의 고론 광산 완료 이벤트 이후에만 구매 가능으로 표시하는지
- `Iron Boots` 등 필요한 도구가 없을 때 데스마운틴과 개별 상자의 조건을 건너뛰지 않는지
- 현재 지역 이름만 카카리코로 바꿨다고 앞선 스토리 조건까지 자동으로 충족시키지 않는지

이 테스트는 Dusklight를 실행하거나 실제 사용자 세이브를 읽는 방식이 아닙니다.
Windows 빌드와 자동 테스트 결과는 Actions 실행 로그와 다운로드한 `BUILD_INFO.json`에서 확인합니다.

기본 경로 자료가 랜덤마이저에서 유래하므로 모든 스토리 이벤트와 변형 세이브까지 검증한 완전한 일반 게임 공략 엔진은 아닙니다.
이미 획득한 일부 이벤트 보상은 과거 획득 기록을 완전히 복원하지 못할 수 있습니다.
오류가 남으면 **Dusklight 버전, 진행 중인 지역/체크 이름, 현재 주요 아이템,
문제 화면**이 다음 수정의 가장 유용한 근거입니다. 게임 전체 파일은 필요하지 않습니다.

## 한국어화 가능 여부

**메뉴, 체크 이름, 획득 방법 안내의 한국어화는 가능합니다.** 현재 수정본은 일반 게임 호환성에
집중한 버전이며 표시 문구는 아직 영어입니다. 한국어화할 때는 화면에 보여 주는 번역을 별도로 두고,
접근 조건 계산과 체크 기록에 쓰는 영어 식별자는 유지해야 합니다.
공략 자료인 `res/check_guides.json`은 658개 항목, 1,080개 문단으로 구성되어 있습니다.

기본 UI 글꼴에는 한글 글리프가 없지만, **Dusklight 2.0.3의 스타일 기능으로 모드에
한글 글꼴을 추가하는 경로를 확인했습니다.** 모드에 포함한 TTF를 모드 자신의 쓰기 가능한
폴더에 풀고, 기존 UI 스타일 API의 `@font-face`로 불러오는 방식입니다.
소스 검토상 Dusklight 본체 재빌드는 필요하지 않습니다. 실제 표시 성공은 별도 검증이 필요하므로,
`현재 지역`, `지역 접근`, 카카리코 공략 몇 개로 글자 표시·줄바꿈·검색·스크롤을 먼저 시험합니다.
게임 지도 위 글자는 별도 렌더러를 사용하므로 따로 확인해야 합니다.

근거: Dusklight 2.0.3의
[UI 글꼴 로딩 코드](https://github.com/TwilitRealm/dusklight/blob/v2.0.3/src/dusk/ui/ui.cpp),
[공개 UI API](https://github.com/TwilitRealm/dusklight/blob/v2.0.3/sdk/include/mods/svc/ui.h),
해당 버전에 포함된 [RmlUi 글꼴 스타일 처리 코드](https://github.com/encounter/RmlUi/blob/f00a0fee38391b2f927114e11bea18dc0a7dba1e/Source/Core/StyleSheetParser.cpp).

## 선택 사항: Windows에서 직접 빌드

GitHub Actions 대신 PC에서 직접 빌드할 개발자를 위한 방법입니다. 다음 도구가 필요합니다.

- Git
- Python 3.10 이상
- CMake 3.26 이상 (`cmake`, `ctest`)
- Visual Studio 2022의 **C++를 사용한 데스크톱 개발** 구성 요소와 Windows SDK
- Visual Studio 설치 관리자의 **Windows용 C++ Clang 컴파일러**와 **MSBuild용 LLVM(Clang-cl) 도구 집합** 구성 요소

소스 폴더에서 PowerShell로 실행합니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build_windows_vanilla.ps1
```

이 실행 정책 옵션은 해당 PowerShell 프로세스에만 적용됩니다.
스크립트는 저장소에 지정된 SDK와 공개 빌드 의존성을 내려받습니다.
최초 실행에는 인터넷 연결이 필요하며, 게임 디스크 이미지는 필요하지 않습니다.
성공 시 파일은 `dist/TPTracker-0.4.44+vanilla.1-windows-amd64.dusk`에 생성됩니다.
다른 Visual Studio 버전을 쓰면 CMake가 지원하는 생성기 이름을 `-Generator`로 지정할 수 있습니다.
기본 도구 집합은 `ClangCL`입니다. 이 SDK의 모드 식별 정보가 최적화 중 사라지지 않도록
Clang으로 빌드하며, 최종 패키지의 식별 정보 검증도 반드시 통과해야 합니다.

## 직접 코드 검토 / 적용

수정된 전체 소스와 리소스는 [수정본 저장소](https://github.com/Alzip/TP_Tracker)에서 확인합니다.
개발자는 **Code → Download ZIP**으로 받거나 다음 명령으로 복제할 수 있습니다.

```powershell
git clone https://github.com/Alzip/TP_Tracker.git
```

수정본은 개인 포크에서 관리하며 업스트림 원본에 병합된 공식 릴리스가 아닙니다.
원본으로 돌아가려면 이 수정본 모드 파일을 빼고 보관해 둔 원래 `.dusk`를 복원합니다.
게임 자체의 진행 상태는 이 패치로 변경되지 않습니다.
