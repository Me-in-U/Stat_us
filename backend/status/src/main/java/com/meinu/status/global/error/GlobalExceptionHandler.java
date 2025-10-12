package com.meinu.status.global.error;

import com.meinu.status.global.common.base.BaseException;
import com.meinu.status.global.common.base.BaseResponse;
import com.meinu.status.global.common.base.BaseResponseStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.MethodArgumentNotValidException;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;

@RestControllerAdvice
public class GlobalExceptionHandler {

    @ExceptionHandler(BaseException.class)
    public ResponseEntity<BaseResponse<Void>> handleBaseException(BaseException e) {
        BaseResponseStatus s = e.getStatus();
        return ResponseEntity.status(s.getCode()).body(BaseResponse.of(s));
    }

    @ExceptionHandler(MethodArgumentNotValidException.class)
    public ResponseEntity<BaseResponse<Void>> handleValidation(MethodArgumentNotValidException e) {
        return ResponseEntity.badRequest().body(BaseResponse.of(BaseResponseStatus.INVALID_REQUEST));
    }

    @ExceptionHandler(Exception.class)
    public ResponseEntity<BaseResponse<Void>> handleOther(Exception e) {
        return ResponseEntity.status(500).body(BaseResponse.of(BaseResponseStatus.SERVER_ERROR));
    }
}
