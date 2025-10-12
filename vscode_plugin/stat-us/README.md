# Stat-us

VS Code에서 현재 작업 상태(브랜치, 파일, 작업 시간 등)를 주기적으로 Spring Boot 서버로 전송하는 확장입니다.

전송 정보 예시:

```json
{
	"timestamp": "2025-10-12T17:18:00.123Z",
	"workspaceRoot": "C:/path/to/repo",
	"filePath": "C:/path/to/repo/src/App.java",
	"languageId": "java",
	"branch": "feature/awesome",
	"isIdle": false,
	"idleForMs": 1200,
	"sessionMs": 345678,
	"sessionActiveMs": 300000,
	"keystrokes": 42,
	"vscodeVersion": "1.105.0",
	"extensionVersion": "0.0.1",
	"code": "// (옵션) 전송되는 파일 내용 (길이 제한)",
	"codeLength": 1234
}
```

## 설정

Settings > Extensions > Stat-us에서 다음 옵션을 설정하세요.

- stat-us.enable: VS Code 시작 시 자동으로 전송 시작 (기본 false)
- stat-us.backendUrl: 수신 Spring Boot 엔드포인트 URL (예: <http://localhost:8080/api/ingest/vscode>)
- stat-us.apiKey: x-api-key 헤더 값 (웹 앱의 프로필 > 새 키 발급에서 받은 개인 API 키)
  - 또는 명령 팔레트에서 "Stat-us: API 키 설정"을 실행해 간편히 입력할 수 있습니다.
- stat-us.intervalSeconds: 전송 주기(초) (기본 60)
- stat-us.idleThresholdSeconds: 비활성으로 간주할 시간(초) (기본 60)
- stat-us.sendCode: 활성 파일 코드 전송 여부 (기본 false)
- stat-us.maxCodeLength: 전송할 코드 최대 길이 (기본 10000)

## 사용법

- 명령 팔레트에서 다음 명령을 사용할 수 있습니다.
  - Stat-us: 전송 시작
  - Stat-us: 전송 중지
  - Stat-us: 지금 즉시 전송
  - Stat-us: API 키 설정 (프로필에서 발급한 개인 API 키와 백엔드 URL을 손쉽게 설정)
- 상태 표시줄(좌측)에 전송 상태가 표시됩니다.

## Spring Boot 수신 예시

```java
@RestController
@RequestMapping("/api")
public class StatusController {
  @PostMapping("/status")
  public ResponseEntity<Void> receive(
    @RequestBody Map<String, Object> body,
    @RequestHeader(value = "x-api-key", required = false) String apiKey
  ) {
    // TODO: 인증/검증 및 저장/로깅 처리
    System.out.println(body);
    return ResponseEntity.ok().build();
  }
}
```

## 개인정보/보안 주의

- sendCode를 활성화하면 현재 파일의 내용이 서버로 전송됩니다. 민감한 정보가 포함되지 않도록 주의하세요.
- backendUrl은 신뢰 가능한 내부망/로컬 서버를 권장합니다.
- 필요 시 apiKey 설정으로 간단한 인증을 추가하세요.

## 개발/테스트

```cmd
npm install
npm test
```
