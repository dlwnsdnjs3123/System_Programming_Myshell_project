# System_Programming_Myshell_project

시스템프로그래밍 수업에서 구현한 `MyShell` 프로젝트입니다. 쉘의 기본 명령 실행부터 파이프, 백그라운드 실행, 시그널 처리, 간단한 잡 컨트롤까지 단계적으로 확장하며 구현했습니다.

개인 정보가 포함된 원본 제출 보고서는 공개 저장소에서 제외했습니다.

## 프로젝트 구성

```text
.
|-- phase1
|   |-- myshell.c
|   |-- myshell.h
|   |-- csapp.c
|   |-- csapp.h
|   `-- Makefile
|-- phase2
|   |-- myshell.c
|   |-- myshell.h
|   |-- csapp.c
|   |-- csapp.h
|   `-- Makefile
`-- phase3
    |-- myshell.c
    |-- myshell.h
    |-- csapp.c
    |-- csapp.h
    `-- Makefile
```

## 단계별 구현 내용

### Phase 1: 기본 쉘

- 프롬프트를 반복 출력하고 한 줄 명령을 입력받음
- 공백 기준으로 명령 인자를 파싱함
- `cd`, `exit` 내장 명령 처리
- `fork()`와 `execvp()`를 이용한 외부 명령 실행
- `waitpid()`를 이용한 foreground 프로세스 대기

### Phase 2: 파이프 처리

- `|`를 기준으로 여러 명령을 파이프라인으로 분리
- 각 단계별 자식 프로세스 생성
- `pipe()`와 `dup2()`로 표준 입출력 연결
- `cat file | grep abc | sort` 형태의 다중 파이프 지원
- 따옴표 인자와 공백 없는 파이프 입력 처리

### Phase 3: 백그라운드 실행과 잡 컨트롤

- `&`를 이용한 백그라운드 실행 지원
- job id와 process group id 기반의 잡 테이블 관리
- `jobs`, `bg`, `fg`, `kill` 내장 명령 구현
- 하나의 파이프라인 전체를 하나의 잡으로 제어
- `SIGCHLD`, `SIGINT`, `SIGTSTP` 처리

## 실행 방법

각 phase는 독립적으로 빌드할 수 있습니다.

```bash
cd phase3
make
./myshell
```

정리:

```bash
make clean
```

## 예시 명령

```bash
CSE4100-SP-P2> ls
CSE4100-SP-P2> cd ..
CSE4100-SP-P2> ls | grep myshell
CSE4100-SP-P2> cat file.txt | grep -i "abc" | sort -r
CSE4100-SP-P2> sleep 10 &
CSE4100-SP-P2> jobs
CSE4100-SP-P2> fg %1
```

## 메모

- 각 phase의 `csapp.c`, `csapp.h`는 이 프로젝트에 필요한 최소 래퍼만 포함하도록 정리했습니다.
- Linux/POSIX 환경을 기준으로 작성했으며, 잡 컨트롤 동작은 프로세스 그룹과 시그널 처리에 의존합니다.
