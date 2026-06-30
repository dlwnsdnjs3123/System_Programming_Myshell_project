# MyShell - Phase 2

## 1. 프로젝트 개요
본 프로젝트는 Linux shell의 기본 동작을 이해하고, 이를 직접 구현해보는 것을 목표로 한다.  
Phase 1에서는 기본적인 명령 실행 구조를 구현하였고, Phase 2에서는 이를 확장하여 pipe(|)를 지원하도록 구현하였다.

쉘은 반복적으로 프롬프트를 출력하고, 사용자로부터 명령어를 입력받은 뒤 이를 파싱하여 실행한다.  
내장 명령어는 쉘 내부에서 직접 처리하고, 일반 명령어는 자식 프로세스를 생성하여 실행한다.

## 2. Phase 1에서 구현한 내용
Phase 2는 Phase 1의 기능을 기반으로 확장되었으므로, 먼저 다음 기능들을 구현하였다.

- 프롬프트 `CSE4100-SP-P2>` 출력
- 사용자 입력을 한 줄 단위로 읽기
- 공백 기준 기본 파싱
- built-in command 처리
  - `cd`
  - `exit`
- 일반 명령어는 `fork()` 후 자식 프로세스에서 `execvp()`로 실행
- 부모 프로세스는 `waitpid()`로 foreground command 종료를 기다림
- `cd` 입력 시 부모 프로세스의 현재 작업 디렉토리를 변경하도록 구현
- `cd` 인자가 없는 경우 HOME 디렉토리로 이동
- `cd ~`, `cd ~/...` 형태 처리

## 3. Phase 2에서 추가한 내용
Phase 2에서는 pipe 기능을 추가하였다.  
이를 위해 기존의 단일 명령 실행 구조를 확장하여, 하나의 명령줄 안에 여러 개의 명령이 포함될 수 있도록 구현하였다.

추가 구현 내용은 다음과 같다.

- `|` 문자를 기준으로 명령줄을 여러 개의 command segment로 분리
- pipeline에 포함된 각 명령마다 새로운 자식 프로세스 생성
- `pipe()` 시스템 콜을 사용하여 명령 간 데이터 전달 경로 생성
- `dup2()`를 사용하여
  - 이전 명령의 출력을 현재 명령의 표준입력으로 연결
  - 현재 명령의 출력을 다음 명령의 표준출력으로 연결
- 부모 프로세스는 마지막 command를 기다리는 방향으로 구현
- 남은 자식 프로세스도 회수하여 zombie process가 생기지 않도록 처리
- `|` 양옆에 공백이 없는 경우도 처리 가능하도록 구현
- quoted argument 지원
  - `"hello world"`와 같은 공백 포함 문자열을 하나의 인자로 처리
  - `grep "a b c"`와 같은 형태도 처리 가능

## 4. 주요 구현 방식

### 4-1. 명령 파싱
입력받은 문자열에서 먼저 newline 문자를 제거한 뒤, pipeline 단위로 분리하였다.  
이 과정에서 따옴표 내부의 `|`는 분리 기준으로 사용하지 않도록 하였다.

각 segment는 다시 argument 단위로 분리하였다.  
이때 공백을 기준으로 나누되, 큰따옴표(`" "`) 또는 작은따옴표(`' '`) 내부의 문자열은 하나의 token으로 처리하였다.

### 4-2. built-in command 처리
다음 명령어는 built-in command로 처리하였다.

- `cd`
- `exit`

단일 명령에서의 `cd`는 부모 프로세스가 직접 처리하도록 하여, 실제 쉘의 현재 디렉토리가 변경되도록 하였다.  
반면 pipeline 내부에 포함된 `cd`는 자식 프로세스 문맥에서 처리되므로 부모 shell의 디렉토리는 바뀌지 않는다.  
이는 Linux shell과 유사한 동작을 따르도록 한 것이다.

### 4-3. pipeline 실행
pipeline이 존재하는 경우, 명령 개수에 따라 필요한 수만큼 pipe를 생성하였다.  
그 뒤 각 command마다 `fork()`를 수행하고, 자식 프로세스에서 `dup2()`를 이용하여 표준입출력을 재설정한 후 명령을 실행하였다.

- 첫 번째 command: stdout을 다음 pipe의 write end에 연결
- 중간 command: stdin은 이전 pipe의 read end, stdout은 다음 pipe의 write end에 연결
- 마지막 command: stdin을 이전 pipe의 read end에 연결

부모 프로세스는 모든 pipe descriptor를 닫은 뒤, 마지막 command를 먼저 기다리도록 구현하였다.

## 5. 테스트한 기능

### 5-1. 기본 pipeline
- `ls | grep myshell`

### 5-2. 공백 없는 pipeline
- `ls|grep myshell`

### 5-3. 다중 pipeline
- `cat p2_test_file | grep -i "abc" | sort -r`

### 5-4. quoted argument
- `echo "hello world"`
- `cat qfile | grep "a b c"`

### 5-5. pipeline 내부 built-in 동작 확인
- `cd .. | cat`

위 테스트들을 통해 pipe, multiple pipes, quoted argument, built-in 관련 동작을 확인하였다.

## 6. 실행 예시

CSE4100-SP-P2> ls | grep myshell
myshell
myshell.c
myshell.h

CSE4100-SP-P2> echo "hello world"
hello world

CSE4100-SP-P2> cat p2_test_file | grep -i "abc" | sort -r
abc
Abc

## 7. 컴파일 방법
make

실행 방법은 다음과 같다.
./myshell

## 8. 파일 구성
- `myshell.c` : 쉘의 메인 로직 구현
- `myshell.h` : 함수 선언 및 상수 정의
- `Makefile` : 컴파일 규칙 정의

## 9. 정리
Phase 2에서는 Phase 1의 기본 쉘 구조를 유지하면서, pipe를 통한 프로세스 간 통신 기능을 추가하였다.  
이를 통해 단일 명령 실행뿐 아니라 여러 명령을 연결하여 실행하는 Linux shell의 핵심 동작을 구현할 수 있었다.  
또한 quoted argument와 공백 없는 pipe 입력도 처리할 수 있도록 하여, 실제 shell과 유사한 동작을 하도록 구성하였다.