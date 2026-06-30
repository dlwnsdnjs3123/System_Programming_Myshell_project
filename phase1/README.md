# CSE4100 Project #2 - MyShell (Phase 1)

## 1. 구현 목표
Phase 1에서는 Linux shell의 가장 기본적인 동작을 구현하는 것을 목표로 하였다.  
사용자로부터 명령어를 입력받고, 이를 파싱한 뒤, 내장 명령어(`cd`, `exit`)는 shell 프로세스에서 직접 처리하고,  
그 외 일반 명령어는 `fork()`로 자식 프로세스를 생성한 후 `execvp()`로 실행하도록 구현하였다.

또한 부모 프로세스는 `waitpid()`를 통해 자식 프로세스가 종료될 때까지 기다리도록 구현하였다.

---

## 2. 구현한 기능

### (1) 프롬프트 출력
쉘이 반복적으로 실행되면서 아래 프롬프트를 출력하도록 구현하였다.

CSE4100-SP-P2>

### (2) 명령어 입력 처리
`fgets()`를 이용하여 사용자로부터 한 줄의 명령어를 입력받도록 구현하였다.  
빈 줄이 입력된 경우에는 아무 동작도 하지 않고 다시 프롬프트를 출력하도록 처리하였다.

### (3) 명령어 파싱
입력된 문자열에서 마지막 개행문자(`\n`)를 제거한 뒤, 공백을 기준으로 명령어와 인자를 분리하였다.  
이를 통해 `argv` 형태로 명령어를 저장하고, 이후 built-in command 처리 또는 외부 명령어 실행에 사용하였다.

예를 들어,

`echo hello world`

는 내부적으로 다음과 같이 파싱된다.

- `argv[0] = "echo"`
- `argv[1] = "hello"`
- `argv[2] = "world"`

### (4) built-in command 구현
Phase 1에서는 다음 built-in command를 구현하였다.

#### 1) `cd`
- `cd <directory>` 형태로 입력하면 해당 디렉토리로 이동한다.
- `cd`만 입력하면 환경변수 `HOME` 경로로 이동하도록 구현하였다.
- 추가로 `cd ~`, `cd ~/...` 형태도 가능하도록 `~`를 `HOME` 경로로 확장하는 기능을 넣었다.
- `cd`는 현재 shell process의 working directory를 변경해야 하므로 자식 프로세스가 아니라 부모 프로세스에서 처리하였다.

#### 2) `exit`
- `exit` 입력 시 shell을 종료하도록 구현하였다.

### (5) 외부 명령어 실행
built-in command가 아닌 경우에는 부모 프로세스가 `fork()`를 호출하여 자식 프로세스를 생성하고,  
자식 프로세스는 `execvp()`를 이용하여 명령어를 실행하도록 구현하였다.

예를 들어 다음 명령어들이 실행 가능하다.

- `ls`
- `pwd`
- `mkdir`
- `rmdir`
- `touch`
- `cat`
- `echo`

`execvp()`를 사용하였기 때문에 `PATH` 환경변수에 포함된 실행 파일들은 경로를 직접 적지 않아도 실행할 수 있다.

### (6) 부모 프로세스 대기
부모 프로세스는 `waitpid()`를 통해 자식 프로세스가 종료될 때까지 기다리도록 구현하였다.  
이를 통해 하나의 명령이 끝난 후 다음 명령을 입력받는 기본적인 shell 동작을 수행한다.

### (7) 잘못된 명령어 처리
존재하지 않는 명령어가 입력되었을 경우 shell이 종료되지 않고, 에러 메시지를 출력한 뒤 다시 프롬프트를 띄우도록 구현하였다.

예:
`abcdxyz: Command not found.`

---

## 3. 주요 구현 흐름
전체 실행 흐름은 다음과 같다.

1. 프롬프트 출력
2. 사용자 입력 읽기
3. 입력 문자열 파싱
4. built-in command인지 확인
5. built-in이면 shell process에서 직접 실행
6. built-in이 아니면 `fork()` 수행
7. 자식 프로세스에서 `execvp()`로 명령 실행
8. 부모 프로세스는 `waitpid()`로 자식 종료 대기
9. 다시 프롬프트 출력

---

## 4. 테스트한 기능
다음과 같은 명령어들을 테스트하였다.

### 기본 명령어 실행
- `ls`
- `pwd`
- `mkdir myshell-dir`
- `touch myshell-dir/cse4100`
- `cat p2_test_file`
- `echo hello`

### 디렉토리 이동
- `cd myshell-dir`
- `cd ..`
- `cd`
- `cd ~`
- `cd ~/my-shell/phase1`

### 예외 상황
- 빈 줄 입력
- 잘못된 명령어 입력
- 존재하지 않는 디렉토리로 `cd`

### 종료
- `exit`

---

## 5. Phase 1에서 포함하지 않은 기능
다음 기능들은 Phase 1 범위에 포함되지 않으므로 구현하지 않았다.

- Pipe (`|`)
- Background execution (`&`)
- Job control (`jobs`, `bg`, `fg`, `kill`)
- Signal handler 기반 job control
- Redirection
- 복잡한 quote parsing

즉, 현재 코드는 **Phase 1 범위의 기능만 구현한 버전**이다.

---

## 6. 사용한 시스템 호출 / 함수
구현에 사용한 주요 함수는 다음과 같다.

- `fgets()`
- `fork()`
- `execvp()`
- `waitpid()`
- `chdir()`
- `getenv()`

---

## 7. 실행 방법

컴파일:
`make`

실행:
`./myshell`

---

## 8. 파일 구성
- `myshell.c` : shell 구현 코드
- `myshell.h` : 함수 선언 및 상수 정의
- `Makefile` : 컴파일 설정
- `README.md` : 구현 내용 설명

---

## 9. 참고 사항
Phase 1에서는 shell의 기본 동작에 집중하여 구현하였다.  
따라서 built-in command인 `cd`, `exit`는 부모 프로세스에서 처리하고,  
그 외 일반 명령어는 `fork()`와 `execvp()`를 통해 실행하는 기본 구조를 따랐다.
