package com.meinu.status.global.common.base;

import org.springframework.http.HttpStatus;

public enum BaseResponseStatus {
    SUCCESS(true, HttpStatus.OK.value(), "요청에 성공했습니다."),

    // Common
    INVALID_REQUEST(false, HttpStatus.BAD_REQUEST.value(), "잘못된 요청입니다."),
    UNAUTHORIZED(false, HttpStatus.UNAUTHORIZED.value(), "인증이 필요합니다."),
    FORBIDDEN(false, HttpStatus.FORBIDDEN.value(), "접근이 거부되었습니다."),
    NOT_FOUND(false, HttpStatus.NOT_FOUND.value(), "리소스를 찾을 수 없습니다."),
    CONFLICT(false, HttpStatus.CONFLICT.value(), "충돌이 발생했습니다."),
    SERVER_ERROR(false, HttpStatus.INTERNAL_SERVER_ERROR.value(), "서버 오류가 발생했습니다."),

    // Auth/Member
    EMAIL_ALREADY_EXISTS(false, HttpStatus.CONFLICT.value(), "이미 가입된 이메일입니다."),
    MEMBER_NOT_FOUND(false, HttpStatus.NOT_FOUND.value(), "회원 정보를 찾을 수 없습니다."),
    INVALID_CREDENTIALS(false, HttpStatus.UNAUTHORIZED.value(), "이메일 또는 비밀번호가 올바르지 않습니다."),
    INVALID_TOKEN(false, HttpStatus.UNAUTHORIZED.value(), "유효하지 않은 토큰입니다."),
    TOKEN_EXPIRED(false, HttpStatus.UNAUTHORIZED.value(), "토큰이 만료되었습니다."),
    REFRESH_TOKEN_NOT_FOUND(false, HttpStatus.UNAUTHORIZED.value(), "리프레시 토큰이 없습니다."),
    API_KEY_REQUIRED(false, HttpStatus.UNAUTHORIZED.value(), "API 키가 필요합니다."),
    API_KEY_INVALID(false, HttpStatus.UNAUTHORIZED.value(), "유효하지 않은 API 키입니다."),
    ;

    private final boolean isSuccess;
    private final int code;
    private final String message;

    BaseResponseStatus(boolean isSuccess, int code, String message) {
        this.isSuccess = isSuccess;
        this.code = code;
        this.message = message;
    }

    public boolean isSuccess() {
        return isSuccess;
    }

    public int getCode() {
        return code;
    }

    public String getMessage() {
        return message;
    }
}
