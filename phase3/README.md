# CSE4100 Project #2 - MyShell (Phase 3)

## 1. 개요
Phase 3에서는 Phase 1, Phase 2에서 구현한 기본 쉘 기능과 pipeline 기능을 바탕으로, **background process 실행 및 job control 기능**을 추가하였다.  
사용자가 명령어 끝에 `&`를 입력하면 해당 작업이 background에서 실행되도록 하였고, shell은 이를 기다리지 않고 즉시 다음 명령을 입력받을 수 있도록 구현하였다.

또한 background 또는 stopped 상태의 작업을 관리할 수 있도록 `jobs`, `bg`, `fg`, `kill` 명령어를 built-in command로 추가하였다.  
이와 함께 `SIGCHLD`, `SIGINT`, `SIGTSTP` signal handler를 등록하여 foreground job에 대한 `Ctrl-C`, `Ctrl-Z` 입력을 처리하고, 자식 프로세스의 상태 변화도 반영할 수 있도록 하였다.

즉, Phase 3의 목표는 **기존 shell 기능 위에 Linux shell과 유사한 job control 기능을 추가하는 것**이다.

---

## 2. Phase 1, 2 대비 추가된 내용

### 2.1 Background 실행 지원
Phase 1과 Phase 2에서는 foreground command만 실행하였기 때문에, 부모 shell은 항상 자식 프로세스가 끝날 때까지 기다렸다.  
하지만 Phase 3에서는 명령줄 끝에 `&`가 붙은 경우 해당 작업을 background job으로 실행하도록 구현하였다.

예를 들어 다음과 같은 명령을 처리할 수 있다.

- `sleep 10 &`
- `ls -al | grep myshell &`
- `cat p2_test_file | grep -i "abc" &`

이 경우 shell은 background job을 실행한 뒤 즉시 프롬프트를 다시 출력한다.

또한 `sort foo.txt &`, `sort foo.txt&`와 같이 `&` 앞에 공백이 없는 경우도 처리할 수 있도록 구현하였다. 

---

### 2.2 Job Table 추가
Background job과 stopped job을 관리하기 위해 **job table**을 추가하였다.  
각 job은 하나의 process group 단위로 관리하며, 다음과 같은 정보를 저장한다.

- job id
- process group id (pgid)
- 현재 job 상태
- 포함된 process id 목록
- 실행 명령줄
- background 여부

이를 통해 shell은 현재 실행 중인 background job과 stopped job을 추적할 수 있으며, 이후 `jobs`, `bg`, `fg`, `kill` 명령으로 해당 job을 제어할 수 있다.

---

### 2.3 Job Control Built-in Command 구현
Phase 3에서는 다음 명령어를 built-in command로 추가하였다.

#### (1) `jobs`
현재 job table에 저장된 job 목록을 출력한다.  
출력 시 각 job의 job id, 상태, command line을 Linux shell과 유사한 형식으로 보여주도록 하였다.

#### (2) `bg <job>`
중단된 job을 다시 background에서 실행하도록 한다.  
필요한 경우 `SIGCONT`를 전달하여 stopped 상태의 job을 재개한다.

#### (3) `fg <job>`
background 또는 stopped 상태의 job을 foreground로 가져와 실행한다.  
foreground로 전환된 job은 다시 shell이 기다리도록 처리하였다.

#### (4) `kill <job>`
지정한 job을 종료한다.  
job 단위로 signal을 전달하기 위해 process group 전체에 대해 signal을 보내도록 구현하였다.

이때 operand는 `%1`과 같은 **job id 형식**을 기준으로 처리하도록 구현하였다.  
존재하지 않는 job id가 들어오는 경우에도 shell이 비정상 종료되지 않고 오류 메시지만 출력한 뒤 계속 동작하도록 하였다.  

---

## 3. 주요 구현 방식

### 3.1 Background 여부 파싱
명령줄 전체를 파싱하기 전에 먼저 입력 문자열 끝부분에 `&`가 존재하는지 검사하였다.  
이때 따옴표 내부에 있는 `&`는 background 기호로 처리하지 않도록 하였고, `&` 뒤에 공백만 있는 경우에만 background 실행으로 인정하였다.

background 실행으로 판단되면 해당 `&`를 제거한 뒤 기존 pipeline parsing 과정을 수행하도록 구현하였다. 

---

### 3.2 Process Group 단위 관리
Phase 3에서는 pipeline 전체를 하나의 job으로 관리하기 위해, job에 속한 모든 child process를 **같은 process group**으로 묶도록 구현하였다.

이를 통해 다음과 같은 동작이 가능해진다.

- 하나의 pipeline 전체를 하나의 job처럼 관리
- foreground pipeline 전체에 `Ctrl-C`, `Ctrl-Z` 전달
- `bg`, `fg`, `kill` 명령을 job 단위로 적용

예를 들어 `cat file | grep abc | sort &`와 같은 pipeline도 하나의 background job으로 다룰 수 있다. 

---

### 3.3 Signal Handling 추가
Phase 1에서는 별도의 signal handler가 필수는 아니었지만, Phase 3에서는 job control을 위해 signal handler가 필요하므로 다음을 추가하였다.

#### `SIGCHLD`
자식 프로세스의 종료, 중단, 재개 상태를 감지하여 job table을 갱신한다.  
종료된 자식은 `waitpid()`로 회수하여 zombie process가 남지 않도록 하였고, 모든 프로세스가 종료된 경우 해당 job의 상태를 `Done` 또는 `Terminated`로 변경하였다.

#### `SIGINT`
사용자가 `Ctrl-C`를 입력하면 현재 foreground job의 process group 전체에 `SIGINT`를 전달하도록 구현하였다.  
이를 통해 foreground command 또는 foreground pipeline 전체가 종료될 수 있도록 하였다.

#### `SIGTSTP`
사용자가 `Ctrl-Z`를 입력하면 현재 foreground job의 process group 전체에 `SIGTSTP`를 전달하도록 구현하였다.  
중단된 job은 이후 `jobs`에서 확인할 수 있고, `bg` 또는 `fg`로 다시 제어할 수 있도록 하였다. 

---

### 3.4 Foreground / Background 처리 분리
Foreground job의 경우 부모 shell은 해당 job이 끝나거나 stopped 상태가 될 때까지 기다리도록 구현하였다.  
반면 background job의 경우 부모 shell은 기다리지 않고 즉시 프롬프트를 출력하도록 구현하였다.

또한 background job이 종료되거나 stopped 상태로 바뀌었을 때는 다음 프롬프트를 출력하기 전에 상태를 알려줄 수 있도록 notification 기능을 추가하였다. 
---

## 4. Phase 3에서 지원하는 기능

다음과 같은 기능을 지원한다.

- 일반 foreground command 실행
- pipeline command 실행
- background command 실행
- background pipeline 실행
- `jobs`로 job 목록 확인
- `bg`로 stopped job 재개
- `fg`로 job을 foreground로 전환
- `kill`로 job 종료
- foreground job에 대한 `Ctrl-C`, `Ctrl-Z` 처리
- 존재하지 않는 job id에 대한 예외 처리

---

## 5. 테스트 예시

다음과 같은 명령어들을 통해 Phase 3 기능을 확인할 수 있다.

### 5.1 Background 실행
- `sleep 10 &`
- `sleep 5&`

### 5.2 Background pipeline
- `ls -al | grep myshell &`
- `cat p2_test_file | grep -i "abc" &`

### 5.3 Job 목록 확인
- `jobs`

### 5.4 Foreground / Background 전환
- `bg %1`
- `fg %1`

### 5.5 Job 종료
- `kill %1`

### 5.6 Signal 처리
- foreground command 실행 후 `Ctrl-C`
- foreground command 실행 후 `Ctrl-Z`

---

## 6. 구현 요약
Phase 3에서는 Phase 1의 기본 shell 실행 구조와 Phase 2의 pipeline 구조를 유지하면서,  
그 위에 background 실행과 job control 기능을 추가하였다.

이를 위해 다음 내용을 구현하였다.

- `&`를 이용한 background 실행
- job table 기반 job 관리
- `jobs`, `bg`, `fg`, `kill` built-in command
- process group 단위 job 제어
- `SIGCHLD`, `SIGINT`, `SIGTSTP` signal handling
- foreground / background 실행 흐름 분리
- background job 상태 알림 및 cleanup 처리

결과적으로 본 Phase 3 구현은 Linux shell과 유사한 방식으로 background process와 job control을 지원하는 MyShell을 목표로 하였다.