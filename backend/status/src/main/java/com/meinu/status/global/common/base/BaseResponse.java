package com.meinu.status.global.common.base;

import com.fasterxml.jackson.annotation.JsonInclude;

@JsonInclude(JsonInclude.Include.NON_NULL)
public class BaseResponse<T> {
    private final boolean isSuccess;
    private final int code;
    private final String message;
    private final T result;

    private BaseResponse(boolean isSuccess, int code, String message, T result) {
        this.isSuccess = isSuccess;
        this.code = code;
        this.message = message;
        this.result = result;
    }

    public static <T> BaseResponse<T> success(T result) {
        return new BaseResponse<>(true, BaseResponseStatus.SUCCESS.getCode(), BaseResponseStatus.SUCCESS.getMessage(),
                result);
    }

    public static <T> BaseResponse<T> of(BaseResponseStatus status) {
        return new BaseResponse<>(status.isSuccess(), status.getCode(), status.getMessage(), null);
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

    public T getResult() {
        return result;
    }
}
