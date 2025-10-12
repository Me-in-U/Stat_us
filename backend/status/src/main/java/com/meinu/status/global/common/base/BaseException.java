package com.meinu.status.global.common.base;

public class BaseException extends RuntimeException {
    private final BaseResponseStatus status;

    public BaseException(BaseResponseStatus status) {
        super(status.getMessage());
        this.status = status;
    }

    public BaseResponseStatus getStatus() {
        return status;
    }
}
